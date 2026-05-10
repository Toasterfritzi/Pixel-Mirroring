#include <iostream>
#include <thread>
#include "adb/adb_client.h"

#include "window/window_interface.h"

int main(int argc, char* argv[]) {
    std::cout << "=== Pixel Mirroring Desktop Client ===" << std::endl;

    pm::adb::AdbClient adb;
    
    if (!adb.init()) {
        std::cerr << "Failed to initialize ADB." << std::endl;
        return 1;
    }

    std::cout << "Checking for USB devices for initial setup..." << std::endl;
    
    if (adb.auto_grant_secure_settings()) {
        std::cout << "\nSetup complete! The Android app now has the necessary permissions." << std::endl;
    } else {
        std::cout << "\nNo USB device found, skipping auto-setup." << std::endl;
    }

    std::cout << "\nStarting Native UI..." << std::endl;

    auto window = pm::window::create_window(340, 604, "Pixel Mirroring");
    if (window->create()) {
        window->set_aspect_ratio(340.0 / 604.0); // Typical portrait aspect ratio
        window->show();
        window->process_messages();
    } else {
        std::cerr << "Failed to create window." << std::endl;
    }

    return 0;
}
