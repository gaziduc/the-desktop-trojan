//
// Created by Gazi on 7/12/2025.
//

#include "infoprovider.h"

#include <string>
#include <SDL3/SDL.h>

void InfoProvider::onCriticalSDLError(SDL_Window* window, const std::string& msg) {
    std::string msgString = msg + ": " + SDL_GetError();
    const char* msgStr = msgString.c_str();
    // Log
    SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, msgStr);
    // Show message to user
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "The Desktop Trojan", msgStr, window);
    // Quit program
    exit(1);
}
