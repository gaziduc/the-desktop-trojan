//
// Created by Gazi on 7/6/2025.
//

#include "resources.h"

// Before init
Resources::Resources(TTF_Font *font, Mix_Chunk *count_sound, Mix_Chunk *explosion_sound, Mix_Chunk *error_sound) {
    _font = font;
    _count_sound = count_sound;
    _explosion_sound = explosion_sound;
    _error_sound = error_sound;
}
