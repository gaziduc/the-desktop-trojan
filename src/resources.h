//
// Created by Gazi on 7/6/2025.
//

#ifndef RESOURCES_H
#define RESOURCES_H
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>


class Resources {
public:
    // Before init
    TTF_Font* _font;
    Mix_Chunk* _count_sound;
    Mix_Chunk* _explosion_sound;
    Mix_Chunk* _error_sound;

    // After window and renderer init
    SDL_Texture* _bomb;

    // Before init
    Resources(TTF_Font* font, Mix_Chunk* count_sound, Mix_Chunk* explosion_sound, Mix_Chunk* error_sound);
};



#endif //RESOURCES_H
