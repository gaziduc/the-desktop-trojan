//
// Created by Gazi on 7/6/2025.
//

#ifndef RESOURCES_H
#define RESOURCES_H

#include <string>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "animation.h"


class Resources {
public:
    TTF_Font* _font;
    Mix_Chunk* _count_sound;
    Mix_Chunk* _explosion_sound;
    Mix_Chunk* _error_sound;
    SDL_Texture* _bomb;
    SDL_Texture* _explosion;
    SDL_Surface* _icon;
    SDL_Texture* _idiot;

    static Resources loadResources(const SDLWrapper &wrapper);

    Resources(TTF_Font* font, Mix_Chunk* count_sound, Mix_Chunk* explosion_sound, Mix_Chunk* error_sound, SDL_Texture* bomb, SDL_Texture* explosion, SDL_Surface* icon, SDL_Texture* idiot);

    static Mix_Chunk *loadSound(SDL_Window* window, const std::string &filepath);

    static SDL_Texture* loadImage(const SDLWrapper& wrapper, const std::string &filepath);

    static TTF_Font* loadFont(SDL_Window* window, const std::string &filepath, float ptsize);

};



#endif //RESOURCES_H
