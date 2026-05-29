#pragma once
#include <string>
#include <functional>
#include <memory>

namespace pm::tray {

class ITray {
public:
    virtual ~ITray() = default;
    
    // Creates the tray icon (initially hidden)
    virtual bool create(const std::string& tooltip, std::function<void()> on_click) = 0;
    
    // Shows the tray icon
    virtual void show() = 0;
    
    // Hides the tray icon
    virtual void hide() = 0;
};

// Factory function to create platform-specific tray
std::unique_ptr<ITray> create_tray();

} // namespace pm::tray
