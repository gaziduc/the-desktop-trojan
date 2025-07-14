#include <cmath>

#include "sdlwrapper.h"
#include "framerate.h"
#include "resources.h"
#include "events.h"
#include "infoprovider.h"
#include "stringutils.h"
#include <shlobj.h>
#include <Lmcons.h>
#include <string>
#include <filesystem>
#include <fstream>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_mixer/SDL_mixer.h>


void initSDL() {
    if (!SDL_SetAppMetadata("The Desktop Trojan", "0.1.0", nullptr)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Couldn't set app metadata: %s", SDL_GetError());
    }
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        InfoProvider::onCriticalSDLError(nullptr, "Couldn't initialize SDL3");
    }
    if (!TTF_Init()) {
        InfoProvider::onCriticalSDLError(nullptr, "Couldn't initialize SDL3_ttf");
    }

    MIX_InitFlags mix_flags = 0;
    MIX_InitFlags mix_init_flags = Mix_Init(mix_flags);
    if ((mix_flags & mix_init_flags) != mix_flags) {
        InfoProvider::onCriticalSDLError(nullptr, "Couldn't initialize SDL3_mixer");
    }

    if (!Mix_OpenAudio(0, nullptr)) {
        InfoProvider::onCriticalSDLError(nullptr, "Couldn't open audio with SDL3_mixer");
    }
}

void showMessage(SDLWrapper &wrapper, const Resources &resources, const char *message, SDL_Color fgColor,
                 SDL_Color bgColor, bool hasOutline) {
    unsigned int length = StringUtils::getUtf8StringSize(message);
    Uint64 millisecondsPassed = 0;
    int currentLength = 1;
    while (true) {
        wrapper.getEvents().handleEvents(wrapper, resources);

        SDL_SetRenderDrawColor(wrapper.getRenderer(), bgColor.r, bgColor.g, bgColor.b, bgColor.a);
        SDL_RenderClear(wrapper.getRenderer());

        TTF_SetFontOutline(resources._font, 0);
        TTF_Text *usernameText = TTF_CreateText(wrapper.getTextEngine(), resources._font, message, currentLength);
        TTF_SetTextColor(usernameText, fgColor.r, fgColor.g, fgColor.b, fgColor.a);
        TTF_DrawRendererText(usernameText, 20, 20);
        TTF_DestroyText(usernameText);

        if (hasOutline) {
            TTF_SetFontOutline(resources._font, 1);
            TTF_Text *usernameTextOutline = TTF_CreateText(wrapper.getTextEngine(), resources._font, message,
                                                           currentLength);
            TTF_SetTextColor(usernameTextOutline, 0, 0, 0, 255);
            TTF_DrawRendererText(usernameTextOutline, 18, 18);
            TTF_DestroyText(usernameTextOutline);
        }


        SDL_RenderPresent(wrapper.getRenderer());

        millisecondsPassed += SDL_framerateDelay(wrapper.getFPSManager());
        if (currentLength >= length) {
            if (wrapper.getEvents()._keys[SDL_SCANCODE_SPACE]) {
                break;
            }
        } else if (millisecondsPassed >= 33) {
            currentLength++;
            millisecondsPassed = 0;
            if (currentLength % 3 == 0) {
                Mix_PlayChannel(-1, resources._count_sound, 0);
            }
        }
    }

    TTF_SetFontOutline(resources._font, 0);
}


