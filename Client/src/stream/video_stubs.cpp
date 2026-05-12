#include "video_decoder.h"
#include "video_renderer.h"

namespace pm::stream {

bool VideoDecoder::init(uint32_t codec_id) { return true; }
bool VideoDecoder::decode(const uint8_t* data, size_t size, bool is_config) { return false; }

bool VideoRenderer::init(void* native_window_handle) { return true; }
void VideoRenderer::render_frame(void* frame) {}
void VideoRenderer::update_viewport(int window_w, int window_h) {}

} // namespace pm::stream
