#define SDL_MAIN_USE_CALLBACKS 1

#define CIH_FILENAME "CIH.txt"
#define ARGV_ZIP_FILENAME "ARGV.zip"
#define ARGV_FILENAME "ARGV.txt"

#include "infoprovider.h"
#include "stringutils.h"
#include <cmath>
#include <shlobj.h>
#include <Lmcons.h>
#include <string>
#include <filesystem>
#include <fstream>
#include <functional>
#include <vector>
#include <zip.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_textengine.h>

struct sfx {
    Mix_Chunk *count_sound;
    Mix_Chunk *explosion_sound;
    Mix_Chunk *error_sound;
    Mix_Chunk *line_feed;
    SDL_Surface *icon;
};

struct img {
    SDL_Surface *icon;
    SDL_Texture *bomb;
    SDL_Texture *explosion;
    SDL_Texture *idiot;
};

enum launch_state {
    FIRST_LAUNCH = 0,
    VBS_LAUNCH,
    CIH_LAUNCH,
    PASSWORD_CORRECT_LAUNCH,
    AFTER_REBOOT_LAUNCH,
    CORRECT_ZIP_LAUNCH
};

enum lifecycle {
    BEGIN = 0,
    MIDDLE,
    END
};

struct message {
    std::string str;
    SDL_Color text_color;
    SDL_Color bg_color;
    bool has_outline;
    float font_size;
    unsigned int utf8_msg_length;
    std::function<void()> end_callback;
};

struct app_state {
    // args
    int argc = 0;
    std::vector<std::string> argv_str;
    std::filesystem::path exe_path;
    wchar_t username[UNLEN + 1];
    wchar_t current_app_dir[MAX_PATH];
    // SDL3
    SDL_Window *window = nullptr;
    int window_width;
    int window_height;
    SDL_Renderer *renderer = nullptr;
    TTF_TextEngine *text_engine = nullptr;
    Uint64 last_ticks;
    Mix_Music *music = nullptr;
    SDL_Camera *camera = nullptr;
    SDL_Texture *camera_texture = nullptr;
    SDL_FRect camera_viewport;
    // save
    launch_state launch_state = FIRST_LAUNCH;
    lifecycle lifecycle = BEGIN;
    // resources
    img img;
    sfx sfx;
    TTF_Font *font = nullptr;
    // countdown
    int countdown_ms;
    Uint64 end_countdown_ticks;
    int countdown_bg_color_red;
    bool bg_color_red_increase;
    // message
    bool is_in_message;
    bool current_message_ended;
    std::vector<message> messages;
    int current_message_num;
    int current_message_display_utf8_length;
    // misc
    std::wstring password;
    bool start_your_are_an_idiot_phase = false;
    bool is_in_you_are_an_idiot_phase = false;
    SDL_FRect window_idiot_coordinates;
    SDL_FPoint window_idiot_speed;
    unsigned long frame_count;
    bool is_in_explosion_phase = false;
    SDL_FRect explosion_coordinates;
    bool start_camera_phase = false;
    bool is_in_camera_phase = false;
    int last_random = -1;
    bool color_going_up = true;
};

bool is_countdown_step(launch_state launch_state);

message create_message(const wchar_t *text, SDL_Color text_color, SDL_Color bg_color, bool has_outline,
                       float font_size);

message create_message(const wchar_t *text, SDL_Color text_color, SDL_Color bg_color, bool has_outline, float font_size,
                       std::function<void()> end_callback);

void set_messages(app_state *state, const std::vector<message> &messages);

