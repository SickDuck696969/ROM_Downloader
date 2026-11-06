#pragma once
#include "model.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#include <atomic>
#include <switch.h>
namespace rd {
constexpr const char *config = "sdmc:/switch/ROM_Downloader/";
enum class Screen { Shelf, Library, Folder, History, Review, Transfer, Results, Settings, Cancel };
enum class Status { Waiting, Active, Saved, Skipped, Failed, Cancelled };
struct Job {
    Item item;
    Status status = Status::Waiting;
    std::string detail;
};
struct Progress {
    std::string title, detail;
    uint64_t now = 0, total = 0;
    double speed = 0;
    int index = 0, count = 0;
    bool extracting = false;
};
struct Settings {
    int theme = 0;
    bool motion = true, sound = true, extract = false, deleteOriginal = false;
    void cycleExtraction() {
        if (!extract) {
            extract = true;
            deleteOriginal = false;
        } else if (!deleteOriginal)
            deleteOriginal = true;
        else {
            extract = false;
            deleteOriginal = false;
        }
    }
    std::string extractionLabel() const {
        return !extract ? "Off" : deleteOriginal ? "Extract + delete ZIP" : "Extract + keep ZIP";
    }
    std::string path = "sdmc:/";
    std::set<std::string> favorites;
    std::vector<std::string> history;
};
struct App {
    SDL_Renderer *renderer = nullptr;
    TTF_Font *small = nullptr, *body = nullptr, *title = nullptr;
    Mix_Chunk *nav = nullptr, *click = nullptr, *complete = nullptr;
    bool compact = false, running = true, loading = false, favoritesOnly = false, descending = false;
    Screen screen = Screen::Shelf, returnScreen = Screen::Shelf;
    Settings settings;
    std::vector<Category> categories;
    std::vector<int> visible;
    std::set<std::string> selected;
    std::vector<Job> jobs;
    std::vector<std::string> folders;
    int shelfTab = 0, shelfPositions[2]{};
    int category = 0, cursor = 0, offset = 0, shelfCursor = 0, libraryCursor = 0;
    std::string query, path = "sdmc:/", toast;
    uint64_t toastUntil = 0, transition = 0;
    float focusY = 0;
    Progress progress;
};
extern App app;
extern std::atomic<bool> cancelWork, pauseWork;
void go(Screen screen);
void notify(const std::string &text);
void rebuild();
void input(u64 down, u64 held);
void render();
void clearUi();
void loadSettings();
void saveSettings();
void listFolders();
void rememberPath();
std::string keyboard(const char *prompt, const std::string &initial = "");
uint64_t freeBytes();
void startCatalog(bool refresh);
void startDownloads(bool retry = false);
void pollWorker();
void stopWorker();
void rect(int x, int y, int w, int h, SDL_Color color);
void text(const std::string &value, int x, int y, int width, SDL_Color color, TTF_Font *font = nullptr);
void theme(int index);
void animateTheme();
} // namespace rd
