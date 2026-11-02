#include "app.hpp"
#include "themes.h"
#include <SDL2/SDL_image.h>
#include <curl/curl.h>
using namespace rd;
int main(int, char **) {
    romfsInit();
    socketInitializeDefault();
    curl_global_init(CURL_GLOBAL_DEFAULT);
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    TTF_Init();
    IMG_Init(IMG_INIT_PNG);
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
    auto *window = SDL_CreateWindow("ROM Studio", 0, 0, 1280, 720, 0);
    app.renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    app.small = TTF_OpenFont("romfs:/reading.ttf", 20);
    app.body = TTF_OpenFont("romfs:/reading.ttf", 24);
    app.title = TTF_OpenFont("romfs:/heading.ttf", 38);
    if (!app.renderer || !app.small || !app.body || !app.title)
        return 1;
    app.nav = Mix_LoadWAV("romfs:/nav.wav");
    app.click = Mix_LoadWAV("romfs:/click.wav");
    app.complete = Mix_LoadWAV("romfs:/comp.wav");
    auto type = appletGetAppletType();
    app.compact = type != AppletType_Application && type != AppletType_SystemApplication;
    loadSettings();
    applyTheme(app.settings.theme);
    theme(app.settings.theme);
    startCatalog(false);
    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    u64 repeating = 0;
    uint64_t repeatAt = 0;
    const u64 navigation = HidNpadButton_Up | HidNpadButton_Down | HidNpadButton_Left | HidNpadButton_Right |
                           HidNpadButton_StickLUp | HidNpadButton_StickLDown | HidNpadButton_StickLLeft |
                           HidNpadButton_StickLRight;
    while (appletMainLoop() && app.running) {
        padUpdate(&pad);
        u64 down = padGetButtonsDown(&pad), held = padGetButtons(&pad);
        uint64_t now = SDL_GetTicks64();
        if (down & navigation) {
            repeating = down & navigation;
            repeatAt = now + 340;
        } else if (repeating && (held & repeating) && now >= repeatAt) {
            down |= repeating;
            repeatAt = now + 85;
        } else if (!(held & repeating))
            repeating = 0;
        SDL_Event event;
        while (SDL_PollEvent(&event))
            if (event.type == SDL_QUIT)
                app.running = false;
        input(down, held);
        pollWorker();
        render();
    }
    stopWorker();
    clearUi();
    saveSettings();
    if (app.nav)
        Mix_FreeChunk(app.nav);
    if (app.click)
        Mix_FreeChunk(app.click);
    if (app.complete)
        Mix_FreeChunk(app.complete);
    Mix_CloseAudio();
    TTF_CloseFont(app.small);
    TTF_CloseFont(app.body);
    TTF_CloseFont(app.title);
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    curl_global_cleanup();
    socketExit();
    romfsExit();
    return 0;
}
