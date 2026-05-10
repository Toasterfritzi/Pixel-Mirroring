#pragma once

#ifdef __APPLE__
#include <string>
#include "window_interface.h"

namespace pm::window {

class CocoaWindow : public IWindow {
public:
    CocoaWindow(int width, int height, const std::string& title);
    ~CocoaWindow() override;

    bool create() override;
    void show() override;
    void process_messages() override;
    
    void set_aspect_ratio(double ratio) override;
    void set_orientation(bool landscape) override;

private:
    void* window_{nullptr};  // NSWindow* (opaque to avoid including Obj-C headers here)
    void* view_{nullptr};    // NSView*
    int width_;
    int height_;
    std::string title_;
    bool is_landscape_{false};
};

} // namespace pm::window
#endif
