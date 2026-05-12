#include "scrcpy_client.h"
#include "../adb/adb_client.h"
#include <iostream>
#include <random>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

namespace pm::stream {

ScrcpyClient::ScrcpyClient() {
    video_socket_ = INVALID_SOCKET;
    control_socket_ = INVALID_SOCKET;
}

ScrcpyClient::~ScrcpyClient() {
    stop();
}

void ScrcpyClient::set_frame_callback(FrameCallback cb) {
    frame_cb_ = std::move(cb);
}

bool ScrcpyClient::start(const Config& config) {
    config_ = config;
    
    // Generate random SCID (31-bit)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0, 0x7FFFFFFF);
    scid_ = std::to_string(dis(gen));

    std::cout << "[Scrcpy] Starting session with SCID: " << scid_ << std::endl;

    if (!setup_tunnel()) return false;
    if (!start_server_process()) return false;
    if (!connect_sockets()) return false;
    if (!read_metadata()) return false;

    running_ = true;
    
    video_thread_ = std::thread(&ScrcpyClient::video_thread_loop, this);
    if (config_.control) {
        control_thread_ = std::thread(&ScrcpyClient::control_thread_loop, this);
    }

    return true;
}

void ScrcpyClient::stop() {
    running_ = false;
    
    if (video_socket_ != INVALID_SOCKET) closesocket(video_socket_);
    if (control_socket_ != INVALID_SOCKET) closesocket(control_socket_);
    
    if (video_thread_.joinable()) video_thread_.join();
    if (control_thread_.joinable()) control_thread_.join();
    
    video_socket_ = INVALID_SOCKET;
    control_socket_ = INVALID_SOCKET;
}

bool ScrcpyClient::is_running() const {
    return running_;
}

bool ScrcpyClient::setup_tunnel() {
    pm::adb::AdbClient adb;
    std::string remote = "localabstract:scrcpy_" + scid_;
    std::string local = "tcp:27183";
    
    std::cout << "[Scrcpy] Setting up adb reverse tunnel..." << std::endl;
    
    if (!adb.reverse_port(config_.device_id, remote, local)) {
        std::cout << "[Scrcpy] Reverse tunnel failed, falling back to adb forward..." << std::endl;
        
        if (!adb.forward_port(config_.device_id, local, remote)) {
            std::cerr << "[Scrcpy] Both reverse and forward tunnels failed!" << std::endl;
            return false;
        }
        config_.tunnel_forward = true;
    } else {
        config_.tunnel_forward = false;
    }
    
    return true;
}

bool ScrcpyClient::start_server_process() {
    pm::adb::AdbClient adb;
    
    // 1. Push server
    std::cout << "[Scrcpy] Pushing server..." << std::endl;
    // We assume the executable is run from Client/build or Client/
    std::string server_path = "../scrcpy_download/scrcpy-server.jar";
    if (!adb.push_file(config_.device_id, server_path, "/data/local/tmp/scrcpy-server.jar")) {
        server_path = "../../scrcpy_download/scrcpy-server.jar"; // fallback
        if (!adb.push_file(config_.device_id, server_path, "/data/local/tmp/scrcpy-server.jar")) {
            std::cerr << "[Scrcpy] Could not push scrcpy-server.jar!" << std::endl;
            return false;
        }
    }
    
    // 2. Start server
    std::string cmd = "CLASSPATH=/data/local/tmp/scrcpy-server.jar app_process / com.genymobile.scrcpy.Server 2.7 ";
    cmd += "scid=" + scid_ + " ";
    cmd += "log_level=info ";
    cmd += "audio=false ";
    cmd += "max_size=" + std::to_string(config_.max_size) + " ";
    
    std::cout << "[Scrcpy] Executing server: " << cmd << std::endl;
    
    // Dies müsste eigentlich asynchron laufen, da app_process blockiert.
    // In einer echten Implementierung würde dies in einem Thread via popen() gestartet.
    std::thread([cmd, this]() {
        pm::adb::AdbClient a;
        a.execute_shell_command(config_.device_id, cmd);
    }).detach();

    return true;
}

bool ScrcpyClient::connect_sockets() {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    std::cout << "[Scrcpy] Connecting to local tunnel..." << std::endl;
    
    auto connect_to_port = [](SOCKET& sock, int port) -> bool {
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) return false;

        sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        // Retry connecting for up to 5 seconds
        for (int i = 0; i < 50; ++i) {
            if (connect(sock, (sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        closesocket(sock);
        sock = INVALID_SOCKET;
        return false;
    };

    auto accept_connection = [](SOCKET& sock, int port) -> bool {
        SOCKET server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server_fd == INVALID_SOCKET) return false;

        sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            closesocket(server_fd);
            return false;
        }

        if (listen(server_fd, 1) == SOCKET_ERROR) {
            closesocket(server_fd);
            return false;
        }

        // Wait up to 5 seconds for connection
        fd_set set;
        FD_ZERO(&set);
        FD_SET(server_fd, &set);
        timeval timeout = {5, 0};
        
        if (select(0, &set, nullptr, nullptr, &timeout) > 0) {
            sock = accept(server_fd, nullptr, nullptr);
            closesocket(server_fd);
            return sock != INVALID_SOCKET;
        }

        closesocket(server_fd);
        return false;
    };

    if (config_.tunnel_forward) {
        if (!connect_to_port(video_socket_, 27183)) {
            std::cerr << "[Scrcpy] Failed to connect video socket" << std::endl;
            return false;
        }
        if (config_.control) {
            if (!connect_to_port(control_socket_, 27183)) {
                std::cerr << "[Scrcpy] Failed to connect control socket" << std::endl;
                return false;
            }
        }
    } else {
        if (!accept_connection(video_socket_, 27183)) {
            std::cerr << "[Scrcpy] Failed to accept video socket" << std::endl;
            return false;
        }
        if (config_.control) {
            if (!accept_connection(control_socket_, 27183)) {
                std::cerr << "[Scrcpy] Failed to accept control socket" << std::endl;
                return false;
            }
        }
    }

    std::cout << "[Scrcpy] Sockets connected!" << std::endl;
    return true;
}

bool ScrcpyClient::read_metadata() {
    char device_name[64];
    if (recv(video_socket_, device_name, 64, 0) != 64) {
        std::cerr << "[Scrcpy] Failed to read device name" << std::endl;
        return false;
    }
    
    // device name is 64 bytes max, might be null terminated early
    std::cout << "[Scrcpy] Device name: " << std::string(device_name, 64).c_str() << std::endl;

    // Read codec info (12 bytes: id, width, height)
    uint8_t codec_meta[12];
    if (recv(video_socket_, (char*)codec_meta, 12, 0) != 12) {
        std::cerr << "[Scrcpy] Failed to read codec meta" << std::endl;
        return false;
    }
    
    video_codec_id_ = (codec_meta[0] << 24) | (codec_meta[1] << 16) | (codec_meta[2] << 8) | codec_meta[3];
    initial_width_ = (codec_meta[4] << 24) | (codec_meta[5] << 16) | (codec_meta[6] << 8) | codec_meta[7];
    initial_height_ = (codec_meta[8] << 24) | (codec_meta[9] << 16) | (codec_meta[10] << 8) | codec_meta[11];
    
    std::cout << "[Scrcpy] Codec: " << video_codec_id_ << ", Width: " << initial_width_ << ", Height: " << initial_height_ << std::endl;

    return true;
}

void ScrcpyClient::video_thread_loop() {
    if (!decoder_.init(video_codec_id_)) {
        std::cerr << "[Scrcpy] Failed to initialize decoder" << std::endl;
        return;
    }

    auto recv_all = [this](char* buf, int len) -> bool {
        int total = 0;
        while (total < len && running_) {
            int r = recv(video_socket_, buf + total, len - total, 0);
            if (r <= 0) return false;
            total += r;
        }
        return true;
    };

    std::vector<uint8_t> packet_data;
    
    while (running_) {
        uint8_t header[12];
        if (!recv_all((char*)header, 12)) break;

        uint64_t pts = 0;
        for (int i = 0; i < 8; ++i) pts = (pts << 8) | header[i];
        uint32_t size = (header[8] << 24) | (header[9] << 16) | (header[10] << 8) | header[11];
        
        bool is_config = (pts == ((uint64_t)-1)); // Scrcpy sends PTS -1 for config packets sometimes
        
        packet_data.resize(size);
        if (!recv_all((char*)packet_data.data(), size)) break;
        
        if (decoder_.decode(packet_data.data(), size, is_config)) {
            if (frame_cb_) {
                frame_cb_((AVFrame*)decoder_.get_frame());
            }
        }
    }
}

void ScrcpyClient::control_thread_loop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void ScrcpyClient::inject_touch(int action, float x, float y, int w, int h) {
    if (!running_ || control_socket_ == INVALID_SOCKET) return;
    
    uint8_t buf[32] = {0};
    buf[0] = 2; // SC_CONTROL_MSG_TYPE_INJECT_TOUCH_EVENT
    buf[1] = action; // 0=DOWN, 1=UP, 2=MOVE
    
    // pointer_id (8 bytes) = 1
    buf[9] = 1; 
    
    // x (4 bytes)
    uint32_t px = static_cast<uint32_t>(x);
    buf[10] = (px >> 24) & 0xff; buf[11] = (px >> 16) & 0xff; buf[12] = (px >> 8) & 0xff; buf[13] = px & 0xff;
    
    // y (4 bytes)
    uint32_t py = static_cast<uint32_t>(y);
    buf[14] = (py >> 24) & 0xff; buf[15] = (py >> 16) & 0xff; buf[16] = (py >> 8) & 0xff; buf[17] = py & 0xff;
    
    // width (2 bytes)
    buf[18] = (w >> 8) & 0xff; buf[19] = w & 0xff;
    
    // height (2 bytes)
    buf[20] = (h >> 8) & 0xff; buf[21] = h & 0xff;
    
    // pressure (2 bytes) = 1.0 (0xffff)
    buf[22] = 0xff; buf[23] = 0xff;
    
    // action_button & buttons (8 bytes total)
    buf[27] = 1; buf[31] = 1;
    
    send(control_socket_, (const char*)buf, 32, 0);
}

void ScrcpyClient::inject_keycode(int action, int keycode) {
    if (!running_ || control_socket_ == INVALID_SOCKET) return;
    
    uint8_t buf[14] = {0};
    buf[0] = 0; // SC_CONTROL_MSG_TYPE_INJECT_KEYCODE
    buf[1] = action; // 0=DOWN, 1=UP
    
    buf[2] = (keycode >> 24) & 0xff;
    buf[3] = (keycode >> 16) & 0xff;
    buf[4] = (keycode >> 8) & 0xff;
    buf[5] = keycode & 0xff;
    
    send(control_socket_, (const char*)buf, 14, 0);
}

void ScrcpyClient::inject_scroll(float x, float y, int w, int h, float hscroll, float vscroll) {
    // Implement scroll message (21 bytes)
}

} // namespace pm::stream
