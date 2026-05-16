#pragma once
#include <cstdint>
#include <mutex>
#include <vector>

struct SwsContext;

namespace pm::stream {

class VideoRenderer {
public:
    VideoRenderer() = default;
    ~VideoRenderer();

    bool init(void* native_window_handle);
    void render_frame(void* frame);
    void update_viewport(int window_w, int window_h);
    void shutdown();

private:
    void draw_latest_frame();

    void* native_window_handle_{nullptr};
    SwsContext* sws_ctx_{nullptr};
    std::mutex frame_mutex_;
    std::vector<uint8_t> bgra_buffer_;
    int frame_width_{0};
    int frame_height_{0};
    int viewport_width_{0};
    int viewport_height_{0};
    bool has_frame_{false};
};

} // namespace pm::stream
