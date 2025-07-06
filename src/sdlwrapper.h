//
// Created by Gazi on 7/6/2025.
//

#ifndef WINDOW_H
#define WINDOW_H

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "framerate.h"
#include "resources.h"

class SDLWrapper {
private:
    SDL_Window *_window;
    int _width;
    int _height;
    SDL_Renderer *_renderer;
    TTF_TextEngine *_text_engine;
    FPSmanager *_fps_manager;
    Resources _resources;

public:
    SDLWrapper(SDL_Window *window, SDL_Renderer *renderer, TTF_TextEngine *text_engine,
               const Resources &resources, FPSmanager *fps_manager)
        : _resources(resources) {
        _window = window;
        SDL_GetWindowSize(_window, &_width, &_height);
        _renderer = renderer;
        _text_engine = text_engine;
        _fps_manager = fps_manager;
    }

    SDL_Window *getWindow() const {
        return _window;
    }

    int getWindowWidth() const {
        return _width;
    }

    int getWindowHeight() const {
        return _height;
    }

    SDL_Renderer *getRenderer() const {
        return _renderer;
    }

    TTF_TextEngine *getTextEngine() const {
        return _text_engine;
    }

    FPSmanager *getFPSManager() const {
        return _fps_manager;
    }

    const Resources &getResources() const {
        return _resources;
    }
};


#endif //WINDOW_H
