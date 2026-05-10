#include "adb_client.h"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <array>
#include <memory>
#include <regex>

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

namespace pm::adb {

bool Device::is_usb() const {
    // ADB over TCP/IP usually contains an IP address format, USB does not
    return id.find('.') == std::string::npos;
}

bool Device::is_tcp() const {
    return !is_usb();
}

AdbClient::AdbClient() {
}

AdbClient::~AdbClient() {
}

bool AdbClient::init() {
    // Start ADB server just in case
    run_adb_command({"start-server"});
    return true;
}

std::string AdbClient::run_adb_command(const std::vector<std::string>& args) {
    std::string command = "adb";
    for (const auto& arg : args) {
        command += " " + arg;
    }

    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&PCLOSE)> pipe(POPEN(command.c_str(), "r"), PCLOSE);
    
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

std::vector<Device> AdbClient::get_connected_devices() {
    std::vector<Device> devices;
    std::string output = run_adb_command({"devices", "-l"});
    
    std::istringstream stream(output);
    std::string line;
    
    // Skip the first line ("List of devices attached")
    std::getline(stream, line);
    
    while (std::getline(stream, line)) {
        if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos) {
            continue;
        }

        std::istringstream linestream(line);
        std::string id, state;
        linestream >> id >> state;

        if (state == "device") {
            Device dev;
            dev.id = id;
            dev.state = state;
            
            // Extract model if available
            std::smatch match;
            if (std::regex_search(line, match, std::regex("model:([^\\s]+)"))) {
                dev.model = match[1].str();
            }
            
            devices.push_back(dev);
        }
    }
    
    return devices;
}

std::string AdbClient::execute_shell_command(const std::string& device_id, const std::string& command) {
    return run_adb_command({"-s", device_id, "shell", command});
}

bool AdbClient::auto_grant_secure_settings() {
    auto devices = get_connected_devices();
    
    Device* usb_device = nullptr;
    for (auto& dev : devices) {
        if (dev.is_usb()) {
            usb_device = &dev;
            break;
        }
    }

    if (!usb_device) {
        std::cout << "No USB device found. Please connect your Android device via USB." << std::endl;
        return false;
    }

    std::cout << "Found USB device: " << usb_device->model << " (" << usb_device->id << ")" << std::endl;
    std::cout << "Granting WRITE_SECURE_SETTINGS..." << std::endl;

    std::string output = execute_shell_command(
        usb_device->id, 
        "pm grant dev.pixelmirroring.app android.permission.WRITE_SECURE_SETTINGS"
    );

    // Usually ADB returns empty on success for pm grant
    if (output.find("Exception") != std::string::npos || output.find("Error") != std::string::npos) {
        std::cerr << "Failed to grant permission: " << output << std::endl;
        return false;
    }

    std::cout << "Permission granted successfully!" << std::endl;
    return true;
}

} // namespace pm::adb