void show_message(app_state *state);

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
    if (!SDL_SetAppMetadata("The Desktop Trojan", "0.2.0", nullptr)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Couldn't set app metadata: %s", SDL_GetError());
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_CAMERA)) {
        InfoProvider::on_critical_SDL_error(nullptr, "Couldn't initialize SDL3");
        return SDL_APP_FAILURE;
    }

    if (!TTF_Init()) {
        InfoProvider::on_critical_SDL_error(nullptr, "Couldn't initialize SDL3_ttf");
        return SDL_APP_FAILURE;
    }

    MIX_InitFlags mix_flags = 0;
    MIX_InitFlags mix_init_flags = Mix_Init(mix_flags);
    if ((mix_flags & mix_init_flags) != mix_flags) {
        InfoProvider::on_critical_SDL_error(nullptr, "Couldn't initialize SDL3_mixer");
        return SDL_APP_FAILURE;
    }

    if (!Mix_OpenAudio(0, nullptr)) {
        InfoProvider::on_critical_SDL_error(nullptr, "Couldn't open audio with SDL3_mixer");
        return SDL_APP_FAILURE;
    }

    SDL_DisplayID primary_display_id = SDL_GetPrimaryDisplay();
    if (primary_display_id == 0) {
        InfoProvider::on_critical_SDL_error(nullptr, "Couldn't get primary display ID");
        return SDL_APP_FAILURE;
    }
    const SDL_DisplayMode *display_mode = SDL_GetCurrentDisplayMode(primary_display_id);
    if (display_mode == nullptr) {
        InfoProvider::on_critical_SDL_error(nullptr, "Couldn't get display mode");
        return SDL_APP_FAILURE;
    }

    SDL_Window *window = SDL_CreateWindow("The Desktop Trojan", display_mode->w, display_mode->h,
                                          SDL_WINDOW_FULLSCREEN | SDL_WINDOW_TRANSPARENT);
    if (window == nullptr) {
        InfoProvider::on_critical_SDL_error(nullptr, "Couldn't create window");
        return SDL_APP_FAILURE;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr) {
        InfoProvider::on_critical_SDL_error(window, "Couldn't create renderer");
        return SDL_APP_FAILURE;
    }

    if (!SDL_SetRenderVSync(renderer, 1)) {
        InfoProvider::on_critical_SDL_error(window, "Couldn't set VSync");
        return SDL_APP_FAILURE;
    }

    TTF_TextEngine *text_engine = TTF_CreateRendererTextEngine(renderer);
    if (text_engine == nullptr) {
        InfoProvider::on_critical_SDL_error(window, "Couldn't create renderer text engine");
        return SDL_APP_FAILURE;
    }

    std::vector<std::string> argv_str;
    for (int i = 0; i < argc; i++) {
        std::string argv_str_i(argv[i]);
        argv_str.push_back(argv_str_i);
    }

    static app_state state;
    state.argc = argc;
    state.argv_str = argv_str;
    state.exe_path = std::filesystem::path(argv_str[0]);
    state.window = window;
    state.window_width = display_mode->w;
    state.window_height = display_mode->h;
    state.renderer = renderer;
    state.text_engine = text_engine;

    unsigned long size = UNLEN + 1;
    if (GetUserNameW(state.username, &size) == 0) {
        InfoProvider::on_critical_error(window, "Couldn't get Windows username");
        return SDL_APP_FAILURE;
    }

    std::wstring correct_password = L"DELETE FROM idiots WHERE name = '";
    correct_password.append(state.username);
    correct_password.append(L"';");

    state.password = correct_password;


    if (argc > 1) {
        // At this point, we've been launched by the LOVE-LETTER-FOR-YOU.TXT.vbs or from startup.
        // we need to change dir so the program loads resources correctly
        _wchdir(state.exe_path.parent_path().wstring().c_str());

        if (argv_str[1] == "--idiot") {
            state.launch_state = VBS_LAUNCH;
            state.countdown_ms = 2000;
        } else if (argv_str[1] == "--really-idiot") {
            state.launch_state = AFTER_REBOOT_LAUNCH;
        } else if (argv_str[1].ends_with(".zip")) {
            // Open zip in parameter
            int error_code = 0;
            zip *z = zip_open(state.argv_str[1].c_str(), 0, &error_code);

            struct zip_stat st;
            zip_stat_init(&st);
            zip_stat(z, ARGV_FILENAME, 0, &st);

            // Allocate memory for its uncompressed contents
            char *content = new char[st.size + 1];

            zip_file *file = zip_fopen_encrypted(z, ARGV_FILENAME, 0, "You are the idiot!");

            content[st.size] = '\0';
            zip_fread(file, content, st.size);
            zip_fclose(file);

            // Close the archive
            zip_close(z);

            std::string content_str(content);
            if (content_str == "shutdown /a") {
                state.launch_state = CORRECT_ZIP_LAUNCH;
            } else {
                state.launch_state = AFTER_REBOOT_LAUNCH;
            }
        }


    } else if (std::filesystem::exists(CIH_FILENAME)) {
        std::wifstream cih_file(CIH_FILENAME);
        if (!cih_file.is_open()) {
            InfoProvider::on_critical_error(window, "Couldn't open file: " CIH_FILENAME);
            return SDL_APP_FAILURE;
        }
        std::wstring password;
        getline(cih_file, password);

        state.launch_state = password != correct_password ? CIH_LAUNCH : PASSWORD_CORRECT_LAUNCH;
    } else {
        state.launch_state = FIRST_LAUNCH;
        state.countdown_ms = 10000;
    }


    // Images
    state.img.icon = IMG_Load("resources/images/icon.png");
    if (state.img.icon == nullptr) {
        InfoProvider::on_critical_SDL_error(window, "Couldn't load image");
        return SDL_APP_FAILURE;
    }
    if (!SDL_SetWindowIcon(window, state.img.icon)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Couldn't set app icon: %s", SDL_GetError());
    }
    state.img.bomb = IMG_LoadTexture(renderer, "resources/images/bomb.png");
    if (state.img.bomb == nullptr) {
        InfoProvider::on_critical_SDL_error(window, "Couldn't load texture");
        return SDL_APP_FAILURE;
    }
    state.img.explosion = IMG_LoadTexture(renderer, "resources/images/explosion.png");
    if (state.img.explosion == nullptr) {
        InfoProvider::on_critical_SDL_error(window, "Couldn't load texture");
        return SDL_APP_FAILURE;
    }
    state.img.idiot = IMG_LoadTexture(renderer, "resources/images/idiot.png");
    if (state.img.idiot == nullptr) {
        InfoProvider::on_critical_SDL_error(window, "Couldn't load texture");
        return SDL_APP_FAILURE;
    }

    // Sounds
    state.sfx.count_sound = Mix_LoadWAV("resources/sfx/count.wav");
    if (state.sfx.count_sound == nullptr) {
        InfoProvider::on_critical_SDL_error(window, "Couldn't load audio");
        return SDL_APP_FAILURE;
    }
    state.sfx.explosion_sound = Mix_LoadWAV("resources/sfx/explosion.wav");
    if (state.sfx.explosion_sound == nullptr) {
        InfoProvider::on_critical_SDL_error(window, "Couldn't load audio");
        return SDL_APP_FAILURE;
    }
    state.sfx.error_sound = Mix_LoadWAV("resources/sfx/error.wav");
    if (state.sfx.error_sound == nullptr) {
        InfoProvider::on_critical_SDL_error(window, "Couldn't load audio");
        return SDL_APP_FAILURE;
    }

    state.sfx.line_feed = Mix_LoadWAV("resources/sfx/linefeed.wav");
    if (state.sfx.line_feed == nullptr) {
        InfoProvider::on_critical_SDL_error(window, "Couldn't load audio");
        return SDL_APP_FAILURE;
    }

    state.font = TTF_OpenFont("resources/fonts/arialnb.ttf", 200);
    if (state.font == nullptr) {
        InfoProvider::on_critical_SDL_error(window, "Couldn't load font");
        return SDL_APP_FAILURE;
    }

    GetCurrentDirectoryW(MAX_PATH, state.current_app_dir);

    state.last_ticks = SDL_GetTicks();
    state.frame_count = 0;

    *appstate = &state;
    return SDL_APP_CONTINUE;
}


SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    app_state *state = static_cast<app_state *>(appstate);

    switch (event->type) {
        case SDL_EVENT_QUIT:
            return SDL_APP_SUCCESS;
        case SDL_EVENT_KEY_DOWN:
            if (event->key.scancode == SDL_SCANCODE_SPACE && state->is_in_message) {
                if (state->current_message_ended) {
                    state->messages[state->current_message_num].end_callback();

                    state->current_message_num++;
                    state->current_message_display_utf8_length = 1;
                    state->current_message_ended = false;

                    if (state->current_message_num >= state->messages.size()) {
                        state->is_in_message = false;
                    }
                } else {
                    state->current_message_display_utf8_length = state->messages[state->current_message_num].
                            utf8_msg_length;
                    state->current_message_ended = true;
                }
            }
            break;
        case SDL_EVENT_KEY_UP:
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (state->is_in_you_are_an_idiot_phase) {
                if (event->button.button == SDL_BUTTON_LEFT) {
                    if (event->button.x >= state->window_idiot_coordinates.x && event->button.x < state->
                        window_idiot_coordinates.x + state->window_idiot_coordinates.w
                        && event->button.y >= state->window_idiot_coordinates.y && event->button.y < state->
                        window_idiot_coordinates.y + state->window_idiot_coordinates.h) {
                        state->window_idiot_coordinates.w /= 1.5;
                        state->window_idiot_coordinates.h /= 1.5;
                        if (state->window_idiot_coordinates.w <= 100) {
                            state->is_in_you_are_an_idiot_phase = false;

                            std::vector<message> messages;
                            messages.push_back(create_message(L"?????: Wh...What? You killed me?",
                                                              {.r = 192, .g = 0, .b = 0, .a = 255},
                                                              {.r = 0, .g = 0, .b = 0, .a = 0}, true, 80, [state] {
                                                                  state->start_camera_phase = true;
                                                              }));
                            set_messages(state, messages);
                        }
                    }
                }
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            break;
        default:
            break;
    }

    return SDL_APP_CONTINUE;
}

Uint32 countdown_sound_callback(void *appstate, SDL_TimerID timer_id, Uint32 interval) {
    app_state *state = static_cast<app_state *>(appstate);
    Uint64 current_ticks = SDL_GetTicks();
    if (current_ticks > state->end_countdown_ticks) {
        return 0;
    }
    Mix_PlayChannel(-1, state->sfx.count_sound, 0);
    return interval * 0.9;
}

