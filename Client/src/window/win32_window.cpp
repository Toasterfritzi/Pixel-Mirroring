#ifdef _WIN32
#include "win32_window.h"
#include <windowsx.h>
#include <algorithm>
#include <iostream>

namespace pm::window {

namespace {
    const int PHONE_CORNER_RADIUS = 24;
    const int BUBBLE_CORNER_RADIUS = 18;
    const int BUBBLE_W = 155;
    const int BUBBLE_H = 36;
    const int BUBBLE_GAP = 6;
    const int MIN_PHONE_W = 140;

    const wchar_t* ICON_DRAG = L"\uE700";
    const wchar_t* ICON_MINIMIZE = L"\uE921";
    const wchar_t* ICON_MAXIMIZE = L"\uE922";
    const wchar_t* ICON_RESTORE = L"\uE923";
    const wchar_t* ICON_CLOSE = L"\uE8BB";
}

// Factory implementation
std::unique_ptr<IWindow> create_window(int width, int height, const std::string& title) {
    return std::make_unique<Win32Window>(width, height, title);
}

Win32Window::Win32Window(int width, int height, const std::string& title)
    : width_(width), height_(height), title_(title) {
}

Win32Window::~Win32Window() {
    if (icon_font_) DeleteObject(icon_font_);
    if (hwnd_) DestroyWindow(hwnd_);
}

bool Win32Window::create() {
    HINSTANCE hinstance = GetModuleHandle(nullptr);
    
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = Win32Window::window_proc;
    wc.hInstance = hinstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "PixelMirroringWindowClass";

    RegisterClassExA(&wc);

    // WS_POPUP removes the standard titlebar entirely, WS_THICKFRAME allows resizing, WS_MINIMIZEBOX allows minimization
    DWORD dwStyle = WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX;

    // Calculate total height including bubble
    int total_w = width_;
    int total_h = height_ + BUBBLE_H + BUBBLE_GAP;

    hwnd_ = CreateWindowExA(
        0, wc.lpszClassName, title_.c_str(), dwStyle,
        CW_USEDEFAULT, CW_USEDEFAULT, total_w, total_h,
        nullptr, nullptr, hinstance, this
    );

    if (!hwnd_) return false;

    // Load Segoe MDL2 Assets font for native icons
    icon_font_ = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, 
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");

    recalc_layout();
    update_region();
    
    // Set a timer to force repaint at ~60 FPS so SDL can render
    SetTimer(hwnd_, 1, 16, nullptr);

    return true;
}

void Win32Window::show() {
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
}

void Win32Window::process_messages() {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void Win32Window::set_aspect_ratio(double ratio) {
    aspect_ratio_ = ratio;
}

void Win32Window::set_orientation(bool landscape) {
    is_landscape_ = landscape;
    is_max_height_ = false;
    
    // Recalculate dimensions for new orientation if we want automatic resize, 
    // but for now just updating the flag is enough for the Aspect Ratio lock.
}

HRGN Win32Window::create_rounded_rect_rgn(const RECT& r, int radius) {
    if (r.right <= r.left || r.bottom <= r.top) return CreateRectRgn(0,0,0,0);
    return CreateRoundRectRgn(r.left, r.top, r.right + 1, r.bottom + 1, radius * 2, radius * 2);
}

void Win32Window::recalc_layout() {
    if (!hwnd_) return;
    
    RECT client_rect;
    GetClientRect(hwnd_, &client_rect);
    int w = client_rect.right - client_rect.left;
    int h = client_rect.bottom - client_rect.top;
    
    int phone_top = BUBBLE_H + BUBBLE_GAP;
    rect_phone_ = {0, phone_top, w, h};
    
    int bw = std::min(BUBBLE_W, w);
    int bubble_x = std::max(0, w - bw);
    rect_bubble_ = {bubble_x, 0, bubble_x + bw, BUBBLE_H};
    
    int btn_w = bw / 4;
    int remainder = bw - (btn_w * 4);
    
    rect_drag_  = {rect_bubble_.left, 0, rect_bubble_.left + btn_w + remainder, BUBBLE_H};
    rect_min_   = {rect_drag_.right, 0, rect_drag_.right + btn_w, BUBBLE_H};
    rect_max_   = {rect_min_.right, 0, rect_min_.right + btn_w, BUBBLE_H};
    rect_close_ = {rect_max_.right, 0, rect_max_.right + btn_w, BUBBLE_H};
}

void Win32Window::update_region() {
    if (!hwnd_) return;
    
    HRGN bubble_rgn = create_rounded_rect_rgn(rect_bubble_, BUBBLE_CORNER_RADIUS);
    HRGN phone_rgn = create_rounded_rect_rgn(rect_phone_, PHONE_CORNER_RADIUS);
    
    HRGN combined = CreateRectRgn(0, 0, 0, 0);
    CombineRgn(combined, bubble_rgn, phone_rgn, RGN_OR);
    
    SetWindowRgn(hwnd_, combined, TRUE);
    
    DeleteObject(bubble_rgn);
    DeleteObject(phone_rgn);
}

int Win32Window::hit_test_button(POINT pt) {
    if (PtInRect(&rect_close_, pt)) return 3;
    if (PtInRect(&rect_max_, pt))   return 2;
    if (PtInRect(&rect_min_, pt))   return 1;
    if (PtInRect(&rect_drag_, pt))  return 0;
    return -1;
}

void Win32Window::toggle_max_height() {
    if (is_max_height_) {
        is_max_height_ = false;
        SetWindowPos(hwnd_, nullptr, restore_bounds_.left, restore_bounds_.top,
                     restore_bounds_.right - restore_bounds_.left,
                     restore_bounds_.bottom - restore_bounds_.top, SWP_NOZORDER);
    } else {
        GetWindowRect(hwnd_, &restore_bounds_);
        is_max_height_ = true;

        HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi = { sizeof(mi) };
        GetMonitorInfo(monitor, &mi);
        RECT work_area = mi.rcWork;

        if (is_landscape_) {
            SetWindowPos(hwnd_, nullptr, work_area.left, work_area.top,
                         work_area.right - work_area.left,
                         work_area.bottom - work_area.top, SWP_NOZORDER);
        } else {
            int total_h = work_area.bottom - work_area.top;
            int phone_h = total_h - BUBBLE_H - BUBBLE_GAP;
            double ratio = aspect_ratio_ > 0 ? aspect_ratio_ : (9.0 / 16.0);
            int phone_w = static_cast<int>(phone_h * ratio);
            int x = work_area.left + (work_area.right - work_area.left - phone_w) / 2;
            SetWindowPos(hwnd_, nullptr, x, work_area.top, phone_w, total_h, SWP_NOZORDER);
        }
    }
}

LRESULT CALLBACK Win32Window::window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    Win32Window* window = nullptr;

    if (msg == WM_NCCREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lparam);
        window = reinterpret_cast<Win32Window*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        window->hwnd_ = hwnd;
    } else {
        window = reinterpret_cast<Win32Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (window) {
        return window->handle_message(msg, wparam, lparam);
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

void Win32Window::handle_paint() {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd_, &ps);
    
    // Create double buffer to prevent flickering
    RECT client_rect;
    GetClientRect(hwnd_, &client_rect);
    int w = client_rect.right - client_rect.left;
    int h = client_rect.bottom - client_rect.top;
    
    HDC mem_dc = CreateCompatibleDC(hdc);
    HBITMAP mem_bitmap = CreateCompatibleBitmap(hdc, w, h);
    HBITMAP old_bitmap = (HBITMAP)SelectObject(mem_dc, mem_bitmap);
    
    // Clear background
    HBRUSH bg_brush = CreateSolidBrush(RGB(30, 30, 30));
    FillRect(mem_dc, &client_rect, bg_brush);
    DeleteObject(bg_brush);
    
    // Draw Phone
    HRGN phone_rgn = create_rounded_rect_rgn(rect_phone_, PHONE_CORNER_RADIUS);
    HBRUSH phone_brush = CreateSolidBrush(RGB(22, 22, 22));
    FillRgn(mem_dc, phone_rgn, phone_brush);
    DeleteObject(phone_brush);
    DeleteObject(phone_rgn);
    
    // Draw Bubble
    HRGN bubble_rgn = create_rounded_rect_rgn(rect_bubble_, BUBBLE_CORNER_RADIUS);
    HBRUSH bubble_brush = CreateSolidBrush(RGB(50, 50, 50));
    FillRgn(mem_dc, bubble_rgn, bubble_brush);
    DeleteObject(bubble_brush);
    DeleteObject(bubble_rgn);
    
    // Draw Hover Effects
    if (hovered_button_ != -1) {
        RECT hr = {0};
        COLORREF hover_color = RGB(75, 75, 75);
        if (hovered_button_ == 0) hr = rect_drag_;
        if (hovered_button_ == 1) hr = rect_min_;
        if (hovered_button_ == 2) hr = rect_max_;
        if (hovered_button_ == 3) { hr = rect_close_; hover_color = RGB(196, 43, 28); }
        
        HBRUSH hover_brush = CreateSolidBrush(hover_color);
        
        // Clip hover effect to bubble shape
        HRGN btn_rgn = CreateRectRgn(hr.left, hr.top, hr.right, hr.bottom);
        HRGN bubble_clip = create_rounded_rect_rgn(rect_bubble_, BUBBLE_CORNER_RADIUS);
        HRGN final_clip = CreateRectRgn(0,0,0,0);
        CombineRgn(final_clip, btn_rgn, bubble_clip, RGN_AND);
        
        FillRgn(mem_dc, final_clip, hover_brush);
        
        DeleteObject(hover_brush);
        DeleteObject(btn_rgn);
        DeleteObject(bubble_clip);
        DeleteObject(final_clip);
    }
    
    // Draw Text/Icons
    SetBkMode(mem_dc, TRANSPARENT);
    HFONT old_font = (HFONT)SelectObject(mem_dc, icon_font_);
    
    auto draw_icon = [&](const wchar_t* icon, const RECT& r, bool is_close) {
        SetTextColor(mem_dc, (is_close && hovered_button_ == 3) ? RGB(255, 255, 255) : RGB(220, 220, 220));
        DrawTextW(mem_dc, icon, -1, (RECT*)&r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    };
    
    draw_icon(ICON_DRAG, rect_drag_, false);
    draw_icon(ICON_MINIMIZE, rect_min_, false);
    draw_icon(is_max_height_ ? ICON_RESTORE : ICON_MAXIMIZE, rect_max_, false);
    draw_icon(ICON_CLOSE, rect_close_, true);
    
    SelectObject(mem_dc, old_font);

    // Placeholder text in phone screen
    if (render_cb_) {
        // We let the callback (SDL2) draw the phone screen area
        render_cb_();
    } else {
        SetTextColor(mem_dc, RGB(160, 160, 160));
        RECT text_rect = rect_phone_;
        DrawTextA(mem_dc, "Waiting for Video Stream...", -1, &text_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    
    // Copy to screen
    BitBlt(hdc, 0, 0, w, h, mem_dc, 0, 0, SRCCOPY);
    
    SelectObject(mem_dc, old_bitmap);
    DeleteObject(mem_bitmap);
    DeleteDC(mem_dc);
    
    EndPaint(hwnd_, &ps);
}

LRESULT Win32Window::handle_message(UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_TIMER:
            if (wparam == 1) {
                InvalidateRect(hwnd_, &rect_phone_, FALSE);
            }
            return 0;
            
        case WM_SIZE:
            if (wparam != SIZE_MINIMIZED) {
                recalc_layout();
                update_region();
            }
            return 0;

        case WM_SIZING:
            handle_sizing(wparam, lparam);
            return TRUE;

        case WM_NCHITTEST:
            return handle_nchittest({GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});

        case WM_LBUTTONDOWN: {
            POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            int btn = hit_test_button(pt);
            if (btn == 1) {
                ShowWindow(hwnd_, SW_MINIMIZE);
            } else if (btn == 2) {
                toggle_max_height();
            } else if (btn == 3) {
                PostMessage(hwnd_, WM_CLOSE, 0, 0);
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            POINT pt = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            int new_hover = hit_test_button(pt);
            if (new_hover != hovered_button_) {
                hovered_button_ = new_hover;
                InvalidateRect(hwnd_, &rect_bubble_, FALSE);
                
                // Ensure we receive WM_MOUSELEAVE
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd_, 0 };
                TrackMouseEvent(&tme);
            }
            return 0;
        }

        case WM_MOUSELEAVE:
            if (hovered_button_ != -1) {
                hovered_button_ = -1;
                InvalidateRect(hwnd_, &rect_bubble_, FALSE);
            }
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
            
        case WM_PAINT:
            handle_paint();
            return 0;
            
        case WM_ERASEBKGND:
            return 1; // Prevent flicker
    }
    return DefWindowProc(hwnd_, msg, wparam, lparam);
}

LRESULT Win32Window::handle_nchittest(POINT screen_pt) {
    POINT pt = screen_pt;
    ScreenToClient(hwnd_, &pt);

    if (PtInRect(&rect_drag_, pt)) return HTCAPTION;
    if (PtInRect(&rect_bubble_, pt)) return HTCLIENT;

    int border = 8;
    bool left = pt.x >= rect_phone_.left && pt.x <= rect_phone_.left + border;
    bool right = pt.x >= rect_phone_.right - border && pt.x <= rect_phone_.right;
    bool top = pt.y >= rect_phone_.top && pt.y <= rect_phone_.top + border;
    bool bottom = pt.y >= rect_phone_.bottom - border && pt.y <= rect_phone_.bottom;

    if (top && left) return HTTOPLEFT;
    if (top && right) return HTTOPRIGHT;
    if (bottom && left) return HTBOTTOMLEFT;
    if (bottom && right) return HTBOTTOMRIGHT;
    if (left) return HTLEFT;
    if (right) return HTRIGHT;
    if (top) return HTTOP;
    if (bottom) return HTBOTTOM;

    return HTCLIENT;
}

void Win32Window::handle_sizing(WPARAM edge, LPARAM lparam) {
    if (aspect_ratio_ <= 0.0) return;

    RECT* r = reinterpret_cast<RECT*>(lparam);
    int total_w = r->right - r->left;
    int total_h = r->bottom - r->top;
    
    int phone_h = total_h - BUBBLE_H - BUBBLE_GAP;
    int phone_w = total_w;

    int new_phone_w, new_phone_h;

    if (edge == WMSZ_LEFT || edge == WMSZ_RIGHT) {
        new_phone_w = phone_w;
        new_phone_h = static_cast<int>(phone_w / aspect_ratio_);
    } else if (edge == WMSZ_TOP || edge == WMSZ_BOTTOM) {
        new_phone_h = phone_h;
        new_phone_w = static_cast<int>(phone_h * aspect_ratio_);
    } else {
        new_phone_w = phone_w;
        new_phone_h = static_cast<int>(phone_w / aspect_ratio_);
    }

    if (new_phone_w < MIN_PHONE_W) {
        new_phone_w = MIN_PHONE_W;
        new_phone_h = static_cast<int>(MIN_PHONE_W / aspect_ratio_);
    }

    int new_total_w = new_phone_w;
    int new_total_h = new_phone_h + BUBBLE_H + BUBBLE_GAP;

    switch (edge) {
        case WMSZ_RIGHT: case WMSZ_BOTTOM: case WMSZ_BOTTOMRIGHT:
            r->right = r->left + new_total_w; r->bottom = r->top + new_total_h; break;
        case WMSZ_LEFT: case WMSZ_BOTTOMLEFT:
            r->left = r->right - new_total_w; r->bottom = r->top + new_total_h; break;
        case WMSZ_TOP: case WMSZ_TOPRIGHT:
            r->right = r->left + new_total_w; r->top = r->bottom - new_total_h; break;
        case WMSZ_TOPLEFT:
            r->left = r->right - new_total_w; r->top = r->bottom - new_total_h; break;
    }
}

} // namespace pm::window
#endif
