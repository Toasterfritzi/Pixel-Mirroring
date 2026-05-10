#pragma once

#include <string>
#include <vector>

namespace pm::adb {

/**
 * Basic representation of an ADB Device.
 */
struct Device {
    std::string id;
    std::string state;
    std::string model;
    
    bool is_usb() const;
    bool is_tcp() const;
};

/**
 * A simple ADB client that wraps around the adb command line tool or connects directly to the ADB server (port 5037).
 * For simplicity in this first step, we use subprocess execution.
 */
class AdbClient {
public:
    AdbClient();
    ~AdbClient();

    // Initializes the ADB connection/daemon
    bool init();

    // Returns a list of currently connected devices
    std::vector<Device> get_connected_devices();

    // Executes a shell command on a specific device
    std::string execute_shell_command(const std::string& device_id, const std::string& command);

    // Automatically finds a USB device and grants WRITE_SECURE_SETTINGS
    bool auto_grant_secure_settings();

private:
    std::string run_adb_command(const std::vector<std::string>& args);
};

} // namespace pm::adb