message create_message(const wchar_t *text, const SDL_Color text_color, const SDL_Color bg_color,
                       const bool has_outline, const float font_size) {
    return create_message(text, text_color, bg_color, has_outline, font_size, [] {
    });
}

message create_message(const wchar_t *text, const SDL_Color text_color, const SDL_Color bg_color,
                       const bool has_outline, const float font_size, std::function<void()> end_callback) {
    std::wstring_convert<std::codecvt_utf8<wchar_t> > utf8_converter;
    const std::string str = utf8_converter.to_bytes(text);
    message message(str, text_color, bg_color, has_outline, font_size, StringUtils::get_utf8_str_length(str.c_str()),
                    end_callback);
    return message;
}

void set_messages(app_state *state, const std::vector<message> &messages) {
    state->messages = messages;
    state->current_message_num = 0;
    state->current_message_display_utf8_length = 1; // 0 would display the whole message
    state->is_in_message = true;
}


/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate) {
    app_state *state = static_cast<app_state *>(appstate);
    Uint64 current_ticks = SDL_GetTicks();
    Uint64 ticks_passed = current_ticks - state->last_ticks;

    if (state->lifecycle == BEGIN) {
        if (is_countdown_step(state->launch_state)) {
            if (state->launch_state == VBS_LAUNCH) {
                for (int i = 0; i < 15; i++) {
                    Mix_PlayChannel(-1, state->sfx.error_sound, 0);
                    SDL_Delay(100);
                }
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "The Desktop Trojan", "You're really an idiot!",
                                         state->window);
            }

            Mix_PlayChannel(-1, state->sfx.error_sound, 0);
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "The Desktop Trojan - System error",
                                     "The code execution cannot proceed because\nYOU_ARE_AN_IDIOT.dll was not found.\nReinstalling the program may fix this problem.",
                                     state->window);

            if (state->launch_state == FIRST_LAUNCH) {
                wchar_t desktop_path[MAX_PATH] = {0};
                SHGetSpecialFolderPathW(nullptr, desktop_path, CSIDL_DESKTOPDIRECTORY, 0);

                std::wstring i_love_you_absolute_filename(desktop_path);
                i_love_you_absolute_filename += L"\\LOVE-LETTER-FOR-YOU.TXT.vbs";



                std::wofstream i_love_you_file(i_love_you_absolute_filename.c_str());
                i_love_you_file << L"Dim shell\n"
                        << L"Set shell = WScript.CreateObject(\"WScript.Shell\")\n"
                        << L"shell.Run(\"\"\"" << state->current_app_dir << L"\\" << state->exe_path.filename().c_str() <<
                        L"\"\" --idiot\")\n"
                        << L"Set shell = Nothing\n\n"
                        << L"'The PASSWORD is: DELETE FROM idiots WHERE name = '" << state->username << "';\n";
                i_love_you_file.close();
            }

            state->countdown_bg_color_red = 0;
            state->end_countdown_ticks = SDL_GetTicks() + state->countdown_ms;
            SDL_AddTimer(state->countdown_ms / 10, countdown_sound_callback, state);
        } else if (state->launch_state == CIH_LAUNCH) {
            state->music = Mix_LoadMUS("resources/sfx/jenova.mp3");
            if (state->music == nullptr) {
                InfoProvider::on_critical_SDL_error(state->window, "Couldn't load music");
                return SDL_APP_FAILURE;
            }
            Mix_PlayMusic(state->music, -1);
            std::vector<message> messages;
            messages.push_back(create_message(
                (L"?????: Hello " + std::wstring(state->username) +
                 L"!\n\nIf you managed to get\nhere, you might not\nbe as idiot as the\nboss thinks...\nMy name is Alice.\n\n(Press SPACE to continue)")
                .c_str(), {.r = 0, .g = 160, .b = 0, .a = 255}, {.r = 0, .g = 0, .b = 0, .a = 0}, true, 80,
                [] { Mix_FadeOutMusic(10000); }));
            messages.push_back(create_message(L"Alice: What the fuck is going on?",
                                              {.r = 0, .g = 160, .b = 0, .a = 255}, {.r = 0, .g = 0, .b = 0, .a = 255},
                                              true, 80));
            messages.push_back(create_message(L"?????: I just killed your PC.\n\nGood luck.",
                                              {.r = 192, .g = 0, .b = 0, .a = 255}, {.r = 0, .g = 0, .b = 0, .a = 255},
                                              false, 20));
            messages.push_back(create_message(
                L"Alice: Psst, I know a flaw about this virus!\nActually, if you clear the " CIH_FILENAME
                " file content and put the password in it, your PC will recover from those viruses.\nThe password text is located somewhere inside a file on your desktop...\nSorry, I can't tell you more...",
                {.r = 0, .g = 160, .b = 0, .a = 255}, {.r = 0, .g = 0, .b = 0, .a = 255}, false, 20));
            set_messages(state, messages);
        } else if (state->launch_state == PASSWORD_CORRECT_LAUNCH) {
            state->music = Mix_LoadMUS("resources/sfx/jenova.mp3");
            if (state->music == nullptr) {
                InfoProvider::on_critical_SDL_error(state->window, "Couldn't load music");
                return SDL_APP_FAILURE;
            }
            Mix_PlayMusic(state->music, -1);
            std::vector<message> messages;
            messages.push_back(create_message((state->password + L"\n\n1 row(s) affected.").c_str(),
                                              {.r = 0, .g = 160, .b = 0, .a = 255},
                                              {.r = 128, .g = 0, .b = 0, .a = 255}, false, 20));
            messages.push_back(create_message(
                L"?????: What? Why did you do that?\nYou're just an idiot anyway.\n\nTry to beat me now!",
                {.r = 192, .g = 0, .b = 0, .a = SDL_ALPHA_OPAQUE}, {.r = 0, .g = 0, .b = 0, .a = SDL_ALPHA_TRANSPARENT}, true, 80, [state] { state->start_your_are_an_idiot_phase = true; }));
            set_messages(state, messages);
        } else if (state->launch_state == AFTER_REBOOT_LAUNCH) {
            int error_code = 0;
            // Create zip
            zip *z = zip_open(ARGV_ZIP_FILENAME, ZIP_CREATE, &error_code);
            char buffer[] = "shutdown /a";
            zip_source_t *source = zip_source_buffer(z, buffer, strlen(buffer), 0);
            zip_file_add(z, ARGV_FILENAME, source, 0);
            zip_file_set_encryption(z, 0, ZIP_EM_AES_256, "You are the idiot!");
            zip_close(z);

            std::vector<message> messages;
            messages.push_back(create_message(
                (L"Alice: " + std::wstring(state->username) +
                 L", to defeat the trojan, you need to\nfind the " ARGV_ZIP_FILENAME " file I created, which is next to\nthe-desktop-trojan.exe file.\n\nDo not try to open the file directly!\nJust drag and drop it onto the-desktop-trojan.exe\nand you will see the magic happening.")
                .c_str(),
                {.r = 0, .g = 160, .b = 0, .a = SDL_ALPHA_OPAQUE}, {
                    .r = 0, .g = 0, .b = 0, .a = SDL_ALPHA_TRANSPARENT
                }, true, 80, [] {
                    HKEY hkey;
                    RegOpenKeyW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", &hkey);
                    RegDeleteValueW(hkey, L"The Desktop Trojan");
                }));

            set_messages(state, messages);
        } else if (state->launch_state == CORRECT_ZIP_LAUNCH) {
            std::vector<message> messages;
            messages.push_back(create_message(L"?????: Wh...Why?", {.r = 192, .g = 0, .b = 0, .a = SDL_ALPHA_OPAQUE}, {.r = 0, .g = 0, .b = 0, .a = SDL_ALPHA_TRANSPARENT}, true, 80));
            messages.push_back(create_message((std::wstring(state->username) + L", thanks a lot for playing the demo.\n\nDon't forget to let a comment on the webpage!").c_str(), {.r = 255, .g = 255, .b = 255, .a = SDL_ALPHA_OPAQUE}, {.r = 0, .g = 0, .b = 0, .a = SDL_ALPHA_TRANSPARENT}, true, 80));
            set_messages(state, messages);
        }
        state->lifecycle = MIDDLE;
    } else if (state->lifecycle == MIDDLE) {
        if (is_countdown_step(state->launch_state)) {
            if (!state->is_in_explosion_phase) {
                // Clear screen
                SDL_SetRenderDrawColor(state->renderer, state->countdown_bg_color_red, 0, 0,
                                       state->countdown_bg_color_red);
                SDL_RenderClear(state->renderer);

                if (state->bg_color_red_increase) {
                    state->countdown_bg_color_red += ticks_passed / 4;
                    if (state->countdown_bg_color_red >= 128) {
                        state->countdown_bg_color_red = 128;
                        state->bg_color_red_increase = false;
                    }
                } else {
                    state->countdown_bg_color_red -= ticks_passed / 4;
                    if (state->countdown_bg_color_red <= 0) {
                        state->countdown_bg_color_red = 0;
                        state->bg_color_red_increase = true;
                    }
                }

                SDL_FRect dst_rect = {
                    .x = static_cast<float>(state->window_width / 2 - 200),
                    .y = static_cast<float>(state->window_height / 2 - 200),
                    .w = static_cast<float>(400),
                    .h = static_cast<float>(400)
                };
                SDL_RenderTexture(state->renderer, state->img.bomb, nullptr, &dst_rect);

                Sint64 remaining_time_ms = state->end_countdown_ticks - SDL_GetTicks();
                int remaining_time = remaining_time_ms / 1000;
                TTF_Text *text = TTF_CreateText(state->text_engine, state->font, std::to_string(remaining_time).c_str(),
                                                0);
                TTF_SetTextColor(text, 255, 255, 255, SDL_ALPHA_OPAQUE);

                int text_w;
                int text_h;
                TTF_GetTextSize(text, &text_w, &text_h);
                TTF_DrawRendererText(text, state->window_width / 2 - text_w / 2, state->window_height / 2 - text_h / 2);
                TTF_DestroyText(text);

                SDL_RenderPresent(state->renderer);

                if (remaining_time_ms <= 0) {
                    state->is_in_explosion_phase = true;
                    state->explosion_coordinates.x = state->window_width / 2 - 10;
                    state->explosion_coordinates.y = state->window_height / 2 - 10;
                    state->explosion_coordinates.w = 20;
                    state->explosion_coordinates.h = 20;
                    Mix_PlayChannel(-1, state->sfx.explosion_sound, 0);
                }
            } else {
                SDL_SetRenderDrawColor(state->renderer, state->countdown_bg_color_red, 0, 0,
                                       state->countdown_bg_color_red);
                SDL_RenderClear(state->renderer);

                SDL_RenderTexture(state->renderer, state->img.explosion, nullptr, &state->explosion_coordinates);

                state->explosion_coordinates.x -= 10;
                state->explosion_coordinates.w += 20;
                state->explosion_coordinates.y -= 10;
                state->explosion_coordinates.h += 20;

                SDL_RenderPresent(state->renderer);

                if (state->explosion_coordinates.w > 1000) {
                    state->lifecycle = END;
                }
            }
        } else if (state->launch_state == CIH_LAUNCH || state->launch_state == AFTER_REBOOT_LAUNCH || state->launch_state == CORRECT_ZIP_LAUNCH) {
            if (state->is_in_message) {
                show_message(state);
            } else {
                state->lifecycle = END;
            }
        } else if (state->launch_state == PASSWORD_CORRECT_LAUNCH) {
            if (state->is_in_message) {
                show_message(state);
            } else if (state->start_camera_phase) {
                SDL_ShowSimpleMessageBox(
                    SDL_MESSAGEBOX_INFORMATION, "The Desktop Trojan",
                    "I'm going to show you what's an idiot!",
                    state->window);

                int device_count = 0;
                SDL_CameraID *devices = SDL_GetCameras(&device_count);
                if (devices == nullptr) {
                    InfoProvider::on_critical_SDL_error(state->window, "Couldn't enumerate camera devices");
                    return SDL_APP_FAILURE;
                }
                if (device_count == 0) {
                    InfoProvider::on_critical_error(
                        state->window,
                        "Couldn't find any camera devices! Please connect a camera and try again.");
                    return SDL_APP_FAILURE;
                }

                int format_count = 0;
                SDL_CameraSpec **camera_spec =
                        SDL_GetCameraSupportedFormats(
                            devices[0], &format_count);
                // just take the first thing we see in any format it wants.
                state->camera = SDL_OpenCamera(
                    devices[0], camera_spec[1]);
                SDL_free(camera_spec);
                SDL_free(devices);
                if (state->camera == nullptr) {
                    InfoProvider::on_critical_SDL_error(state->window, "Couldn't open camera");
                    return SDL_APP_FAILURE;
                }

                state->music = Mix_LoadMUS(
                    "resources/sfx/lavender.mp3");
                if (state->music == nullptr) {
                    InfoProvider::on_critical_SDL_error(state->window, "Couldn't load music");
                    return SDL_APP_FAILURE;
                }
                Mix_PlayMusic(state->music, -1);

                HKEY hkey = nullptr;
                std::wstring absolute_exe_path_with_params = L"\"" + std::wstring(state->current_app_dir) + L"\\" + state->exe_path.filename().c_str() + L"\" --really-idiot";
                // Creates a key
                LONG create_status = RegCreateKeyW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", &hkey);
                LONG status = RegSetValueExW(hkey,
                    L"The Desktop Trojan",
                    0,
                    REG_SZ,
                    reinterpret_cast<const BYTE *>(absolute_exe_path_with_params.c_str()),
                    (absolute_exe_path_with_params.size() + 1) * sizeof(wchar_t)
                    );

                ShellExecute(nullptr, "open", "shutdown", "/r /t 45", nullptr, SW_SHOWDEFAULT);

                state->start_camera_phase = false;
                state->is_in_camera_phase = true;
            } else if (state->start_your_are_an_idiot_phase) {
                Mix_FreeMusic(state->music);
                state->music = Mix_LoadMUS("resources/sfx/idiot.mp3");
                if (state->music == nullptr) {
                    InfoProvider::on_critical_SDL_error(state->window, "Couldn't load music");
                    return SDL_APP_FAILURE;
                }

                Mix_PlayMusic(state->music, -1);

                float w;
                float h;
                SDL_GetTextureSize(state->img.idiot, &w, &h);
                state->window_idiot_coordinates.x = state->window_width / 2 - w / 2;
                state->window_idiot_coordinates.y = state->window_height / 2 - h / 2;
                state->window_idiot_coordinates.w = w;
                state->window_idiot_coordinates.h = h;
                state->window_idiot_speed.x = 10;
                state->window_idiot_speed.y = 10;

                state->start_your_are_an_idiot_phase = false;
                state->is_in_you_are_an_idiot_phase = true;
            } else if (state->is_in_camera_phase) {
                Uint64 timestamp = 0;
                SDL_Surface *frame = SDL_AcquireCameraFrame(state->camera, &timestamp);

                if (frame != nullptr) {
                    if (state->camera_texture == nullptr) {
                        state->camera_texture = SDL_CreateTexture(state->renderer, frame->format,
                                                                  SDL_TEXTUREACCESS_STREAMING, frame->w, frame->h);
                        state->camera_viewport.x = state->window_width - frame->w / 2 - 100;
                        state->camera_viewport.y = 100;
                        state->camera_viewport.w = frame->w / 2;
                        state->camera_viewport.h = frame->h / 2;
                    }

                    if (state->camera_texture) {
                        SDL_UpdateTexture(state->camera_texture, nullptr, frame->pixels, frame->pitch);
                    }

                    SDL_ReleaseCameraFrame(state->camera, frame);
                }

                SDL_SetRenderDrawColor(state->renderer, 0, 0, 0, SDL_ALPHA_TRANSPARENT);
                SDL_RenderClear(state->renderer);
                if (state->camera_texture) {
                    /* draw the latest camera frame, if available. */
                    SDL_RenderTexture(state->renderer, state->camera_texture, nullptr, &state->camera_viewport);
                }

                SDL_SetRenderDrawColor(state->renderer, 128, 128, 128, SDL_ALPHA_OPAQUE);
                std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_converter;
                const std::string str = utf8_converter.to_bytes((std::wstring(state->username) + L" is an idiot!     →").c_str());
                TTF_SetFontOutline(state->font, 0);
                TTF_Text *text = TTF_CreateText(state->text_engine, state->font, str.c_str(), 0);
                for (int i = 1; i <= 5; i++) {
                    if (state->last_random == -1) {
                        state->last_random = std::rand() % 256;
                    } else {
                        if (state->color_going_up) {
                            state->last_random += 4;
                            if (state->last_random > 255) {
                                state->color_going_up = false;
                                state->last_random = 255;
                            }
                        } else {
                            state->last_random -= 4;
                            if (state->last_random < 0) {
                                state->color_going_up = true;
                                state->last_random = 0;
                            }
                        }

                    }
                    TTF_SetTextColor(text,state->last_random, state->last_random, state->last_random, state->last_random);
                    TTF_DrawRendererText(text, 450, i * 100);
                }

                TTF_DestroyText(text);

                SDL_RenderRect(state->renderer, &state->camera_viewport);
                SDL_RenderPresent(state->renderer);
            } else {
                SDL_SetRenderDrawColor(state->renderer, 0, 0, 0, SDL_ALPHA_TRANSPARENT);
                SDL_RenderClear(state->renderer);

                state->window_idiot_coordinates.x += state->window_idiot_speed.x;
                if (state->window_idiot_coordinates.x + state->window_idiot_coordinates.w > state->window_width) {
                    state->window_idiot_coordinates.x = state->window_width - state->window_idiot_coordinates.w;
                    state->window_idiot_speed.x = -state->window_idiot_speed.x;
                } else if (state->window_idiot_coordinates.x < 0) {
                    state->window_idiot_coordinates.x = 0;
                    state->window_idiot_speed.x = -state->window_idiot_speed.x;
                }
                state->window_idiot_coordinates.y += state->window_idiot_speed.y;
                if (state->window_idiot_coordinates.y + state->window_idiot_coordinates.h >= state->window_height) {
                    state->window_idiot_coordinates.y = state->window_height - state->window_idiot_coordinates.h;
                    state->window_idiot_speed.y = -state->window_idiot_speed.y;
                } else if (state->window_idiot_coordinates.y < 0) {
                    state->window_idiot_coordinates.y = 0;
                    state->window_idiot_speed.y = -state->window_idiot_speed.y;
                }

                SDL_RenderTexture(state->renderer, state->img.idiot, nullptr, &state->window_idiot_coordinates);
                SDL_RenderPresent(state->renderer);
            }
        }
    } else if (state->lifecycle == END) {
        if (state->launch_state == FIRST_LAUNCH) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "The Desktop Trojan",
                                     "Click on the new icon on your desktop!", state->window);
        } else if (state->launch_state == VBS_LAUNCH) {
            SDL_MessageBoxButtonData download_button_data = {
                .flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, .buttonID = 0, .text = "Download"
            };
            SDL_MessageBoxData msg_data = {
                .flags = SDL_MESSAGEBOX_ERROR, .window = state->window, .title = "The Desktop Trojan",
                .message =
                "Some of the game files are corrupted.\nPlease re-download the game to be able to really play it",
                .numbuttons = 1,
                .buttons = &download_button_data,
                .colorScheme = nullptr
            };
            int button_id = 0;
            SDL_ShowMessageBox(&msg_data, &button_id);
            if (button_id == 0) {
                ShellExecute(nullptr, "open", "https://github.com/gaziduc/you-are-an-idiot", "", nullptr,
                             SW_SHOWDEFAULT);
            }
        } else if (state->launch_state == CORRECT_ZIP_LAUNCH) {
            ShellExecute(nullptr, "open", "https://gaziduc.itch.io/the-desktop-trojan", "", nullptr,
                             SW_SHOWDEFAULT);
        }

        return SDL_APP_SUCCESS;
    }

    state->last_ticks = current_ticks;
    state->frame_count++;
    return SDL_APP_CONTINUE;
}


