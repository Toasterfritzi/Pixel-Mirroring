#pragma once

#ifdef _WIN32
#include <windows.h>
#include <string>
#include "window_interface.h"

namespace pm::window {

class Win32Window : public IWindow {
public:
    Win32Window(int width, int height, const std::string& title);
    ~Win32Window() override;

    bool create() override;
    void show() override;
    void process_messages() override;
    
    void set_aspect_ratio(double ratio) override;
    void set_orientation(bool landscape) override;
    
    void* get_native_handle() override { return hwnd_; }
    void set_render_callback(std::function<void()> cb) override { render_cb_ = std::move(cb); }

private:
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT handle_message(UINT msg, WPARAM wparam, LPARAM lparam);
    
    LRESULT handle_nchittest(POINT pt);
    void handle_sizing(WPARAM wparam, LPARAM lparam);
    void handle_paint();
    
    void toggle_max_height();
    void recalc_layout();
    void update_region();
    int hit_test_button(POINT client_pt);
    HRGN create_rounded_rect_rgn(const RECT& r, int radius);

    HWND hwnd_{nullptr};
    HFONT icon_font_{nullptr};
    int width_;
    int height_;
    std::string title_;
    
    double aspect_ratio_{0.0};
    bool is_landscape_{false};
    bool is_max_height_{false};
    RECT restore_bounds_{0};
    int hovered_button_{-1}; // 0=drag, 1=min, 2=max, 3=close

    // Layout rectangles
    RECT rect_phone_{0};
    RECT rect_bubble_{0};
    RECT rect_drag_{0}, rect_min_{0}, rect_max_{0}, rect_close_{0};
    
    std::function<void()> render_cb_;
};

} // namespace pm::window
#endif
