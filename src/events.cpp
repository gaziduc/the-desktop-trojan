//
// Created by Gazi on 7/9/2025.
//

#include "events.h"

#include <SDL3/SDL.h>

#include "sdlwrapper.h"

void Events::handleEvents(const SDLWrapper& wrapper, const Resources& resources) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                TTF_DestroyRendererTextEngine(wrapper.getTextEngine());
                SDL_DestroyRenderer(wrapper.getRenderer());
                SDL_DestroyWindow(wrapper.getWindow());

                TTF_DestroyRendererTextEngine(wrapper.getTextEngine());
                TTF_CloseFont(resources._font);
                TTF_Quit();

                Mix_FreeChunk(resources._count_sound);
                Mix_FreeChunk(resources._explosion_sound);
                Mix_FreeChunk(resources._error_sound);
                Mix_CloseAudio();
                Mix_Quit();

                SDL_DestroyTexture(resources._bomb);
                SDL_DestroyTexture(resources._explosion);
                SDL_Quit();
                exit(0);
                break;
            case SDL_EVENT_KEY_DOWN:
                _keys[event.key.scancode] = true;
                break;
            case SDL_EVENT_KEY_UP:
                _keys[event.key.scancode] = false;
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                _mouseBtn[event.button.button] = true;
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                _mouseBtn[event.button.button] = false;
                break;

        }
    }
}
