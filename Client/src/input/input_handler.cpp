#include "input_handler.h"

namespace pm::input {

InputHandler::InputHandler(pm::stream::ScrcpyClient* client) : client_(client) {}

void InputHandler::window_to_device(int wx, int wy, int ww, int wh, int* dx, int* dy) {
    // For now, assume device is 1080x1920
    *dx = (wx * 1080) / ww;
    *dy = (wy * 1920) / wh;
}

void InputHandler::handle_mouse_down(int x, int y, int ww, int wh) {
    int dx, dy;
    window_to_device(x, y, ww, wh, &dx, &dy);
    client_->inject_touch(0, dx, dy, 1080, 1920); // ACTION_DOWN
}

void InputHandler::handle_mouse_up(int x, int y, int ww, int wh) {
    int dx, dy;
    window_to_device(x, y, ww, wh, &dx, &dy);
    client_->inject_touch(1, dx, dy, 1080, 1920); // ACTION_UP
}

void InputHandler::handle_mouse_move(int x, int y, int ww, int wh) {
    int dx, dy;
    window_to_device(x, y, ww, wh, &dx, &dy);
    client_->inject_touch(2, dx, dy, 1080, 1920); // ACTION_MOVE
}

void InputHandler::handle_key_down(int keycode) {
    client_->inject_keycode(0, keycode);
}

void InputHandler::handle_key_up(int keycode) {
    client_->inject_keycode(1, keycode);
}

} // namespace pm::input
