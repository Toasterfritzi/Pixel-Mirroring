#pragma once

namespace pm::stream {

class VideoRenderer {
public:
    VideoRenderer() = default;
    ~VideoRenderer() = default;

    bool init(void* native_window_handle);
    void render_frame(void* frame);
    void update_viewport(int window_w, int window_h);
    void shutdown() {}
};

} // namespace pm::stream
