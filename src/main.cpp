#include <string>

#include "framerate.h"
#include "sdlwrapper.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_image/SDL_image.h>


void initSDL() {
    if (!SDL_SetAppMetadata("The Desktop Trojan", "0.1.0", nullptr)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Couldn't set app metadata: %s", SDL_GetError());
    }
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL3: %s", SDL_GetError());
        exit(1);
    }
    if (!TTF_Init()) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL3_ttf: %s", SDL_GetError());
        exit(1);
    }

    MIX_InitFlags mix_flags = 0;
    MIX_InitFlags mix_init_flags = Mix_Init(mix_flags);
    if ((mix_flags & mix_init_flags) != mix_flags) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL3_mixer: %s", SDL_GetError());
        exit(1);
    }

    if (!Mix_OpenAudio(0, nullptr)) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open audio with SDL3_mixer: %s", SDL_GetError());
        exit(1);
    }
}

Resources loadResources() {
    Mix_Chunk *count_sound = Mix_LoadWAV("resources/sfx/count.wav");
    if (count_sound == nullptr) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load audio: %s", SDL_GetError());
        exit(1);
    }

    Mix_Chunk *explosion_sound = Mix_LoadWAV("resources/sfx/explosion.wav");
    if (explosion_sound == nullptr) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load audio: %s", SDL_GetError());
        exit(1);
    }

    TTF_Font *font = TTF_OpenFont("resources/fonts/super-adorable.ttf", 240.0);
    if (font == nullptr) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load font: %s", SDL_GetError());
        exit(1);
    }

    Resources resources(font, count_sound, explosion_sound);
    return resources;
}

SDLWrapper createSDLWrapper(Resources& resources) {
    SDL_DisplayID primary_display_id = SDL_GetPrimaryDisplay();
    if (primary_display_id == 0) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't get primary display ID: %s", SDL_GetError());
        exit(1);
    }
    const SDL_DisplayMode *display_mode = SDL_GetCurrentDisplayMode(primary_display_id);
    if (display_mode == nullptr) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't get display mode: %s", SDL_GetError());
        exit(1);
    }

    SDL_Window *window = SDL_CreateWindow("The Desktop Trojan", display_mode->w, display_mode->h,
                                             SDL_WINDOW_FULLSCREEN | SDL_WINDOW_TRANSPARENT);
    if (window == nullptr) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window: %s", SDL_GetError());
        exit(1);
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer: %s", SDL_GetError());
        exit(1);
    }

    TTF_TextEngine *text_engine = TTF_CreateRendererTextEngine(renderer);
    if (text_engine == nullptr) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer text engine: %s", SDL_GetError());
        exit(1);
    }

    FPSmanager* fps_manager = static_cast<FPSmanager *>(malloc(sizeof(FPSmanager)));
    SDL_initFramerate(fps_manager);
    SDL_setFramerate(fps_manager, 60);

    SDL_Texture* bomb = IMG_LoadTexture(renderer, "resources/images/bomb.png");
    if (bomb == nullptr) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load image: %s", SDL_GetError());
        exit(1);
    }

    resources._bomb = bomb;

    const SDLWrapper wrapper(window, renderer, text_engine, resources, fps_manager);
    return wrapper;
}


int main(int argc, char *argv[]) {
    initSDL();
    Resources resources = loadResources();
    const SDLWrapper wrapper = createSDLWrapper(resources);

    SDL_FRect locSquare = {.x = 0, .y = static_cast<float>(wrapper.getWindowHeight()) / 2, .w = 3, .h = 3};
    SDL_Event locEvent;
    bool locRunning = true;
    Sint64 locCountDown = 10000;

    while (locRunning) {
        while (SDL_PollEvent(&locEvent)) {
            switch (locEvent.type) {
                case SDL_EVENT_QUIT: // triggered on window close
                    locRunning = false;
                    break;
            }
        }

        locSquare.x++;

        // Clear screen
        SDL_SetRenderDrawColor(wrapper.getRenderer(), 0, 0, 0, 0);
        SDL_RenderClear(wrapper.getRenderer());

        std::string locCountString = std::to_string(locCountDown / 1000);
        TTF_Text *locText = TTF_CreateText(wrapper.getTextEngine(), wrapper.getResources()._font, locCountString.c_str(), 0);
        if (locText == nullptr) {
            SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create text: %s", SDL_GetError());
            return 1;
        }

        int w;
        int h;
        TTF_GetTextSize(locText, &w, &h);

        float bomb_w;
        float bomb_h;
        SDL_GetTextureSize(wrapper.getResources()._bomb, &bomb_w, &bomb_h);
        SDL_FRect dst_rect = { .x = wrapper.getWindowWidth() / 2 - bomb_w / 2, .y = wrapper.getWindowHeight() / 2 - bomb_h / 2, .w = bomb_w, .h = bomb_h };
        SDL_RenderTexture(wrapper.getRenderer(), wrapper.getResources()._bomb, nullptr, &dst_rect);

        TTF_DrawRendererText(
            locText, static_cast<float>(wrapper.getWindowWidth()) / 2 - static_cast<float>(w) / 2,
            static_cast<float>(wrapper.getWindowHeight()) / 2 - static_cast<float>(h) / 2 + 30);

        TTF_DestroyText(locText);



        SDL_SetRenderDrawColor(wrapper.getRenderer(), 255, 255, 255, 255);
        SDL_RenderFillRect(wrapper.getRenderer(), &locSquare);

        // Draw everything to screen
        SDL_RenderPresent(wrapper.getRenderer());

        Uint64 locOldCountDown = locCountDown;
        locCountDown -= static_cast<Sint64>(SDL_framerateDelay(wrapper.getFPSManager()));
        if (locCountDown <= 0) {
            Mix_PlayChannel(-1, wrapper.getResources()._explosion_sound, 0);
            SDL_Delay(3000);
            break;
        }
        if (locOldCountDown / 1000 != locCountDown / 1000) {
            Mix_PlayChannel(-1, wrapper.getResources()._count_sound, 0);
        }
    }

    SDL_DestroyRenderer(wrapper.getRenderer());
    SDL_DestroyWindow(wrapper.getWindow());
    TTF_DestroyRendererTextEngine(wrapper.getTextEngine());
    TTF_CloseFont(wrapper.getResources()._font);
    TTF_Quit();
    Mix_FreeChunk(wrapper.getResources()._count_sound);
    Mix_FreeChunk(wrapper.getResources()._explosion_sound);
    Mix_CloseAudio();
    Mix_Quit();
    SDL_Quit();
    return 0;
}
