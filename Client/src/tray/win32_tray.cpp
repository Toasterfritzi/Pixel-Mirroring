#ifdef _WIN32
#include "win32_tray.h"
#include <iostream>

namespace pm::tray {

namespace {
constexpr UINT WM_TRAYICON = WM_APP + 100;
}

std::unique_ptr<ITray> create_tray() {
    return std::make_unique<Win32Tray>();
}

Win32Tray::Win32Tray() {}

Win32Tray::~Win32Tray() {
    hide();
    if (hwnd_) {
        DestroyWindow(hwnd_);
    }
}

bool Win32Tray::create(const std::string& tooltip, std::function<void()> on_click) {
    on_click_ = std::move(on_click);
    HINSTANCE hinst = GetModuleHandle(nullptr);

    // Cave man register message-only window class
    WNDCLASSEXA wc = { sizeof(wc) };
    wc.lpfnWndProc = Win32Tray::window_proc;
    wc.hInstance = hinst;
    wc.lpszClassName = "PixelMirroringTrayMsgClass";
    RegisterClassExA(&wc);

    // Create message-only window
    hwnd_ = CreateWindowExA(0, wc.lpszClassName, nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, hinst, this);
    if (!hwnd_) {
        std::cerr << "[Tray] Failed to create message-only window" << std::endl;
        return false;
    }

    // Set up NOTIFYICONDATA
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = hwnd_;
    nid_.uID = 1;
    nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid_.uCallbackMessage = WM_TRAYICON;
    nid_.hIcon = LoadIcon(nullptr, IDI_APPLICATION); // Standard application icon

    // Convert UTF-8 tooltip to UTF-16
    MultiByteToWideChar(CP_UTF8, 0, tooltip.c_str(), -1, nid_.szTip, sizeof(nid_.szTip) / sizeof(wchar_t));

    return true;
}

void Win32Tray::show() {
    if (shown_) return;
    if (Shell_NotifyIconW(NIM_ADD, &nid_)) {
        shown_ = true;
    } else {
        std::cerr << "[Tray] NIM_ADD failed" << std::endl;
    }
}

void Win32Tray::hide() {
    if (!shown_) return;
    if (Shell_NotifyIconW(NIM_DELETE, &nid_)) {
        shown_ = false;
    } else {
        std::cerr << "[Tray] NIM_DELETE failed" << std::endl;
    }
}

LRESULT CALLBACK Win32Tray::window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Win32Tray* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = reinterpret_cast<Win32Tray*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<Win32Tray*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    if (self) {
        return self->handle_message(msg, wp, lp);
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

LRESULT Win32Tray::handle_message(UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_TRAYICON) {
        UINT event_id = LOWORD(lp);
        if (event_id == WM_LBUTTONUP || event_id == WM_LBUTTONDBLCLK) {
            if (on_click_) {
                on_click_();
            }
            return 0;
        }
    }
    return DefWindowProc(hwnd_, msg, wp, lp);
}

} // namespace pm::tray
#else
namespace pm::tray {
std::unique_ptr<ITray> create_tray() {
    return nullptr;
}
}
#endif
