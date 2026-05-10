#pragma once
#include <string>
#include <memory>

namespace pm::window {

class IWindow {
public:
    virtual ~IWindow() = default;
    
    // Creates and initializes the window. Returns true on success.
    virtual bool create() = 0;
    
    // Shows the window on screen.
    virtual void show() = 0;
    
    // Enters the main message loop. Blocks until the window is closed.
    virtual void process_messages() = 0;
    
    // Sets the aspect ratio to lock to (e.g. 9.0/16.0 for portrait).
    virtual void set_aspect_ratio(double ratio) = 0;
    
    // Signals the window that the orientation has changed.
    virtual void set_orientation(bool landscape) = 0;
};

// Factory function to create the appropriate window implementation for the current OS.
std::unique_ptr<IWindow> create_window(int width, int height, const std::string& title);

} // namespace pm::window
