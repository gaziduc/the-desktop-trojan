//
// Created by Gazi on 7/9/2025.
//

#ifndef EVENTS_H
#define EVENTS_H

#include "resources.h"

class Events {
public:
    bool _keys[SDL_SCANCODE_COUNT] = { false };
    bool _mouseBtn[5] = { false };

    void handleEvents(const SDLWrapper& wrapper, const Resources& resources);
};



#endif //EVENTS_H
