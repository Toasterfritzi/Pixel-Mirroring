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
    
    std::cout << "[Scrcpy] Setting up adb reverse tunnel..." << std::endl;
    std::string out = adb.execute_shell_command(config_.device_id, "reverse " + remote + " tcp:27183");
    
    // Fallback zu adb forward hier weggelassen für simplicity im ersten Schritt
    return true;
}

bool ScrcpyClient::start_server_process() {
    pm::adb::AdbClient adb;
    
    // 1. Push server
    std::cout << "[Scrcpy] Pushing server..." << std::endl;
    // We assume adb push is handled elsewhere or the file is already there for now.
    
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
    // Hier würde die TCP-Verbindung zu localhost:27183 stattfinden.
    // Platzhalter für WinSock Initialisierung und connect()
    return true;
}

bool ScrcpyClient::read_metadata() {
    // Lese Device Name (64 bytes)
    // Lese Video Codec Meta (12 bytes: id, w, h)
    return true;
}

void ScrcpyClient::video_thread_loop() {
    while (running_) {
        // Empfange 12-Byte Header (PTS, Flags, Size)
        // Empfange Paket-Daten
        // Sende an Decoder
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
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
