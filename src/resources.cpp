//
// Created by Gazi on 7/6/2025.
//

#include "resources.h"

#include <string>
#include <SDL3_image/SDL_image.h>

#include "infoprovider.h"
#include "sdlwrapper.h"


Resources Resources::loadResources(const SDLWrapper &wrapper) {
    Mix_Chunk *count_sound = loadSound(wrapper.getWindow(), "resources/sfx/count.wav");
    Mix_Chunk *explosion_sound = loadSound(wrapper.getWindow(), "resources/sfx/explosion.wav");
    Mix_Chunk *error_sound = loadSound(wrapper.getWindow(), "resources/sfx/error.wav");
    TTF_Font *font = loadFont(wrapper.getWindow(), "resources/fonts/arialnb.ttf", 200);
    SDL_Texture *bomb = loadImage(wrapper, "resources/images/bomb.png");
    SDL_Surface *icon = IMG_Load("resources/images/icon.png");
    if (icon == nullptr) {
        InfoProvider::onCriticalSDLError(nullptr, "Couldn't load image");
    }
    SDL_Texture *explosion = loadImage(wrapper, "resources/images/explosion.png");
    SDL_Texture *idiot = loadImage(wrapper, "resources/images/idiot.png");

    Resources resources(font, count_sound, explosion_sound, error_sound, bomb, explosion, icon, idiot);
    return resources;
}


Resources::Resources(
    TTF_Font *font,
    Mix_Chunk *count_sound,
    Mix_Chunk *explosion_sound,
    Mix_Chunk *error_sound,
    SDL_Texture *bomb,
    SDL_Texture* explosion,
    SDL_Surface* icon,
    SDL_Texture* idiot
) {
    _font = font;
    _count_sound = count_sound;
    _explosion_sound = explosion_sound;
    _error_sound = error_sound;
    _bomb = bomb;
    _explosion = explosion;
    _icon = icon;
    _idiot = idiot;
}

Mix_Chunk *Resources::loadSound(SDL_Window* window, const std::string &filepath) {
    Mix_Chunk *chunk = Mix_LoadWAV(filepath.c_str());
    if (chunk == nullptr) {
        InfoProvider::onCriticalSDLError(window, "Couldn't load audio");
    }
    return chunk;
}

SDL_Texture *Resources::loadImage(const SDLWrapper &wrapper, const std::string &filepath) {
    SDL_Texture *texture = IMG_LoadTexture(wrapper.getRenderer(), filepath.c_str());
    if (texture == nullptr) {
        InfoProvider::onCriticalSDLError(wrapper.getWindow(), "Couldn't load texture");
    }
    return texture;
}

TTF_Font *Resources::loadFont(SDL_Window* window, const std::string &filepath, float ptsize) {
    TTF_Font *font = TTF_OpenFont(filepath.c_str(), ptsize);
    if (font == nullptr) {
        InfoProvider::onCriticalSDLError(window, "Couldn't load font");
    }
    return font;
}
