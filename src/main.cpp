#include <cmath>

#include "sdlwrapper.h"
#include "framerate.h"
#include "resources.h"
#include <shlobj.h>
#include <string>
#include <filesystem>
#include <fstream>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_mixer/SDL_mixer.h>

#include "events.h"


void initSDL() {
    if (!SDL_SetAppMetadata("The Desktop Trojan", "0.1.0", nullptr)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Couldn't set app metadata: %s", SDL_GetError());
    }
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL3: %s", SDL_GetError());
        exit(1);
    }
    if (!TTF_Init()) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL3_ttf: %s", SDL_GetError());
        exit(1);
    }

    MIX_InitFlags mix_flags = 0;
    MIX_InitFlags mix_init_flags = Mix_Init(mix_flags);
    if ((mix_flags & mix_init_flags) != mix_flags) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL3_mixer: %s", SDL_GetError());
        exit(1);
    }

    if (!Mix_OpenAudio(0, nullptr)) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open audio with SDL3_mixer: %s", SDL_GetError());
        exit(1);
    }
}

Resources loadResources(const SDLWrapper &wrapper) {
    Mix_Chunk *count_sound = Resources::loadSound("resources/sfx/count.wav");
    Mix_Chunk *explosion_sound = Resources::loadSound("resources/sfx/explosion.wav");
    Mix_Chunk *error_sound = Resources::loadSound("resources/sfx/error.wav");
    TTF_Font *font = Resources::loadFont("resources/fonts/impact.ttf", 200);
    SDL_Texture *bomb = Resources::loadImage(wrapper, "resources/images/bomb.png");
    SDL_Texture *explosion = Resources::loadImage(wrapper, "resources/images/explosion.png");

    Resources resources(font, count_sound, explosion_sound, error_sound, bomb, explosion);
    return resources;
}

SDLWrapper createSDLWrapper() {
    SDL_DisplayID primary_display_id = SDL_GetPrimaryDisplay();
    if (primary_display_id == 0) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't get primary display ID: %s", SDL_GetError());
        exit(1);
    }
    const SDL_DisplayMode *display_mode = SDL_GetCurrentDisplayMode(primary_display_id);
    if (display_mode == nullptr) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't get display mode: %s", SDL_GetError());
        exit(1);
    }

    SDL_Window *window = SDL_CreateWindow("The Desktop Trojan", display_mode->w, display_mode->h,
                                          SDL_WINDOW_FULLSCREEN | SDL_WINDOW_TRANSPARENT);
    if (window == nullptr) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window: %s", SDL_GetError());
        exit(1);
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer: %s", SDL_GetError());
        exit(1);
    }

    TTF_TextEngine *text_engine = TTF_CreateRendererTextEngine(renderer);
    if (text_engine == nullptr) {
        SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer text engine: %s", SDL_GetError());
        exit(1);
    }

    FPSmanager *fps_manager = static_cast<FPSmanager *>(malloc(sizeof(FPSmanager)));
    SDL_initFramerate(fps_manager);
    SDL_setFramerate(fps_manager, 60);

    const SDLWrapper wrapper(window, renderer, text_engine, fps_manager);
    return wrapper;
}


