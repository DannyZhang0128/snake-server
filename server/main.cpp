#include "App.h"
#include <iostream>

struct PerSocketData {};

int main() {
    uWS::App().ws<PerSocketData>("/*", {
        .open = [](auto *ws) {
            std::cout << "client connected\n";
        },
        .message = [](auto *ws, std::string_view msg, uWS::OpCode op) {
            ws->send(msg, op);   // echo it back
        },
        .close = [](auto *ws, int code, std::string_view reason) {
            std::cout << "client disconnected\n";
        }
    }).listen(9001, [](auto *token) {
        std::cout << (token ? "listening on 9001\n" : "failed to listen\n");
    }).run();
}
