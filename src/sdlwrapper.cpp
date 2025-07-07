//
// Created by Gazi on 7/6/2025.
//

#include "sdlwrapper.h"

SDLWrapper::SDLWrapper(SDL_Window *window, SDL_Renderer *renderer, TTF_TextEngine *text_engine,
               const Resources &resources, FPSmanager *fps_manager)
        : _resources(resources) {
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

const Resources& SDLWrapper::getResources() const {
    return _resources;
}
