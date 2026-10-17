#define SDL_MAIN_USE_CALLBACKS 1

#define CIH_FILENAME "CIH.txt"
#define ARGV_ZIP_FILENAME "ARGV.zip"
#define ARGV_FILENAME "ARGV.txt"

#include "effects.h"
#include "infoprovider.h"
#include "stringutils.h"
#include <algorithm>
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
    MIX_Audio *count_sound;
    MIX_Audio *explosion_sound;
    MIX_Audio *error_sound;
    MIX_Audio *line_feed;
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
    MIX_Audio *music = nullptr;
    MIX_Track *track = nullptr;
    MIX_Mixer* mixer = nullptr;
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
    // visual effects
    fx::Effects fx;
    int last_countdown_second = -1;
    float countdown_pulse = 0.f; // 1 right after a tick, then decays to 0
    Uint64 explosion_start_ticks = 0;
    Uint64 explosion_finished_ticks = 0;
    int last_shown_len = 0;
    int last_message_num = -1;
    struct idiot_ghost {
        SDL_FRect rect;
        float angle;
        float hue;
    };
    std::vector<idiot_ghost> idiot_trail;
    float idiot_hue = 0.f;
};

bool is_countdown_step(launch_state launch_state);

message create_message(const wchar_t *text, SDL_Color text_color, SDL_Color bg_color, bool has_outline,
                       float font_size);

message create_message(const wchar_t *text, SDL_Color text_color, SDL_Color bg_color, bool has_outline, float font_size,
                       std::function<void()> end_callback);

void set_messages(app_state *state, const std::vector<message> &messages);

void show_message(app_state *state);

