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
    SDL_Renderer*_renderer;
    TTF_TextEngine* _text_engine;
    FPSmanager* _fps_manager;

public:
    SDLWrapper(SDL_Window *window, SDL_Renderer *renderer, TTF_TextEngine *text_engine, FPSmanager *fps_manager);

    SDL_Window *getWindow() const;

    int getWindowWidth() const;

    int getWindowHeight() const;

    SDL_Renderer *getRenderer() const;

    TTF_TextEngine *getTextEngine() const;

    FPSmanager *getFPSManager() const;
};


#endif //WINDOW_H
