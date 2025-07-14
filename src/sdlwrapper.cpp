//
// Created by Gazi on 7/6/2025.
//

#include "sdlwrapper.h"

#include "infoprovider.h"

SDLWrapper SDLWrapper::createSDLWrapper() {
    SDL_DisplayID primary_display_id = SDL_GetPrimaryDisplay();
    if (primary_display_id == 0) {
        InfoProvider::onCriticalSDLError(nullptr, "Couldn't get primary display ID");
    }
    const SDL_DisplayMode *display_mode = SDL_GetCurrentDisplayMode(primary_display_id);
    if (display_mode == nullptr) {
        InfoProvider::onCriticalSDLError(nullptr, "Couldn't get display mode");
    }

    SDL_Window *window = SDL_CreateWindow("The Desktop Trojan", display_mode->w, display_mode->h,
                                          SDL_WINDOW_FULLSCREEN | SDL_WINDOW_TRANSPARENT);
    if (window == nullptr) {
        InfoProvider::onCriticalSDLError(nullptr, "Couldn't create window");
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr) {
        InfoProvider::onCriticalSDLError(nullptr, "Couldn't create renderer");
    }

    TTF_TextEngine *text_engine = TTF_CreateRendererTextEngine(renderer);
    if (text_engine == nullptr) {
        InfoProvider::onCriticalSDLError(nullptr, "Couldn't create renderer text engine");
    }

    FPSmanager *fps_manager = static_cast<FPSmanager *>(malloc(sizeof(FPSmanager)));
    SDL_initFramerate(fps_manager);
    SDL_setFramerate(fps_manager, 60);

    Events events;

    const SDLWrapper wrapper(window, renderer, text_engine, fps_manager, events);
    return wrapper;
}


SDLWrapper::SDLWrapper(SDL_Window *window, SDL_Renderer *renderer, TTF_TextEngine *text_engine, FPSmanager *fps_manager,
                       const Events &events)
    : _events(events) {
    _window = window;
    SDL_GetWindowSize(_window, &_width, &_height);
    _renderer = renderer;
    _text_engine = text_engine;
    _fps_manager = fps_manager;
}

SDL_Window* SDLWrapper::getWindow() const {
    return _window;
}

int SDLWrapper::getWindowWidth() const {
    return _width;
}

int SDLWrapper::getWindowHeight() const {
    return _height;
}

SDL_Renderer* SDLWrapper::getRenderer() const {
    return _renderer;
}

TTF_TextEngine* SDLWrapper::getTextEngine() const {
    return _text_engine;
}

FPSmanager* SDLWrapper::getFPSManager() const {
    return _fps_manager;
}

Events& SDLWrapper::getEvents() {
    return _events;
}
