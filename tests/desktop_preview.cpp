// Executes the production renderer and controller against a synthetic catalog.
// No network access or actual file downloads. Requires desktop SDL2 + extensions.
#define SDL_MAIN_HANDLED
#include "app.hpp"
#include "themes.h"
#include <SDL2/SDL_image.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
namespace rd {
App app;
std::atomic<bool> cancelWork{false}, pauseWork{false};
bool downloadStarted = false;
std::string keyboardValue;
void notify(const std::string &s) {
    app.toast = s;
}
void go(Screen s) {
    app.screen = s;
    app.cursor = app.offset = 0;
    app.focusY = 0;
    app.transition = 0;
}
void rebuild() {
    app.visible = filterItems(app.categories[app.category], app.query, app.settings.favorites,
                              app.favoritesOnly, app.descending);
    app.cursor = std::clamp(app.cursor, 0, std::max(0, (int)app.visible.size() - 1));
    app.offset = 0;
}
void saveSettings() {}
void listFolders() {
    app.folders = {"roms", "switch"};
    app.cursor = app.offset = 0;
}
void rememberPath() {}
std::string keyboard(const char *, const std::string &) {
    return keyboardValue;
}
uint64_t freeBytes() {
    return 35ULL * 1024 * 1024 * 1024;
}
void startCatalog(bool) {}
void startDownloads(bool) {
    downloadStarted = true;
    go(Screen::Transfer);
}
} // namespace rd
using namespace rd;
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                                                  \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)
int main() {
    SDL_SetMainReady();
    SDL_Init(0);
    TTF_Init();
    IMG_Init(IMG_INIT_PNG);
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 1280, 720, 32, SDL_PIXELFORMAT_RGBA32);
    app.renderer = SDL_CreateSoftwareRenderer(surface);
    app.small = TTF_OpenFont("romfs/reading.ttf", 20);
    app.body = TTF_OpenFont("romfs/reading.ttf", 24);
    app.title = TTF_OpenFont("romfs/heading.ttf", 38);
    CHECK(app.renderer && app.small && app.body && app.title);
    app.settings.sound = false;
    app.settings.motion = false;
    for (auto name : {"Game Boy Advance", "Nintendo DS", "Super Nintendo", "Game Boy Color", "Music", "eBook",
                      "Arcade", "Homebrew"}) {
        Category c;
        c.name = name;
        for (int i = 0; i < 20; ++i)
            c.items.push_back(
                {"Sample adventure " + std::to_string(i + 1) + ".zip", name, "https://example.org/file.zip"});
        app.categories.push_back(c);
    }
    applyTheme(6);
    theme(6);
    render();
    CHECK(IMG_SavePNG(surface, "build/preview-full.png") == 0);
    app.compact = true;
    render();
    CHECK(IMG_SavePNG(surface, "build/preview-applet.png") == 0);
    input(HidNpadButton_A, 0);
    CHECK(app.screen == Screen::Library);
    CHECK(app.visible.size() == 20);
    input(HidNpadButton_A, 0);
    CHECK(app.selected.size() == 1);
    input(HidNpadButton_StickR, 0);
    CHECK(app.settings.favorites.size() == 1);
    input(HidNpadButton_L, 0);
    CHECK(app.visible.size() == 1);
    input(HidNpadButton_L, 0);
    CHECK(app.visible.size() == 20);
    keyboardValue = "adventure 20";
    input(HidNpadButton_Y, 0);
    CHECK(app.visible.size() == 1);
    input(HidNpadButton_Minus, 0);
    CHECK(app.selected.size() == 2);
    input(HidNpadButton_Minus, 0);
    CHECK(app.selected.size() == 1);
    keyboardValue = "";
    input(HidNpadButton_Y, 0);
    app.compact = false;
    render();
    CHECK(IMG_SavePNG(surface, "build/preview-library.png") == 0);
    input(HidNpadButton_ZR, 0);
    CHECK(app.cursor == 7);
    input(HidNpadButton_Plus, 0);
    CHECK(app.screen == Screen::Settings);
    input(HidNpadButton_B, 0);
    CHECK(app.screen == Screen::Library && app.cursor == 7);
    int priorTheme = app.settings.theme;
    input(HidNpadButton_ZR, HidNpadButton_ZL | HidNpadButton_ZR);
    CHECK(app.settings.theme == (priorTheme + 1) % NUM_THEMES && app.cursor == 7);
    input(HidNpadButton_X, 0);
    CHECK(app.screen == Screen::Folder);
    CHECK(app.jobs.size() == 1);
    // Use a host directory solely for the folder existence check.
    app.path = "./";
    input(HidNpadButton_X, 0);
    CHECK(app.screen == Screen::Review);
    input(HidNpadButton_Y, 0);
    CHECK(app.settings.extract);
    CHECK(!app.settings.deleteOriginal);
    input(HidNpadButton_Y, 0);
    CHECK(app.settings.extract && app.settings.deleteOriginal);
    input(HidNpadButton_Y, 0);
    CHECK(!app.settings.extract && !app.settings.deleteOriginal);
    input(HidNpadButton_Plus, 0);
    app.cursor = 3;
    input(HidNpadButton_A, 0);
    CHECK(app.settings.extract && !app.settings.deleteOriginal);
    input(HidNpadButton_A, 0);
    CHECK(app.settings.extract && app.settings.deleteOriginal);
    input(HidNpadButton_B, 0);
    CHECK(app.screen == Screen::Review);
    input(HidNpadButton_A, 0);
    CHECK(downloadStarted && app.screen == Screen::Transfer);
    input(HidNpadButton_A, 0);
    CHECK(pauseWork);
    input(HidNpadButton_B, 0);
    CHECK(app.screen == Screen::Cancel);
    input(HidNpadButton_B, 0);
    CHECK(app.screen == Screen::Transfer && pauseWork);
    input(HidNpadButton_A, 0);
    CHECK(!pauseWork);
    input(HidNpadButton_B, 0);
    input(HidNpadButton_A, 0);
    CHECK(cancelWork && !pauseWork);
    app.progress = {"Sample adventure 1.zip",
                    "Receiving data",
                    37 * 1024 * 1024,
                    100 * 1024 * 1024,
                    2100000,
                    1,
                    4,
                    false};
    applyTheme(8);
    theme(8);
    render();
    CHECK(IMG_SavePNG(surface, "build/preview-transfer.png") == 0);
    app.screen = Screen::Settings;
    render();
    CHECK(IMG_SavePNG(surface, "build/preview-settings.png") == 0);
    for (int i = 0; i < NUM_THEMES; ++i) {
        theme(i);
        for (auto s : {Screen::Shelf, Screen::Library, Screen::Review, Screen::Transfer, Screen::Results,
                       Screen::Settings}) {
            app.screen = s;
            app.cursor = 0;
            render();
        }
    }
    clearUi();
    SDL_DestroyRenderer(app.renderer);
    SDL_FreeSurface(surface);
    TTF_CloseFont(app.small);
    TTF_CloseFont(app.body);
    TTF_CloseFont(app.title);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    puts("PASS: production navigation, queue selection, favorites, search, pause/cancel, all 12 theme render "
         "smoke checks");
}
