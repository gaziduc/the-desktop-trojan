//
// Created by Gazi on 7/9/2025.
//

#ifndef EVENTS_H
#define EVENTS_H

#include "sdlwrapper.h"
#include "resources.h"

class Events {
public:
    static void handleQuitEvent(const SDLWrapper& wrapper, const Resources& resources);
};



#endif //EVENTS_H
