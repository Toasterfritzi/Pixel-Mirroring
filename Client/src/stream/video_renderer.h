#pragma once
#include <cstdint>
#include <mutex>
#include <vector>
#include <memory>

struct SwsContext;
extern "C" void sws_freeContext(struct SwsContext* swsContext);

namespace pm::stream {

struct SwsContextDeleter {
    void operator()(SwsContext* ctx) const {
        if (ctx) sws_freeContext(ctx);
    }
};

class VideoRenderer {
public:
    VideoRenderer() = default;
    ~VideoRenderer();

    bool init(void* native_window_handle);
    void render_frame(void* frame);
    void paint(struct SDL_Renderer* renderer, int x, int y, int width, int height);
    void update_viewport(int x, int y, int width, int height);
    void shutdown();

private:
    void request_render();

    void* m_native_window_handle{nullptr};
    struct SDL_Texture* m_texture{nullptr};
    std::unique_ptr<SwsContext, SwsContextDeleter> m_sws_ctx{nullptr};
    std::unique_ptr<SwsContext, SwsContextDeleter> m_paint_sws_ctx{nullptr};
    std::mutex m_frame_mutex;
    std::vector<uint8_t> m_bgra_buffer;
    std::vector<uint8_t> m_scaled_buffer;
    int m_frame_width{0};
    int m_frame_height{0};
    int m_scaled_width{0};
    int m_scaled_height{0};
    int m_viewport_x{0};
    int m_viewport_y{0};
    int m_viewport_width{0};
    int m_viewport_height{0};
    bool m_has_frame{false};
};

} // namespace pm::stream
