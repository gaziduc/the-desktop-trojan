//
// Created by Gazi on 7/12/2025.
//

#include "infoprovider.h"

#include <string>
#include <SDL3/SDL.h>

void InfoProvider::on_critical_SDL_error(SDL_Window* window, const std::string& msg) {
    on_critical_error(window, msg + ": " + SDL_GetError());
}

void InfoProvider::on_critical_error(SDL_Window* window, const std::string& msg) {
    const char* msgStr = msg.c_str();
    // Log
    SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, msgStr);
    // Show message to user
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "The Desktop Trojan", msgStr, window);
}
