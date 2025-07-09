//
// Created by Gazi on 4/20/2024.
//

#ifndef ANIMATION_H
#define ANIMATION_H

#include <SDL3/SDL.h>
#include <string>
#include <vector>


class SDLWrapper;

class Animation {
private:
    std::vector<SDL_Texture*> _textures;

public:
    // Constructors
    Animation(const SDLWrapper& wrapper, const std::string &anim_filename);

    // Destructors
    virtual ~Animation() = default;

    SDL_Texture* getTexture(unsigned texture_index) const;
    unsigned getNumTextures() const;
};



#endif //ANIMATION_H