int main(int argc, char *argv[]) {
    std::filesystem::path exe_path = std::filesystem::path(argv[0]);
    std::string idiotArg("--idiot");
    bool isIdiot = argc > 1 && idiotArg == argv[1];

    Sint64 locCountDown;

    if (isIdiot) {
        // At this point, we've been launched by the LOVE-LETTER-FOR-YOU.TXT.vbs, we need to change dir so the program loads resources correctly
        _wchdir(exe_path.parent_path().wstring().c_str());

        locCountDown = 2000;
    } else {
        locCountDown = 10000;
    }

    initSDL();
    SDLWrapper wrapper = SDLWrapper::createSDLWrapper();
    const Resources resources = Resources::loadResources(wrapper);
    SDL_SetWindowIcon(wrapper.getWindow(), resources._icon);

    wchar_t username[UNLEN + 1];
    unsigned long size = UNLEN + 1;
    GetUserNameW(username, &size);

    std::string tchernobylFileName = "Tchernobyl.txt";
    bool isReallyIdiot = std::filesystem::exists(tchernobylFileName);
    if (isReallyIdiot) {
        std::wifstream tchernobylFile(tchernobylFileName);
        std::wstring password;
        getline(tchernobylFile, password);

        std::wstring_convert<std::codecvt_utf8<wchar_t> > utf8;

        Mix_Music *music = Mix_LoadMUS("resources/sfx/jenova.mp3");
        if (music == nullptr) {
            InfoProvider::onCriticalSDLError(wrapper.getWindow(), "Couldn't load music");
        }


        std::wstring correctPwd(L"DELETE FROM idiots WHERE name = '");
        correctPwd.append(username).append(L"';");

        if (password == correctPwd) {
            std::wstring rowsAffected = password + L"\n\n1 row(s) affected.";
            TTF_SetFontSize(resources._font, 20);
            Mix_FadeInMusic(music, -1, 20000);
            showMessage(wrapper, resources, utf8.to_bytes(rowsAffected).c_str(), {.r = 0, .g = 160, .b = 0, .a = 255},
                        {.r = 128, .g = 0, .b = 0, .a = 255}, false);
            TTF_SetFontSize(resources._font, 80);
            showMessage(wrapper, resources,
                        utf8.to_bytes(
                            L"???: What? Why did you do that?\nYou're just an idiot anyway.\n\nTry to beat me now!").
                        c_str(),
                        {.r = 128, .g = 0, .b = 0, .a = 255}, {.r = 0, .g = 0, .b = 0, .a = 0}, true);

            SDL_SetWindowFullscreen(wrapper.getWindow(), false);
            SDL_SetWindowSize(wrapper.getWindow(), 800, 600);
            SDL_SetWindowTitle(wrapper.getWindow(), "You are an idiot!");
            Mix_FreeMusic(music);
            music = Mix_LoadMUS("resources/sfx/idiot.mp3");
            if (music == nullptr) {
                InfoProvider::onCriticalSDLError(wrapper.getWindow(), "Couldn't load music");
            }
            Mix_PlayMusic(music, -1);
            SDL_SetWindowBordered(wrapper.getWindow(), false);
            bool windowDirRight = true;
            bool windowDirDown = true;
            while (true) {
                int w;
                int h;
                SDL_GetWindowSize(wrapper.getWindow(), &w, &h);
                wrapper.getEvents().handleEvents(wrapper, resources);
                if (wrapper.getEvents()._mouseBtn[SDL_BUTTON_LEFT]) {
                    wrapper.getEvents()._mouseBtn[SDL_BUTTON_LEFT] = false;

                    SDL_SetWindowSize(wrapper.getWindow(), w / 1.5, h / 1.5);

                    if (w <= 100) {
                        SDL_SetWindowFullscreen(wrapper.getWindow(), true);
                        SDL_SetWindowSize(wrapper.getWindow(), wrapper.getWindowWidth(), wrapper.getWindowHeight());
                        showMessage(wrapper, resources, utf8.to_bytes(L"???: Wh..What? You killed me?\n\n\n").c_str(),{.r = 160, .g = 0, .b = 0, .a = 255},
                        {.r = 0, .g = 0, .b = 0, .a = 0}, true);
                        Mix_HaltMusic();
                        showMessage(wrapper, resources, utf8.to_bytes(L"Thanks for playing this demo!\nDon't forget to put a comment on the game page\n(Be nice, I've done this game in less than 3 days)").c_str(),{.r = 255, .g = 255, .b = 255, .a = 255},
                        {.r = 0, .g = 0, .b = 0, .a = 0}, true);
                        return 0;
                    }
                }

                int x;
                int y;

                SDL_GetWindowPosition(wrapper.getWindow(), &x, &y);
                if (windowDirRight) {
                    if (x >= wrapper.getWindowWidth() - w) {
                        windowDirRight = false;
                    }
                } else if (x <= 0) {
                    windowDirRight = true;
                }
                if (windowDirDown) {
                    if (y >= wrapper.getWindowHeight() - h) {
                        windowDirDown = false;
                    }
                } else if (y <= 0) {
                    windowDirDown = true;
                }
                SDL_SetWindowPosition(wrapper.getWindow(), windowDirRight ? x + 10 : x - 10, windowDirDown ? y + 10 : y - 10);

                // Clear screen
                SDL_SetRenderDrawColor(wrapper.getRenderer(), 0, 0, 0, 255);
                SDL_RenderClear(wrapper.getRenderer());

                // Draw bomb
                SDL_RenderTexture(wrapper.getRenderer(), resources._idiot, nullptr, nullptr);

                // Draw everything to screen
                SDL_RenderPresent(wrapper.getRenderer());

                // Delay
                SDL_framerateDelay(wrapper.getFPSManager());
            }
            return 0;
        }

        TTF_SetFontSize(resources._font, 80);

        std::wstring wStr = L"???: Hello " + std::wstring(username) +
                            L"!\n\nIf you managed to get\nhere, you might not\nbe as idiot as the\nboss thinks...\nMy name is Alice.\n\n(Press SPACE to continue)";
        std::string utf8Message = utf8.to_bytes(wStr);
        const char *message = utf8Message.c_str();

        Mix_PlayMusic(music, -1);
        showMessage(wrapper, resources, message, {.r = 0, .g = 160, .b = 0, .a = 255}, {.r = 0, .g = 0, .b = 0, .a = 0},
                    true);
        Mix_FadeOutMusic(20000);
        showMessage(wrapper, resources, utf8.to_bytes(L"Alice: What the fuck is going on?").c_str(),
                    {.r = 0, .g = 160, .b = 0, .a = 255}, {.r = 0, .g = 0, .b = 0, .a = 255}, true);
        TTF_SetFontSize(resources._font, 20);
        showMessage(wrapper, resources, utf8.to_bytes(L"???: I just killed your PC.\n\nGood luck.").c_str(),
                    {.r = 192, .g = 0, .b = 0, .a = 255}, {.r = 0, .g = 0, .b = 0, .a = 255}, false);
        showMessage(wrapper, resources,
                    utf8.to_bytes(
                        L"Alice: Psst, I know a flaw about this virus!\nActually, if you clear the Tchernobyl.txt file content and put the password in it, your PC will recover from those viruses.\nThe password text is located somewhere inside a file on your desktop...\nSorry, I can't tell you more...")
                    .c_str(), {.r = 0, .g = 160, .b = 0, .a = 255}, {.r = 0, .g = 0, .b = 0, .a = 255}, false);

        return 0;
    }

    if (isIdiot) {
        for (int i = 0; i < 15; i++) {
            Mix_PlayChannel(-1, resources._error_sound, 0);
            SDL_Delay(100);
        }
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "The Desktop Trojan", "You're really an idiot!",
                                 wrapper.getWindow());
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
            << L"Set shell = Nothing\n\n"
            << L"'The PASSWORD is: DELETE FROM idiots WHERE name = '" << username << "';\n";
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
        wrapper.getEvents().handleEvents(wrapper, resources);

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

    locCountDown = 1400;
    dst_rect.x = wrapper.getWindowWidth() / 2;
    dst_rect.y = wrapper.getWindowHeight() / 2;
    dst_rect.w = 20;
    dst_rect.h = 20;
    while (locCountDown > 0) {
        wrapper.getEvents().handleEvents(wrapper, resources);

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
        SDL_MessageBoxButtonData downloadButtonData = {
            .flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, .buttonID = 0, .text = "Download"
        };
        SDL_MessageBoxData msgData = {
            .flags = SDL_MESSAGEBOX_ERROR, .window = wrapper.getWindow(), .title = "The Desktop Trojan",
            .message =
            "Some of the game files are corrupted.\nPlease re-download the game to be able to really play it",
            .numbuttons = 1,
            .buttons = &downloadButtonData,
            .colorScheme = nullptr
        };
        int buttonId = 0;
        SDL_ShowMessageBox(&msgData, &buttonId);
        if (buttonId == 0) {
            ShellExecute(nullptr, "open", "https://github.com/gaziduc/you-are-an-idiot", "", nullptr,
                         SW_SHOWDEFAULT);
        }
    } else {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "The Desktop Trojan",
                                 "Click on the new icon on your desktop!", wrapper.getWindow());
    }

    return 0;
}
