//
// Created by Gazi on 4/20/2024.
//

#include "animation.h"
#include "sdlwrapper.h"

#include <iostream>
#include <SDL3_image/SDL_image.h>

Animation::Animation(const SDLWrapper& wrapper, const std::string &anim_filename) {
    IMG_Animation* anim_surfaces = IMG_LoadAnimation(anim_filename.c_str());
    if (anim_surfaces == nullptr) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load animated image: %s", SDL_GetError());
        exit(1);
    }

    for (int i = 0; i < anim_surfaces->count; i++) {
        SDL_Texture *texture = SDL_CreateTextureFromSurface(wrapper.getRenderer(), anim_surfaces->frames[i]);
        if (texture == nullptr) {
            SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create texture from surface: %s", SDL_GetError());
            exit(1);
        }
        _textures.push_back(texture);
    }


    IMG_FreeAnimation(anim_surfaces);
}

SDL_Texture* Animation::getTexture(unsigned texture_index) const {
    return _textures[texture_index];
}

unsigned Animation::getNumTextures() const  {
    return _textures.size();
}



