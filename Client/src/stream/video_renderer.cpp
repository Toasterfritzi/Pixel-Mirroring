#include "video_renderer.h"

#include <algorithm>
#include <iostream>

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace pm::stream {

namespace {
constexpr int WINDOW_TOOLBAR_HEIGHT = 42;

int fit_size(int outer, int inner, int source_outer, int source_inner) {
    if (source_outer <= 0 || source_inner <= 0 || outer <= 0 || inner <= 0) {
        return 0;
    }
    return (source_outer * inner <= source_inner * outer)
        ? source_outer * inner / source_inner
        : outer;
}
}

VideoRenderer::~VideoRenderer() {
    shutdown();
}

bool VideoRenderer::init(void* native_window_handle) {
    native_window_handle_ = native_window_handle;
    return native_window_handle_ != nullptr;
}

void VideoRenderer::render_frame(void* frame) {
    if (!native_window_handle_) {
        return;
    }

    AVFrame* av_frame = static_cast<AVFrame*>(frame);
    if (av_frame) {
        const int width = av_frame->width;
        const int height = av_frame->height;
        if (width <= 0 || height <= 0) {
            return;
        }

        std::lock_guard<std::mutex> lock(frame_mutex_);
        if (width != frame_width_ || height != frame_height_) {
            frame_width_ = width;
            frame_height_ = height;
            bgra_buffer_.assign(static_cast<size_t>(width) * height * 4, 0);
            if (sws_ctx_) {
                sws_freeContext(sws_ctx_);
                sws_ctx_ = nullptr;
            }
        }

        sws_ctx_ = sws_getCachedContext(
            sws_ctx_,
            width,
            height,
            static_cast<AVPixelFormat>(av_frame->format),
            width,
            height,
            AV_PIX_FMT_BGRA,
            SWS_BILINEAR,
            nullptr,
            nullptr,
            nullptr
        );
        if (!sws_ctx_) {
            std::cerr << "[Renderer] Could not create scaler" << std::endl;
            return;
        }

        uint8_t* dst_data[4] = { bgra_buffer_.data(), nullptr, nullptr, nullptr };
        int dst_linesize[4] = { width * 4, 0, 0, 0 };
        sws_scale(
            sws_ctx_,
            av_frame->data,
            av_frame->linesize,
            0,
            height,
            dst_data,
            dst_linesize
        );
        has_frame_ = true;
    }

    draw_latest_frame();
}

void VideoRenderer::update_viewport(int window_w, int window_h) {
    viewport_width_ = window_w;
    viewport_height_ = window_h;
}

void VideoRenderer::shutdown() {
    std::lock_guard<std::mutex> lock(frame_mutex_);
    if (sws_ctx_) {
        sws_freeContext(sws_ctx_);
        sws_ctx_ = nullptr;
    }
    bgra_buffer_.clear();
    frame_width_ = 0;
    frame_height_ = 0;
    has_frame_ = false;
    native_window_handle_ = nullptr;
}

void VideoRenderer::draw_latest_frame() {
#ifdef _WIN32
    HWND hwnd = static_cast<HWND>(native_window_handle_);
    if (!hwnd) {
        return;
    }

    std::lock_guard<std::mutex> lock(frame_mutex_);
    if (!has_frame_ || bgra_buffer_.empty() || frame_width_ <= 0 || frame_height_ <= 0) {
        return;
    }

    RECT client{};
    GetClientRect(hwnd, &client);
    int window_w = viewport_width_ > 0 ? viewport_width_ : client.right - client.left;
    int window_h = viewport_height_ > 0 ? viewport_height_ : client.bottom - client.top;
    int phone_x = 0;
    int phone_y = WINDOW_TOOLBAR_HEIGHT;
    int phone_w = window_w;
    int phone_h = (std::max)(0, window_h - WINDOW_TOOLBAR_HEIGHT);
    if (phone_w <= 0 || phone_h <= 0) {
        return;
    }

    int draw_w = fit_size(phone_w, phone_h, frame_width_, frame_height_);
    int draw_h = draw_w > 0 ? draw_w * frame_height_ / frame_width_ : 0;
    if (draw_h > phone_h) {
        draw_h = phone_h;
        draw_w = draw_h * frame_width_ / frame_height_;
    }

    int draw_x = phone_x + (phone_w - draw_w) / 2;
    int draw_y = phone_y + (phone_h - draw_h) / 2;

    HDC hdc = GetDC(hwnd);
    if (!hdc) {
        return;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = frame_width_;
    bmi.bmiHeader.biHeight = -frame_height_;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    StretchDIBits(
        hdc,
        draw_x,
        draw_y,
        draw_w,
        draw_h,
        0,
        0,
        frame_width_,
        frame_height_,
        bgra_buffer_.data(),
        &bmi,
        DIB_RGB_COLORS,
        SRCCOPY
    );
    ReleaseDC(hwnd, hdc);
#endif
}

} // namespace pm::stream
