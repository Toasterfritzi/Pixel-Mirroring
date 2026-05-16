#pragma once

#include <string>
#include <vector>
#include <optional>

namespace pm::network {

struct DiscoveredDevice {
    std::string ip;
    int adb_port;
    std::string device_name;
    std::vector<std::string> all_ips;
};

class NetworkScanner {
public:
    NetworkScanner();
    ~NetworkScanner();

    std::optional<DiscoveredDevice> discover_and_connect(const std::string& client_id, const std::string& client_name);
    
private:
    std::vector<std::string> get_local_ipv4_bases();
};

} // namespace pm::network
