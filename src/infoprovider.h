//
// Created by Gazi on 7/12/2025.
//

#ifndef INFOPROVIDER_H
#define INFOPROVIDER_H

#include <string>
#include <SDL3/SDL.h>


class InfoProvider {
public:
    static void onCriticalSDLError(SDL_Window* window, const std::string& msg);
};



#endif //INFOPROVIDER_H
