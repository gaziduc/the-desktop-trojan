//
// Created by Gazi on 7/9/2025.
//

#include "events.h"

#include <SDL3/SDL.h>

#include "sdlwrapper.h"

void Events::handleQuitEvent(const SDLWrapper& wrapper, const Resources& resources) {
    SDL_Event locEvent;

    while (SDL_PollEvent(&locEvent)) {
        switch (locEvent.type) {
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
        }
    }
}