void play_music(app_state *state);

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

    if (!MIX_Init()) {
        InfoProvider::on_critical_SDL_error(nullptr, "Couldn't initialize SDL3_mixer");
        return SDL_APP_FAILURE;
    }

    SDL_AudioSpec spec = { .format = SDL_AUDIO_F32, .channels = 2, .freq = 48000 };
    MIX_Mixer *mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (!mixer) {
        InfoProvider::on_critical_SDL_error(nullptr, "Couldn't create mixer device with SDL3_mixer");
        return SDL_APP_FAILURE;
    }

    MIX_Track* track = MIX_CreateTrack(mixer);

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
    state.mixer = mixer;
    state.track = track;

    if (!state.fx.init(renderer, display_mode->w, display_mode->h)) {
        InfoProvider::on_critical_SDL_error(window, "Couldn't create effects textures");
        return SDL_APP_FAILURE;
    }

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
    state.sfx.count_sound = MIX_LoadAudio(mixer, "resources/sfx/count.wav", false);
    if (state.sfx.count_sound == nullptr) {
        InfoProvider::on_critical_SDL_error(window, "Couldn't load audio");
        return SDL_APP_FAILURE;
    }
    state.sfx.explosion_sound = MIX_LoadAudio(mixer, "resources/sfx/explosion.wav", false);
    if (state.sfx.explosion_sound == nullptr) {
        InfoProvider::on_critical_SDL_error(window, "Couldn't load audio");
        return SDL_APP_FAILURE;
    }
    state.sfx.error_sound = MIX_LoadAudio(mixer, "resources/sfx/error.wav", false);
    if (state.sfx.error_sound == nullptr) {
        InfoProvider::on_critical_SDL_error(window, "Couldn't load audio");
        return SDL_APP_FAILURE;
    }

    state.sfx.line_feed = MIX_LoadAudio(mixer, "resources/sfx/linefeed.wav", false);
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
                    state->fx.flash(0.2f);
                    state->fx.add_shake(0.25f);

                    if (state->current_message_num >= state->messages.size()) {
                        state->is_in_message = false;
                    }
                } else {
                    state->current_message_display_utf8_length = state->messages[state->current_message_num].
                            utf8_msg_length;
                    state->current_message_ended = true;
                    state->fx.add_shake(0.12f);
                }
            }
            break;
        case SDL_EVENT_KEY_UP:
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (state->is_in_you_are_an_idiot_phase) {
                if (event->button.button == SDL_BUTTON_LEFT) {
                    const float click_x = event->button.x;
                    const float click_y = event->button.y;
                    const bool hit = click_x >= state->window_idiot_coordinates.x
                                     && click_x < state->window_idiot_coordinates.x + state->window_idiot_coordinates.w
                                     && click_y >= state->window_idiot_coordinates.y
                                     && click_y < state->window_idiot_coordinates.y + state->window_idiot_coordinates.h;
                    if (hit) {
                        const SDL_Color hit_color = fx::hsv(state->idiot_hue, 0.8f, 1.f);
                        state->fx.sparks(click_x, click_y, fx::rgb(255, 255, 255), hit_color, 36, 800.f);
                        state->fx.ring(click_x, click_y, 8.f, 260.f, 0.55f, hit_color);
                        state->fx.ring(click_x, click_y, 8.f, 140.f, 0.35f, fx::rgb(255, 255, 255), 0.8f);
                        state->fx.add_shake(0.5f);
                        state->fx.flash(0.15f, hit_color);
                    } else {
                        state->fx.sparks(click_x, click_y, fx::rgb(200, 200, 200), fx::rgb(90, 90, 90), 8, 260.f);
                        state->fx.ring(click_x, click_y, 4.f, 70.f, 0.35f, fx::rgb(200, 200, 200), 0.6f);
                    }
                    if (event->button.x >= state->window_idiot_coordinates.x && event->button.x < state->
                        window_idiot_coordinates.x + state->window_idiot_coordinates.w
                        && event->button.y >= state->window_idiot_coordinates.y && event->button.y < state->
                        window_idiot_coordinates.y + state->window_idiot_coordinates.h) {
                        state->window_idiot_coordinates.w /= 1.5;
                        state->window_idiot_coordinates.h /= 1.5;
                        if (state->window_idiot_coordinates.w <= 100) {
                            state->is_in_you_are_an_idiot_phase = false;
                            state->idiot_trail.clear();
                            const float death_x = state->window_idiot_coordinates.x + state->window_idiot_coordinates.w / 2;
                            const float death_y = state->window_idiot_coordinates.y + state->window_idiot_coordinates.h / 2;
                            state->fx.confetti(death_x, death_y, 220);
                            state->fx.flash(0.7f);
                            state->fx.add_shake(1.0f);

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
    MIX_PlayAudio(state->mixer, state->sfx.count_sound);
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
    // Effects use real time; clamp so a blocking message box doesn't make them jump
    const float dt = static_cast<float>(std::min<Uint64>(ticks_passed, 50)) / 1000.f;
    state->fx.update(dt);

    if (state->lifecycle == BEGIN) {
        if (is_countdown_step(state->launch_state)) {
            if (state->launch_state == VBS_LAUNCH) {
                for (int i = 0; i < 15; i++) {
                    MIX_PlayAudio(state->mixer, state->sfx.error_sound);
                    SDL_Delay(100);
                }
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "The Desktop Trojan", "You're really an idiot!",
                                         state->window);
            }

           MIX_PlayAudio(state->mixer, state->sfx.error_sound);
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
            state->music = MIX_LoadAudio(state->mixer, "resources/sfx/jenova.mp3", false);
            if (state->music == nullptr) {
                InfoProvider::on_critical_SDL_error(state->window, "Couldn't load music");
                return SDL_APP_FAILURE;
            }
            play_music(state);
            std::vector<message> messages;
            messages.push_back(create_message(
                (L"?????: Hello " + std::wstring(state->username) +
                 L"!\n\nIf you managed to get\nhere, you might not\nbe as idiot as the\nboss thinks...\nMy name is Alice.\n\n(Press SPACE to continue)")
                .c_str(), {.r = 0, .g = 160, .b = 0, .a = 255}, {.r = 0, .g = 0, .b = 0, .a = 0}, true, 80,
                [state] { MIX_StopTrack(state->track, 10000); }));
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
            state->music = MIX_LoadAudio(state->mixer, "resources/sfx/jenova.mp3", false);
            if (state->music == nullptr) {
                InfoProvider::on_critical_SDL_error(state->window, "Couldn't load music");
                return SDL_APP_FAILURE;
            }
            play_music(state);
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

                const float center_x = static_cast<float>(state->window_width) / 2.f;
                const float center_y = static_cast<float>(state->window_height) / 2.f;

                Sint64 remaining_time_ms = state->end_countdown_ticks - SDL_GetTicks();
                int remaining_time = remaining_time_ms / 1000;
                // 0 at the start of the countdown, 1 when the bomb is about to blow
                const float urgency = fx::clamp01(
                    1.f - static_cast<float>(remaining_time_ms) / static_cast<float>(state->countdown_ms));

                // Every second: heartbeat of the bomb
                if (remaining_time != state->last_countdown_second) {
                    state->last_countdown_second = remaining_time;
                    state->countdown_pulse = 1.f;
                    const SDL_Color tick_color = fx::lerp(fx::rgb(255, 140, 40), fx::rgb(255, 30, 30), urgency);
                    state->fx.ring(center_x, center_y, 120.f, 420.f + 340.f * urgency, 0.9f, tick_color, 0.8f);
                    state->fx.ring(center_x, center_y, 120.f, 300.f, 0.6f, fx::rgb(255, 255, 255), 0.4f);
                    state->fx.sparks(center_x, center_y, fx::rgb(255, 230, 150), tick_color,
                                     16 + static_cast<int>(40.f * urgency), 500.f + 700.f * urgency);
                    state->fx.add_shake(0.12f + 0.4f * urgency);
                    state->fx.flash(0.05f + 0.12f * urgency, tick_color);
                }
                state->countdown_pulse = std::max(0.f, state->countdown_pulse - dt * 2.8f);
                const float pulse = state->countdown_pulse * state->countdown_pulse;
                // the closer to zero, the more the whole screen trembles
                state->fx.keep_shake(urgency * urgency * 0.4f);

                // Embers drifting up from the bottom of the screen
                {
                    fx::Burst ember;
                    ember.count = 1 + static_cast<int>(urgency * 3.f);
                    ember.speed_min = 60;
                    ember.speed_max = 220;
                    ember.angle_min = -fx::PI * 0.62f;
                    ember.angle_max = -fx::PI * 0.38f;
                    ember.life_min = 2.f;
                    ember.life_max = 4.f;
                    ember.size_min = 5;
                    ember.size_max = 13;
                    ember.drag = 0.2f;
                    ember.alpha = 0.8f;
                    ember.c0 = fx::rgb(255, 190, 60);
                    ember.c1 = fx::rgb(200, 20, 0);
                    state->fx.burst(fx::rnd(0.f, static_cast<float>(state->window_width)),
                                    static_cast<float>(state->window_height) + 10.f, ember);
                }

                state->fx.begin_shake(state->renderer);

                state->fx.draw_rain(state->renderer, fx::rgb(255, 40, 20), 0.18f + 0.3f * urgency);

                // Halo behind the bomb
                state->fx.draw_glow(state->renderer, center_x, center_y, 330.f * (1.f + 0.2f * pulse),
                                    fx::lerp(fx::rgb(255, 120, 20), fx::rgb(255, 20, 20), urgency),
                                    0.25f + 0.2f * urgency + 0.4f * pulse);

                // Bomb: heartbeat scale + nervous wobble
                const float bomb_scale = 1.f + 0.12f * pulse + 0.03f * std::sin(state->fx.time() * 9.f) * urgency;
                const float bomb_angle = std::sin(state->fx.time() * 23.f) * 5.f * urgency * urgency;
                SDL_FRect dst_rect = {
                    .x = center_x - 200.f * bomb_scale,
                    .y = center_y - 200.f * bomb_scale,
                    .w = 400.f * bomb_scale,
                    .h = 400.f * bomb_scale
                };
                SDL_RenderTextureRotated(state->renderer, state->img.bomb, nullptr, &dst_rect, bomb_angle, nullptr,
                                         SDL_FLIP_NONE);

                // Sparks coming out of the fuse (tip of the fuse is at ~(0.79, 0.16) in the bomb image)
                {
                    const float fuse_x = center_x + 175.f * bomb_scale;
                    const float fuse_y = center_y - 137.f * bomb_scale;
                    fx::Burst fuse;
                    fuse.count = 2 + static_cast<int>(urgency * 3.f);
                    fuse.speed_min = 60;
                    fuse.speed_max = 320;
                    fuse.angle_min = -fx::PI * 0.95f;
                    fuse.angle_max = -fx::PI * 0.05f;
                    fuse.life_min = 0.2f;
                    fuse.life_max = 0.55f;
                    fuse.size_min = 6;
                    fuse.size_max = 11;
                    fuse.drag = 3.f;
                    fuse.gravity = 260;
                    fuse.shape = fx::Shape::Spark;
                    fuse.c0 = fx::rgb(255, 250, 200);
                    fuse.c1 = fx::rgb(255, 90, 0);
                    state->fx.burst(fuse_x, fuse_y, fuse);
                    state->fx.draw_glow(state->renderer, fuse_x, fuse_y, 40.f + 20.f * std::sin(state->fx.time() * 40.f),
                                        fx::rgb(255, 200, 80), 0.8f);
                }

                state->fx.draw(state->renderer);

                // Countdown number: pops on every tick and turns red
                TTF_SetFontSize(state->font, 200.f * (1.f + 0.35f * pulse));
                TTF_SetFontOutline(state->font, 0);
                TTF_Text *text = TTF_CreateText(state->text_engine, state->font, std::to_string(remaining_time).c_str(),
                                                0);

                int text_w;
                int text_h;
                TTF_GetTextSize(text, &text_w, &text_h);
                const float text_x = center_x - static_cast<float>(text_w) / 2.f;
                const float text_y = center_y - static_cast<float>(text_h) / 2.f;
                const SDL_Color number_color = fx::lerp(fx::rgb(255, 255, 255), fx::rgb(255, 70, 50), urgency);
                // glow
                const int glow_offsets[4][2] = {{-8, 0}, {8, 0}, {0, -8}, {0, 8}};
                TTF_SetTextColor(text, 255, 40, 0, 70);
                for (const auto &o: glow_offsets) {
                    TTF_DrawRendererText(text, text_x + static_cast<float>(o[0]), text_y + static_cast<float>(o[1]));
                }
                TTF_SetTextColor(text, number_color.r, number_color.g, number_color.b, SDL_ALPHA_OPAQUE);
                TTF_DrawRendererText(text, text_x, text_y);
                TTF_DestroyText(text);

                state->fx.end_shake(state->renderer);

                state->fx.draw_vignette(state->renderer, fx::rgb(150, 0, 0), 0.35f + 0.55f * urgency + 0.2f * pulse,
                                        240.f + 160.f * urgency);
                state->fx.draw_scanlines(state->renderer, fx::rgb(255, 60, 40), 0.12f, 0.05f);
                state->fx.draw_flash(state->renderer);

                SDL_RenderPresent(state->renderer);

                if (remaining_time_ms <= 0) {
                    state->is_in_explosion_phase = true;
                    state->explosion_coordinates.x = state->window_width / 2 - 10;
                    state->explosion_coordinates.y = state->window_height / 2 - 10;
                    state->explosion_coordinates.w = 20;
                    state->explosion_coordinates.h = 20;
                    state->explosion_start_ticks = SDL_GetTicks();
                    state->fx.explosion(center_x, center_y);
                    MIX_PlayAudio(state->mixer, state->sfx.explosion_sound);
                }
            } else {
                // Background fades out while the debris fly
                const float since_boom = static_cast<float>(SDL_GetTicks() - state->explosion_start_ticks);
                const int bg = static_cast<int>(static_cast<float>(state->countdown_bg_color_red) *
                                                (1.f - fx::clamp01(since_boom / 1200.f)));
                SDL_SetRenderDrawColor(state->renderer, bg, 0, 0, bg);
                SDL_RenderClear(state->renderer);

                state->fx.begin_shake(state->renderer);

                const float progress = fx::clamp01((state->explosion_coordinates.w - 20.f) / 980.f);
                const float center_x = static_cast<float>(state->window_width) / 2.f;
                const float center_y = static_cast<float>(state->window_height) / 2.f;
                state->fx.draw_glow(state->renderer, center_x, center_y, 200.f + 900.f * progress,
                                    fx::rgb(255, 150, 40), 0.55f * (1.f - progress));

                SDL_SetTextureAlphaMod(state->img.explosion,
                                       fx::to_u8(1.f - fx::smoothstep(0.45f, 1.f, progress)));
                SDL_RenderTextureRotated(state->renderer, state->img.explosion, nullptr, &state->explosion_coordinates,
                                         static_cast<double>(progress * 25.f), nullptr, SDL_FLIP_NONE);

                state->fx.draw(state->renderer);
                state->fx.end_shake(state->renderer);

                state->fx.draw_vignette(state->renderer, fx::rgb(0, 0, 0), 0.6f * progress, 300.f);
                state->fx.draw_flash(state->renderer);

                if (state->explosion_coordinates.w <= 1000) {
                    state->explosion_coordinates.x -= 10;
                    state->explosion_coordinates.w += 20;
                    state->explosion_coordinates.y -= 10;
                    state->explosion_coordinates.h += 20;
                } else if (state->explosion_finished_ticks == 0) {
                    state->explosion_finished_ticks = SDL_GetTicks();
                }

                SDL_RenderPresent(state->renderer);

                // Let the debris and shockwaves finish before closing
                if (state->explosion_finished_ticks != 0 && SDL_GetTicks() - state->explosion_finished_ticks > 1100) {
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
                SDL_CameraSpec **specs = SDL_GetCameraSupportedFormats(devices[0], &format_count);
                if (specs == nullptr || format_count == 0) {
                    InfoProvider::on_critical_SDL_error(state->window, "No camera format available");
                    return SDL_APP_FAILURE;
                }

                // Préférer un format simple à afficher, sinon retomber sur celui qui marchait (index 1, ou 0)
                int chosen = 0;
                for (int i = 0; i < format_count; i++) {
                    const SDL_PixelFormat f = specs[i]->format;
                    const bool simple = f == SDL_PIXELFORMAT_RGBA32 || f == SDL_PIXELFORMAT_BGRA32 ||
                                        f == SDL_PIXELFORMAT_ARGB32 || f == SDL_PIXELFORMAT_ABGR32 ||
                                        f == SDL_PIXELFORMAT_RGB24 || f == SDL_PIXELFORMAT_BGR24 ||
                                        f == SDL_PIXELFORMAT_XRGB8888 || f == SDL_PIXELFORMAT_YUY2;
                    if (simple) {
                        chosen = i;
                        break;
                    }
                }
                state->camera = SDL_OpenCamera(devices[0], specs[chosen]);
                if (state->camera == nullptr) {
                    InfoProvider::on_critical_SDL_error(state->window, "Couldn't open camera");
                    return SDL_APP_FAILURE;
                }
                SDL_free(specs);
                SDL_free(devices);

                state->music = MIX_LoadAudio(state->mixer, "resources/sfx/lavender.mp3", false);
                if (state->music == nullptr) {
                    InfoProvider::on_critical_SDL_error(state->window, "Couldn't load music");
                    return SDL_APP_FAILURE;
                }
                play_music(state);

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
                MIX_DestroyAudio(state->music);
                state->music = MIX_LoadAudio(state->mixer, "resources/sfx/idiot.mp3", false);
                if (state->music == nullptr) {
                    InfoProvider::on_critical_SDL_error(state->window, "Couldn't load music");
                    return SDL_APP_FAILURE;
                }

                play_music(state);

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

                if (frame) {
                    SDL_Surface *rgba = SDL_ConvertSurface(frame, SDL_PIXELFORMAT_RGBA32);
                    SDL_ReleaseCameraFrame(state->camera, frame);

                    if (rgba) {
                        if (state->camera_texture == nullptr) {
                            state->camera_texture = SDL_CreateTexture(state->renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, rgba->w, rgba->h);
                            state->camera_viewport.x = state->window_width - rgba->w / 2 - 100;
                            state->camera_viewport.y = 100;
                            state->camera_viewport.w = rgba->w / 2;
                            state->camera_viewport.h = rgba->h / 2;
                        }
                        if (state->camera_texture) {
                            SDL_UpdateTexture(state->camera_texture, nullptr, rgba->pixels, rgba->pitch);
                        }
                        SDL_DestroySurface(rgba);
                    }
                }

                SDL_SetRenderDrawColor(state->renderer, 0, 0, 0, SDL_ALPHA_TRANSPARENT);
                SDL_RenderClear(state->renderer);

                // Random glitches, and a nervous shake that never completely stops
                if (fx::rnd() < 0.02f) {
                    state->fx.trigger_glitch(fx::rnd(0.08f, 0.25f));
                    state->fx.add_shake(0.2f);
                }
                state->fx.keep_shake(0.05f);
                if (fx::rnd() < 0.3f) {
                    fx::Burst ember;
                    ember.count = 1;
                    ember.speed_min = 60;
                    ember.speed_max = 200;
                    ember.angle_min = -fx::PI * 0.62f;
                    ember.angle_max = -fx::PI * 0.38f;
                    ember.life_min = 2.f;
                    ember.life_max = 4.f;
                    ember.size_min = 5;
                    ember.size_max = 12;
                    ember.drag = 0.2f;
                    ember.alpha = 0.7f;
                    ember.c0 = fx::rgb(255, 60, 40);
                    ember.c1 = fx::rgb(120, 0, 0);
                    state->fx.burst(fx::rnd(0.f, static_cast<float>(state->window_width)),
                                    static_cast<float>(state->window_height) + 10.f, ember);
                }

                state->fx.begin_shake(state->renderer);
                state->fx.draw_rain(state->renderer, fx::rgb(255, 30, 30), 0.25f);

                if (state->camera_texture) {
                    const SDL_FRect &view = state->camera_viewport;
                    const float cam_pulse = 0.5f + 0.5f * std::sin(state->fx.time() * 5.f);
                    state->fx.draw_glow(state->renderer, view.x + view.w / 2.f, view.y + view.h / 2.f,
                                        std::max(view.w, view.h) * 0.85f, fx::rgb(255, 20, 20), 0.2f + 0.2f * cam_pulse);

                    // Chromatic aberration ghosts while glitching
                    if (state->fx.glitching()) {
                        const float split = fx::rnd(4.f, 14.f);
                        SDL_SetTextureAlphaMod(state->camera_texture, 120);
                        SDL_SetTextureColorMod(state->camera_texture, 255, 0, 0);
                        SDL_FRect ghost = view;
                        ghost.x -= split;
                        SDL_RenderTexture(state->renderer, state->camera_texture, nullptr, &ghost);
                        SDL_SetTextureColorMod(state->camera_texture, 0, 255, 255);
                        ghost.x = view.x + split;
                        SDL_RenderTexture(state->renderer, state->camera_texture, nullptr, &ghost);
                        SDL_SetTextureColorMod(state->camera_texture, 255, 255, 255);
                        SDL_SetTextureAlphaMod(state->camera_texture, SDL_ALPHA_OPAQUE);
                    }

                    /* draw the latest camera frame, if available. */
                    SDL_RenderTexture(state->renderer, state->camera_texture, nullptr, &state->camera_viewport);

                    // Scanning line sweeping over the picture
                    const float scan = std::fmod(state->fx.time() * 0.45f, 1.f);
                    const float scan_y = view.y + scan * view.h;
                    fx::Effects::draw_gradient_quad(state->renderer, view.x, scan_y - 50.f, view.x + view.w,
                                                    scan_y - 50.f, view.x + view.w, scan_y, view.x, scan_y,
                                                    fx::rgb(255, 60, 60), 0.f, 0.25f);
                    fx::Effects::fill_rect(state->renderer, SDL_FRect{view.x, scan_y - 1.f, view.w, 2.f},
                                           fx::rgb(255, 120, 120), 0.8f);

                    // Neon frame + corner brackets
                    for (int i = 3; i >= 1; i--) {
                        const float grow = static_cast<float>(i) * 3.f;
                        const SDL_FRect frame = {view.x - grow, view.y - grow, view.w + grow * 2.f, view.h + grow * 2.f};
                        SDL_SetRenderDrawBlendMode(state->renderer, SDL_BLENDMODE_BLEND);
                        SDL_SetRenderDrawColor(state->renderer, 255, 40, 40, fx::to_u8((0.15f + 0.25f * cam_pulse) / static_cast<float>(i)));
                        SDL_RenderRect(state->renderer, &frame);
                    }
                    const float bracket = 36.f;
                    const float bx0 = view.x - 12.f;
                    const float by0 = view.y - 12.f;
                    const float bx1 = view.x + view.w + 12.f;
                    const float by1 = view.y + view.h + 12.f;
                    SDL_SetRenderDrawBlendMode(state->renderer, SDL_BLENDMODE_BLEND);
                    SDL_SetRenderDrawColor(state->renderer, 255, 70, 70, 255);
                    for (int t = 0; t < 3; t++) {
                        const float o = static_cast<float>(t);
                        SDL_RenderLine(state->renderer, bx0 + o, by0, bx0 + o + bracket, by0);
                        SDL_RenderLine(state->renderer, bx0, by0 + o, bx0, by0 + o + bracket);
                        SDL_RenderLine(state->renderer, bx1 - o, by0, bx1 - o - bracket, by0);
                        SDL_RenderLine(state->renderer, bx1, by0 + o, bx1, by0 + o + bracket);
                        SDL_RenderLine(state->renderer, bx0 + o, by1, bx0 + o + bracket, by1);
                        SDL_RenderLine(state->renderer, bx0, by1 - o, bx0, by1 - o - bracket);
                        SDL_RenderLine(state->renderer, bx1 - o, by1, bx1 - o - bracket, by1);
                        SDL_RenderLine(state->renderer, bx1, by1 - o, bx1, by1 - o - bracket);
                    }

                    // Blinking REC dot
                    if (std::fmod(state->fx.time(), 1.f) < 0.65f) {
                        state->fx.draw_glow(state->renderer, view.x + 36.f, view.y + 36.f, 26.f, fx::rgb(255, 0, 0), 1.f);
                        state->fx.draw_glow(state->renderer, view.x + 36.f, view.y + 36.f, 11.f, fx::rgb(255, 200, 200), 1.f);
                    }
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

                SDL_SetRenderDrawColor(state->renderer, 128, 128, 128, SDL_ALPHA_OPAQUE);
                SDL_RenderRect(state->renderer, &state->camera_viewport);

                state->fx.draw(state->renderer);
                state->fx.draw_glitch_bars(state->renderer, fx::rgb(255, 40, 40));
                state->fx.end_shake(state->renderer);

                state->fx.draw_vignette(state->renderer, fx::rgb(160, 0, 0),
                                        0.35f + 0.15f * std::sin(state->fx.time() * 3.f), 220.f);
                state->fx.draw_scanlines(state->renderer, fx::rgb(255, 60, 60), 0.12f, 0.05f);
                state->fx.draw_flash(state->renderer);
                SDL_RenderPresent(state->renderer);
            } else {
                SDL_SetRenderDrawColor(state->renderer, 0, 0, 0, SDL_ALPHA_TRANSPARENT);
                SDL_RenderClear(state->renderer);

                bool bounced_x = false;
                bool bounced_y = false;
                state->window_idiot_coordinates.x += state->window_idiot_speed.x;
                if (state->window_idiot_coordinates.x + state->window_idiot_coordinates.w > state->window_width) {
                    state->window_idiot_coordinates.x = state->window_width - state->window_idiot_coordinates.w;
                    state->window_idiot_speed.x = -state->window_idiot_speed.x;
                    bounced_x = true;
                } else if (state->window_idiot_coordinates.x < 0) {
                    state->window_idiot_coordinates.x = 0;
                    state->window_idiot_speed.x = -state->window_idiot_speed.x;
                    bounced_x = true;
                }
                state->window_idiot_coordinates.y += state->window_idiot_speed.y;
                if (state->window_idiot_coordinates.y + state->window_idiot_coordinates.h >= state->window_height) {
                    state->window_idiot_coordinates.y = state->window_height - state->window_idiot_coordinates.h;
                    state->window_idiot_speed.y = -state->window_idiot_speed.y;
                    bounced_y = true;
                } else if (state->window_idiot_coordinates.y < 0) {
                    state->window_idiot_coordinates.y = 0;
                    state->window_idiot_speed.y = -state->window_idiot_speed.y;
                    bounced_y = true;
                }

                const SDL_FRect &idiot = state->window_idiot_coordinates;
                const float idiot_cx = idiot.x + idiot.w / 2.f;
                const float idiot_cy = idiot.y + idiot.h / 2.f;
                state->idiot_hue += dt * 0.25f;
                state->idiot_hue -= std::floor(state->idiot_hue);
                const SDL_Color hue_color = fx::hsv(state->idiot_hue, 0.85f, 1.f);
                const float tilt = std::sin(state->fx.time() * 3.f) * 8.f;

                // Impact on the walls
                if (bounced_x || bounced_y) {
                    const float hit_x = bounced_x ? (state->window_idiot_speed.x < 0 ? idiot.x + idiot.w : idiot.x) : idiot_cx;
                    const float hit_y = bounced_y ? (state->window_idiot_speed.y < 0 ? idiot.y + idiot.h : idiot.y) : idiot_cy;
                    state->fx.sparks(hit_x, hit_y, fx::rgb(255, 255, 255), hue_color, 26, 700.f);
                    state->fx.ring(hit_x, hit_y, 6.f, 220.f, 0.5f, hue_color, 0.9f);
                    state->fx.add_shake(0.3f);
                }

                // Rainbow trail of fading copies
                state->idiot_trail.push_back({idiot, tilt, state->idiot_hue});
                if (state->idiot_trail.size() > 14) {
                    state->idiot_trail.erase(state->idiot_trail.begin());
                }

                // Particles shed behind the box
                {
                    fx::Burst shed;
                    shed.count = 2;
                    shed.speed_min = 20;
                    shed.speed_max = 140;
                    shed.life_min = 0.5f;
                    shed.life_max = 1.1f;
                    shed.size_min = 10;
                    shed.size_max = 24;
                    shed.drag = 1.f;
                    shed.alpha = 0.6f;
                    shed.radius = std::min(idiot.w, idiot.h) / 2.f;
                    shed.rainbow = true;
                    state->fx.burst(idiot_cx, idiot_cy, shed);
                }

                state->fx.begin_shake(state->renderer);

                // Pulsing aura
                state->fx.draw_glow(state->renderer, idiot_cx, idiot_cy,
                                    std::max(idiot.w, idiot.h) * (0.8f + 0.08f * std::sin(state->fx.time() * 6.f)),
                                    hue_color, 0.35f);

                const size_t ghost_count = state->idiot_trail.size();
                for (size_t i = 0; i + 1 < ghost_count; i++) {
                    const auto &g = state->idiot_trail[i];
                    const float k = static_cast<float>(i + 1) / static_cast<float>(ghost_count);
                    const SDL_Color gc = fx::hsv(g.hue, 0.8f, 1.f);
                    SDL_SetTextureColorMod(state->img.idiot, gc.r, gc.g, gc.b);
                    SDL_SetTextureAlphaMod(state->img.idiot, fx::to_u8(0.22f * k * k));
                    SDL_RenderTextureRotated(state->renderer, state->img.idiot, nullptr, &g.rect, g.angle, nullptr,
                                             SDL_FLIP_NONE);
                }
                SDL_SetTextureColorMod(state->img.idiot, 255, 255, 255);
                SDL_SetTextureAlphaMod(state->img.idiot, SDL_ALPHA_OPAQUE);
                SDL_RenderTextureRotated(state->renderer, state->img.idiot, nullptr, &state->window_idiot_coordinates,
                                         tilt, nullptr, SDL_FLIP_NONE);

                state->fx.draw(state->renderer);
                state->fx.end_shake(state->renderer);

                state->fx.draw_vignette(state->renderer, hue_color, 0.28f, 200.f);
                state->fx.draw_flash(state->renderer);

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

    state->fx.destroy();

    SDL_DestroyWindow(state->window);
    SDL_DestroyRenderer(state->renderer);
    TTF_DestroyRendererTextEngine(state->text_engine);

    // Free textures/images
    SDL_DestroyTexture(state->img.bomb);
    SDL_DestroyTexture(state->img.explosion);
    SDL_DestroyTexture(state->img.idiot);
    SDL_DestroySurface(state->img.icon);

    // Free sounds
    MIX_StopTrack(state->track, 0);
    MIX_DestroyAudio(state->sfx.count_sound);
    MIX_DestroyAudio(state->sfx.error_sound);
    MIX_DestroyAudio(state->sfx.explosion_sound);
    MIX_DestroyAudio(state->sfx.line_feed);
    MIX_Quit();

    TTF_CloseFont(state->font);
    TTF_Quit();

    SDL_Quit();
}


void show_message(app_state *state) {
    message msg = state->messages[state->current_message_num];
    std::string substr = StringUtils::get_utf8_substr(msg.str, 0, state->current_message_display_utf8_length);
    bool is_line_feed = substr[substr.length() - 1] == '\n';
    const std::string typed_str = substr; // without the blinking cursor
    if (!state->current_message_ended) {
        substr += '_';
    }
    const char *display_msg = substr.c_str();

    // Red text = the villain, which gets a more aggressive look than Alice (green) or the narrator (white)
    const bool menacing = msg.text_color.r > 150 && msg.text_color.g < 100;
    const SDL_Color text_color = msg.text_color;

    // New message: slam + glitch
    if (state->current_message_num != state->last_message_num) {
        state->last_message_num = state->current_message_num;
        state->last_shown_len = 0;
        if (menacing) {
            state->fx.trigger_glitch(0.45f);
            state->fx.add_shake(0.5f);
            state->fx.flash(0.25f, fx::rgb(255, 0, 0));
        }
    }
    if (fx::rnd() < (menacing ? 0.02f : 0.004f)) {
        state->fx.trigger_glitch(fx::rnd(0.08f, 0.2f));
    }

    // Clear screen
    SDL_SetRenderDrawColor(state->renderer, msg.bg_color.r, msg.bg_color.g, msg.bg_color.b, msg.bg_color.a);
    SDL_RenderClear(state->renderer);

    state->fx.begin_shake(state->renderer);

    // Digital rain behind the text
    state->fx.draw_rain(state->renderer, text_color, menacing ? 0.5f : 0.38f);

    // Draw text
    TTF_SetFontSize(state->font, msg.font_size);
    TTF_SetFontOutline(state->font, 0);
    TTF_Text *text = TTF_CreateText(state->text_engine, state->font, display_msg,
                                    state->current_message_display_utf8_length + 1);

    float text_x = 20.f;
    const float text_y = 20.f;
    if (state->fx.glitching()) {
        text_x += fx::rnd(-7.f, 7.f);
        // RGB split
        const float split = fx::rnd(3.f, 9.f);
        TTF_SetTextColor(text, 255, 0, 60, 170);
        TTF_DrawRendererText(text, text_x - split, text_y);
        TTF_SetTextColor(text, 0, 255, 255, 170);
        TTF_DrawRendererText(text, text_x + split, text_y);
    }

    // Soft halo around the letters
    const float halo = std::max(2.f, msg.font_size / 22.f);
    const float halo_offsets[4][2] = {{-halo, 0.f}, {halo, 0.f}, {0.f, -halo}, {0.f, halo}};
    TTF_SetTextColor(text, text_color.r, text_color.g, text_color.b, 60);
    for (const auto &o: halo_offsets) {
        TTF_DrawRendererText(text, text_x + o[0], text_y + o[1]);
    }

    TTF_SetTextColor(text, text_color.r, text_color.g, text_color.b, text_color.a);
    TTF_DrawRendererText(text, text_x, text_y);
    TTF_DestroyText(text);

    if (msg.has_outline) {
        TTF_SetFontOutline(state->font, 1);
        TTF_Text *text_outline = TTF_CreateText(state->text_engine, state->font, display_msg,
                                                state->current_message_display_utf8_length + 1);
        TTF_SetTextColor(text_outline, 0, 0, 0, SDL_ALPHA_OPAQUE);
        TTF_DrawRendererText(text_outline, text_x - 2.f, text_y - 2.f);
        TTF_DestroyText(text_outline);
    }

    // Sparks and a glowing cursor at the end of the typed text
    {
        TTF_SetFontOutline(state->font, 0);
        const size_t last_newline = typed_str.rfind('\n');
        const std::string last_line = last_newline == std::string::npos ? typed_str : typed_str.substr(last_newline + 1);
        int line_w = 0;
        int line_h = 0;
        if (!last_line.empty()) {
            TTF_GetStringSize(state->font, last_line.c_str(), 0, &line_w, &line_h);
        }
        const int line_count = static_cast<int>(std::count(typed_str.begin(), typed_str.end(), '\n'));
        const float line_skip = static_cast<float>(TTF_GetFontLineSkip(state->font));
        const float cursor_x = text_x + static_cast<float>(line_w) + msg.font_size * 0.25f;
        const float cursor_y = text_y + (static_cast<float>(line_count) + 0.5f) * line_skip;

        if (!state->current_message_ended) {
            state->fx.draw_glow(state->renderer, cursor_x, cursor_y, msg.font_size * (0.7f + 0.15f * std::sin(state->fx.time() * 20.f)),
                                text_color, 0.45f);
            if (state->current_message_display_utf8_length != state->last_shown_len) {
                fx::Burst typing;
                typing.count = 3;
                typing.speed_min = 40;
                typing.speed_max = 220;
                typing.life_min = 0.3f;
                typing.life_max = 0.8f;
                typing.size_min = msg.font_size * 0.12f;
                typing.size_max = msg.font_size * 0.3f;
                typing.drag = 2.f;
                typing.gravity = 120;
                typing.alpha = 0.8f;
                typing.shape = fx::Shape::Spark;
                typing.c0 = fx::lerp(text_color, fx::rgb(255, 255, 255), 0.6f);
                typing.c1 = text_color;
                state->fx.burst(cursor_x, cursor_y, typing);
                if (menacing) {
                    state->fx.add_shake(0.04f);
                }
            }
        }
        state->last_shown_len = state->current_message_display_utf8_length;
    }

    state->fx.draw(state->renderer);
    state->fx.draw_glitch_bars(state->renderer, text_color);
    state->fx.end_shake(state->renderer);

    state->fx.draw_vignette(state->renderer, text_color, menacing ? 0.22f : 0.14f, 200.f);
    state->fx.draw_scanlines(state->renderer, text_color);
    state->fx.draw_flash(state->renderer);

    SDL_RenderPresent(state->renderer);

    if (state->current_message_display_utf8_length < msg.utf8_msg_length) {
        if (state->frame_count % 2 == 0) {
            if (is_line_feed) {
                if (state->frame_count % 6 == 0) {
                    MIX_PlayAudio(state->mixer, state->sfx.line_feed);
                    state->current_message_display_utf8_length++;
                }
            } else {
                state->current_message_display_utf8_length++;
                if (state->frame_count % 6 == 0) {
                    MIX_PlayAudio(state->mixer, state->sfx.count_sound);
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

void play_music(app_state *state) {
    MIX_SetTrackAudio(state->track, state->music);
    SDL_PropertiesID properties = SDL_CreateProperties();
    SDL_SetNumberProperty(properties, MIX_PROP_PLAY_LOOPS_NUMBER, -1);
    MIX_PlayTrack(state->track, properties);
}

