//
// Created by Gazi on 7/6/2025.
//

#include "resources.h"

#include <string>
#include <SDL3_image/SDL_image.h>

#include "sdlwrapper.h"

// Before init
Resources::Resources(
    TTF_Font *font,
    Mix_Chunk *count_sound,
    Mix_Chunk *explosion_sound,
    Mix_Chunk *error_sound,
    SDL_Texture *bomb,
    SDL_Texture* explosion
) {
    _font = font;
    _count_sound = count_sound;
    _explosion_sound = explosion_sound;
    _error_sound = error_sound;
    _bomb = bomb;
    _explosion = explosion;
}

Mix_Chunk *Resources::loadSound(const std::string &filepath) {
    Mix_Chunk *chunk = Mix_LoadWAV(filepath.c_str());
    if (chunk == nullptr) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load audio: %s", SDL_GetError());
        exit(1);
    }
    return chunk;
}

SDL_Texture *Resources::loadImage(const SDLWrapper &wrapper, const std::string &filepath) {
    SDL_Texture *texture = IMG_LoadTexture(wrapper.getRenderer(), filepath.c_str());
    if (texture == nullptr) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load image: %s", SDL_GetError());
        exit(1);
    }
    return texture;
}

TTF_Font *Resources::loadFont(const std::string &filepath, float ptsize) {
    TTF_Font *font = TTF_OpenFont(filepath.c_str(), ptsize);
    if (font == nullptr) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load font: %s", SDL_GetError());
        exit(1);
    }
    return font;
}
