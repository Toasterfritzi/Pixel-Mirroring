#include "video_renderer.h"

#include <iostream>

extern "C" {
#include <libavutil/frame.h>
#include <libswscale/swscale.h>
}

#include <SDL2/SDL.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace pm::stream {

namespace {
#ifdef _WIN32
constexpr UINT WM_VIDEO_RENDER = WM_APP + 2;
#endif
}

VideoRenderer::~VideoRenderer() {
    shutdown();
}

bool VideoRenderer::init(void* native_window_handle) {
    m_native_window_handle = native_window_handle;
    return m_native_window_handle != nullptr;
}

void VideoRenderer::render_frame(void* frame) {
    if (!m_native_window_handle || !frame) {
        return;
    }

    AVFrame* av_frame = static_cast<AVFrame*>(frame);
    const int width = av_frame->width;
    const int height = av_frame->height;
    if (width <= 0 || height <= 0) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_frame_mutex);
        if (width != m_frame_width || height != m_frame_height) {
            m_frame_width = width;
            m_frame_height = height;
            m_bgra_buffer.assign(static_cast<size_t>(width) * height * 4, 0);
            m_sws_ctx.reset();
        }

        m_sws_ctx.reset(sws_getCachedContext(
            m_sws_ctx.release(),
            width,
            height,
            static_cast<AVPixelFormat>(av_frame->format),
            width,
            height,
            AV_PIX_FMT_BGRA,
            SWS_LANCZOS,
            nullptr,
            nullptr,
            nullptr
        ));
        if (!m_sws_ctx) {
            std::cerr << "[Renderer] Could not create scaler" << std::endl;
            return;
        }

        uint8_t* dst_data[4] = { m_bgra_buffer.data(), nullptr, nullptr, nullptr };
        int dst_linesize[4] = { width * 4, 0, 0, 0 };
        sws_scale(
            m_sws_ctx.get(),
            av_frame->data,
            av_frame->linesize,
            0,
            height,
            dst_data,
            dst_linesize
        );
        m_has_frame = true;
    }

    request_render();
}

void VideoRenderer::paint(struct SDL_Renderer* renderer, int x, int y, int width, int height) {
    if (!renderer || width <= 0 || height <= 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_frame_mutex);
    if (!m_has_frame || m_bgra_buffer.empty() || m_frame_width <= 0 || m_frame_height <= 0) {
        return;
    }

    const uint8_t* pixels = m_bgra_buffer.data();
    int source_w = m_frame_width;
    int source_h = m_frame_height;

    if (width != m_frame_width || height != m_frame_height) {
        if (width != m_scaled_width || height != m_scaled_height) {
            m_scaled_width = width;
            m_scaled_height = height;
            m_scaled_buffer.assign(static_cast<size_t>(width) * height * 4, 0);
            m_paint_sws_ctx.reset();
            if (m_texture) {
                SDL_DestroyTexture(m_texture);
                m_texture = nullptr;
            }
        }

        double src_aspect = static_cast<double>(m_frame_width) / m_frame_height;
        double dst_aspect = static_cast<double>(width) / height;

        int target_w = width;
        int target_h = height;

        if (src_aspect > dst_aspect) {
            target_h = static_cast<int>(width / src_aspect);
        } else {
            target_w = static_cast<int>(height * src_aspect);
        }

        int offset_x = (width - target_w) / 2;
        int offset_y = (height - target_h) / 2;

        m_paint_sws_ctx.reset(sws_getCachedContext(
            m_paint_sws_ctx.release(),
            m_frame_width,
            m_frame_height,
            AV_PIX_FMT_BGRA,
            target_w,
            target_h,
            AV_PIX_FMT_BGRA,
            SWS_LANCZOS,
            nullptr,
            nullptr,
            nullptr
        ));
        if (m_paint_sws_ctx && !m_scaled_buffer.empty()) {
            std::fill(m_scaled_buffer.begin(), m_scaled_buffer.end(), 0);
            const uint8_t* src_data[4] = { m_bgra_buffer.data(), nullptr, nullptr, nullptr };
            int src_linesize[4] = { m_frame_width * 4, 0, 0, 0 };
            
            uint8_t* dst_ptr = m_scaled_buffer.data() + (offset_y * width + offset_x) * 4;
            uint8_t* dst_data[4] = { dst_ptr, nullptr, nullptr, nullptr };
            int dst_linesize[4] = { width * 4, 0, 0, 0 };
            
            sws_scale(m_paint_sws_ctx.get(), src_data, src_linesize, 0, m_frame_height, dst_data, dst_linesize);
            pixels = m_scaled_buffer.data();
            source_w = width;
            source_h = height;
        }
    }

    if (!m_texture && source_w > 0 && source_h > 0) {
        m_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, source_w, source_h);
    }
    
    if (m_texture) {
        SDL_UpdateTexture(m_texture, nullptr, pixels, source_w * 4);
        SDL_Rect dst_rect = { x, y, width, height };
        SDL_RenderCopy(renderer, m_texture, nullptr, &dst_rect);
    }
}

void VideoRenderer::update_viewport(int x, int y, int width, int height) {
    m_viewport_x = x;
    m_viewport_y = y;
    m_viewport_width = width;
    m_viewport_height = height;
}

void VideoRenderer::shutdown() {
    std::lock_guard<std::mutex> lock(m_frame_mutex);
    m_sws_ctx.reset();
    m_paint_sws_ctx.reset();
    if (m_texture) {
        SDL_DestroyTexture(m_texture);
        m_texture = nullptr;
    }
    m_bgra_buffer.clear();
    m_scaled_buffer.clear();
    m_frame_width = 0;
    m_frame_height = 0;
    m_scaled_width = 0;
    m_scaled_height = 0;
    m_has_frame = false;
    m_native_window_handle = nullptr;
}

void VideoRenderer::request_render() {
#ifdef _WIN32
    HWND hwnd = static_cast<HWND>(m_native_window_handle);
    if (hwnd) {
        PostMessage(hwnd, WM_VIDEO_RENDER, 0, 0);
    }
#endif
}

} // namespace pm::stream
