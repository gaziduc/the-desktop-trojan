#include <iostream>
#include "framerate.h"
#include <SDL3/SDL.h>

int main(int argc, char *argv[]) {
    if (!SDL_SetAppMetadata("The Desktop Trojan", "0.1.0", nullptr)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Couldn't set app metadata: %s", SDL_GetError());
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
        return 1;
    }
    SDL_DisplayID locPrimaryDisplayID = SDL_GetPrimaryDisplay();
    if (locPrimaryDisplayID == 0) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't get primary display ID: %s", SDL_GetError());
        return 1;
    }
    const SDL_DisplayMode *locDisplayMode = SDL_GetCurrentDisplayMode(locPrimaryDisplayID);
    if (locDisplayMode == nullptr) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't get display mode: %s", SDL_GetError());
        return 1;
    }
    SDL_Window *locWindow = SDL_CreateWindow("The Desktop Trojan", locDisplayMode->w, locDisplayMode->h,
                                             SDL_WINDOW_FULLSCREEN | SDL_WINDOW_TRANSPARENT);
    if (locWindow == nullptr) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window: %s", SDL_GetError());
        return 1;
    }
    SDL_Renderer *locRenderer = SDL_CreateRenderer(locWindow, nullptr);
    if (locRenderer == nullptr) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer: %s", SDL_GetError());
        return 1;
    }

    FPSmanager locFpsManager;
    SDL_initFramerate(&locFpsManager);
    SDL_setFramerate(&locFpsManager, 60);

    SDL_FRect locSquare = { .x = 0, .y = static_cast<float>(locDisplayMode->h) / 2, .w = 3, .h = 3 };
    SDL_Event locEvent;
    bool locRunning = true;

    while (locRunning) {
        while (SDL_PollEvent(&locEvent)) {
            switch (locEvent.type) {
                case SDL_EVENT_QUIT: // triggered on window close
                    locRunning = false;
                    break;
            }
        }

        locSquare.x++;


        SDL_SetRenderDrawColor(locRenderer, 0, 0, 0, 0);
        SDL_RenderClear(locRenderer);

        SDL_SetRenderDrawColor(locRenderer, 255, 255, 255, 255);
        SDL_RenderFillRect(locRenderer, &locSquare);

        // draw everything to screen
        SDL_RenderPresent(locRenderer);

        SDL_framerateDelay(&locFpsManager);
    }

    SDL_DestroyRenderer(locRenderer);
    SDL_DestroyWindow(locWindow);
    SDL_Quit();
    return 0;
}