#pragma once
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include "tray_interface.h"

namespace pm::tray {

class Win32Tray : public ITray {
public:
    Win32Tray();
    ~Win32Tray() override;

    bool create(const std::string& tooltip, std::function<void()> on_click) override;
    void show() override;
    void hide() override;

private:
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT handle_message(UINT msg, WPARAM wparam, LPARAM lparam);

    HWND hwnd_{nullptr};
    NOTIFYICONDATAW nid_{};
    std::function<void()> on_click_;
    bool shown_{false};
};

} // namespace pm::tray
#endif