int main(int argc, char *argv[]) {
    std::filesystem::path exe_path = std::filesystem::path(argv[0]);
    std::string idiotArg("--idiot");
    bool isIdiot = argc > 1 && idiotArg == argv[1];

    Sint64 locCountDown;

    if (isIdiot) {
        // At this point, we've been launched by the LOVE-LETTER-FOR-YOU.TXT.vbs, we need to change dir so the program loads resources correctly
        _wchdir(exe_path.parent_path().wstring().c_str());

        locCountDown = 4000;
    } else {
        locCountDown = 10000;
    }

    bool isReallyIdiot = std::filesystem::exists("Tchernobyl.txt");
    if (isReallyIdiot) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "The Desktop Trojan", "End of the demo. Thanks for playing!", nullptr);
        return 0;
    }

    initSDL();
    const SDLWrapper wrapper = createSDLWrapper();
    const Resources resources = loadResources(wrapper);

    if (isIdiot) {
        for (int i = 0; i < 15; i++) {
            Mix_PlayChannel(-1, resources._error_sound, 0);
            SDL_Delay(100);
        }
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "The Desktop Trojan", "You're really an idiot!", nullptr);
    }

    Mix_PlayChannel(-1, resources._error_sound, 0);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "The Desktop Trojan - System error",
                             "The code execution cannot proceed because\nYOU_ARE_AN_IDIOT.dll was not found.\nReinstalling the program may fix this problem.",
                             wrapper.getWindow());
    SDL_framerateDelay(wrapper.getFPSManager());

    wchar_t path[MAX_PATH] = {0};
    SHGetSpecialFolderPathW(nullptr, path, CSIDL_DESKTOPDIRECTORY, 0);

    std::wstring i_love_you_absolute_filename(path);
    wchar_t i_love_you_filename[] = L"LOVE-LETTER-FOR-YOU.TXT.vbs";
    i_love_you_absolute_filename += L"\\" + std::wstring(i_love_you_filename);

    wchar_t current_app_dir[MAX_PATH] = {0};
    GetCurrentDirectoryW(MAX_PATH, current_app_dir);

    std::wofstream i_love_you(i_love_you_absolute_filename.c_str());
    i_love_you << L"Dim shell\n"
            << L"Set shell = WScript.CreateObject(\"WScript.Shell\")\n"
            << L"shell.Run(\"\"\"" << current_app_dir << L"\\" << exe_path.filename().c_str() << L"\"\" --idiot\")\n"
            << L"Set shell = Nothing";
    i_love_you.close();


    float bomb_w;
    float bomb_h;
    SDL_GetTextureSize(resources._bomb, &bomb_w, &bomb_h);
    SDL_FRect dst_rect = {
        .x = wrapper.getWindowWidth() / 2 - bomb_w / 2, .y = wrapper.getWindowHeight() / 2 - bomb_h / 2, .w = bomb_w,
        .h = bomb_h
    };


    int red = 0;
    bool isRedGoingUp = true;

    while (locCountDown > 0) {
        Events::handleQuitEvent(wrapper, resources);

        // Clear screen
        SDL_SetRenderDrawColor(wrapper.getRenderer(), red, 0, 0, 0);
        SDL_RenderClear(wrapper.getRenderer());

        // Draw bomb
        SDL_RenderTexture(wrapper.getRenderer(), resources._bomb, nullptr, &dst_rect);

        std::string locCountString = std::to_string(locCountDown / 1000);
        TTF_Text *locText = TTF_CreateText(wrapper.getTextEngine(), resources._font, locCountString.c_str(), 0);
        if (locText == nullptr) {
            SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create text: %s", SDL_GetError());
            return 1;
        }

        int w;
        int h;
        TTF_GetTextSize(locText, &w, &h);
        TTF_DrawRendererText(
            locText,
            std::ceil(static_cast<float>(wrapper.getWindowWidth()) / 2 - static_cast<float>(w) / 2),
            std::ceil(static_cast<float>(wrapper.getWindowHeight()) / 2 - static_cast<float>(h) / 2 + 30));

        TTF_DestroyText(locText);

        // Draw everything to screen
        SDL_RenderPresent(wrapper.getRenderer());

        Uint64 locOldCountDown = locCountDown;
        Uint64 millisecondsPassed = SDL_framerateDelay(wrapper.getFPSManager());
        locCountDown -= static_cast<Sint64>(millisecondsPassed);
        Sint64 locSecondsCountDown = locCountDown / 1000;
        if (locOldCountDown / 1000 != locSecondsCountDown || (
                locSecondsCountDown <= 5 && locOldCountDown / 500 != locCountDown / 500) || (
                locSecondsCountDown <= 1 && locOldCountDown / 250 != locCountDown / 250)) {
            Mix_VolumeChunk(resources._count_sound, (MIX_MAX_VOLUME * (10 - locSecondsCountDown) / 10));
            Mix_PlayChannel(-1, resources._count_sound, 0);
        }

        if (isRedGoingUp) {
            red += millisecondsPassed / 2;
            if (red > 200) {
                red = 200;
                isRedGoingUp = false;
            }
        } else {
            red -= millisecondsPassed / 2;
            if (red < 20) {
                red = 20;
                isRedGoingUp = true;
            }
        }
    }

    Mix_PlayChannel(-1, resources._explosion_sound, 0);

    locCountDown = 2000;
    dst_rect.x = wrapper.getWindowWidth() / 2;
    dst_rect.y = wrapper.getWindowHeight() / 2;
    dst_rect.w = 20;
    dst_rect.h = 20;
    while (locCountDown > 0) {
        Events::handleQuitEvent(wrapper, resources);

        // Clear screen
        SDL_SetRenderDrawColor(wrapper.getRenderer(), red, 0, 0, 0);
        SDL_RenderClear(wrapper.getRenderer());

        // Draw explosion
        SDL_RenderTexture(wrapper.getRenderer(), resources._explosion, nullptr, &dst_rect);

        // Draw everything to screen
        SDL_RenderPresent(wrapper.getRenderer());

        // Delay
        locCountDown -= static_cast<Sint64>(SDL_framerateDelay(wrapper.getFPSManager()));

        dst_rect.x -= 10;
        dst_rect.y -= 10;
        dst_rect.w += 20;
        dst_rect.h += 20;
    }

    if (isIdiot) {
        SDL_MessageBoxButtonData downloadButtonData = {.flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, .buttonID = 0, .text = "Download" };
        SDL_MessageBoxData msgData = {
            .flags = SDL_MESSAGEBOX_ERROR, .window = wrapper.getWindow(), .title = "The Desktop Trojan",
            .message = "Some of the game files are corrupted.\nPlease re-download the game at https://github.com/gaziduc/the-desktop-trojan to be able to really play it",
            .numbuttons = 1,
            .buttons = &downloadButtonData,
            .colorScheme = nullptr
        };
        int buttonId = 0;
        SDL_ShowMessageBox(&msgData, &buttonId);
        if (buttonId == 0) {
            ShellExecute(NULL, "open", "iexplore.exe", "https://github.com/gaziduc/you-are-an-idiot", NULL, SW_SHOWDEFAULT);
        }
    } else {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "The Desktop Trojan",
                                 "Click on the new icon on your desktop!", wrapper.getWindow());
    }

    return 0;
}