void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    if (result == SDL_APP_FAILURE) {
        exit(1);
    }

    app_state *state = static_cast<app_state *>(appstate);

    SDL_CloseCamera(state->camera);

    SDL_DestroyWindow(state->window);
    SDL_DestroyRenderer(state->renderer);
    TTF_DestroyRendererTextEngine(state->text_engine);

    // Free textures/images
    SDL_DestroyTexture(state->img.bomb);
    SDL_DestroyTexture(state->img.explosion);
    SDL_DestroyTexture(state->img.idiot);
    SDL_DestroySurface(state->img.icon);

    // Free sounds
    Mix_HaltMusic();
    Mix_FreeChunk(state->sfx.count_sound);
    Mix_FreeChunk(state->sfx.error_sound);
    Mix_FreeChunk(state->sfx.explosion_sound);
    Mix_FreeChunk(state->sfx.line_feed);
    Mix_Quit();

    TTF_CloseFont(state->font);
    TTF_Quit();

    SDL_Quit();
}


void show_message(app_state *state) {
    message msg = state->messages[state->current_message_num];
    std::string substr = StringUtils::get_utf8_substr(msg.str, 0, state->current_message_display_utf8_length);
    bool is_line_feed = substr[substr.length() - 1] == '\n';
    if (!state->current_message_ended) {
        substr += '_';
    }
    const char *display_msg = substr.c_str();

    // Clear screen
    SDL_SetRenderDrawColor(state->renderer, msg.bg_color.r, msg.bg_color.g, msg.bg_color.b, msg.bg_color.a);
    SDL_RenderClear(state->renderer);

    // Draw text
    TTF_SetFontSize(state->font, msg.font_size);
    TTF_SetFontOutline(state->font, 0);
    TTF_Text *text = TTF_CreateText(state->text_engine, state->font, display_msg,
                                    state->current_message_display_utf8_length + 1);
    TTF_SetTextColor(text, msg.text_color.r, msg.text_color.g, msg.text_color.b, msg.text_color.a);
    TTF_DrawRendererText(text, 20, 20);
    TTF_DestroyText(text);

    if (msg.has_outline) {
        TTF_SetFontOutline(state->font, 1);
        TTF_Text *text_outline = TTF_CreateText(state->text_engine, state->font, display_msg,
                                                state->current_message_display_utf8_length + 1);
        TTF_SetTextColor(text_outline, 0, 0, 0, SDL_ALPHA_OPAQUE);
        TTF_DrawRendererText(text_outline, 18, 18);
        TTF_DestroyText(text_outline);
    }

    SDL_RenderPresent(state->renderer);

    if (state->current_message_display_utf8_length < msg.utf8_msg_length) {
        if (state->frame_count % 2 == 0) {
            if (is_line_feed) {
                if (state->frame_count % 6 == 0) {
                    Mix_PlayChannel(-1, state->sfx.line_feed, 0);
                    state->current_message_display_utf8_length++;
                }
            } else {
                state->current_message_display_utf8_length++;
                if (state->frame_count % 6 == 0) {
                    Mix_PlayChannel(-1, state->sfx.count_sound, 0);
                    state->current_message_display_utf8_length++;
                }
            }
        }
    } else {
        state->current_message_ended = true;
    }
}

bool is_countdown_step(launch_state launch_state) {
    return launch_state == FIRST_LAUNCH || launch_state == VBS_LAUNCH;
}
