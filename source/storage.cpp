#include "app.hpp"
#define JSON_NOEXCEPTION 1
#include "json.hpp"
#include <algorithm>
#include <dirent.h>
#include <fstream>
#include <sys/stat.h>
#include <sys/statvfs.h>
namespace rd {
App app;
void notify(const std::string &s) {
    app.toast = s;
    app.toastUntil = SDL_GetTicks64() + 3200;
}
void go(Screen s) {
    app.screen = s;
    app.cursor = 0;
    app.offset = 0;
    app.focusY = 0;
    app.transition = SDL_GetTicks64();
}
void rebuild() {
    if (app.category >= 0 && app.category < (int)app.categories.size())
        app.visible = filterItems(app.categories[app.category], app.query, app.settings.favorites,
                                  app.favoritesOnly, app.descending);
    else
        app.visible.clear();
    app.cursor = std::clamp(app.cursor, 0, std::max(0, (int)app.visible.size() - 1));
    app.offset = 0;
}
void loadSettings() {
    mkdir("sdmc:/switch", 0777);
    mkdir(config, 0777);
    std::ifstream f(std::string(config) + "studio.json");
    auto j = nlohmann::json::parse(f, nullptr, false);
    if (!j.is_object()) {
        std::ifstream backup(std::string(config) + "studio.json.bak");
        j = nlohmann::json::parse(backup, nullptr, false);
    }
    if (j.is_object()) {
        auto t = j.find("theme");
        if (t != j.end() && t->is_number_integer())
            app.settings.theme = std::clamp(t->get<int>(), 0, 11);
        for (auto p : {std::pair<const char *, bool *>("motion", &app.settings.motion),
                       {"sound", &app.settings.sound},
                       {"extract", &app.settings.extract},
                       {"deleteOriginal", &app.settings.deleteOriginal}}) {
            auto i = j.find(p.first);
            if (i != j.end() && i->is_boolean())
                *p.second = i->get<bool>();
        }
        if (j.contains("path") && j["path"].is_string()) {
            std::string p;
            if (normalizeSdPath(j["path"].get<std::string>(), p))
                app.settings.path = p;
        }
        if (j.contains("favorites") && j["favorites"].is_array())
            for (const auto &v : j["favorites"])
                if (v.is_string())
                    app.settings.favorites.insert(v.get<std::string>());
        if (j.contains("history") && j["history"].is_array())
            for (const auto &v : j["history"]) {
                std::string p;
                if (v.is_string() && app.settings.history.size() < 12 &&
                    normalizeSdPath(v.get<std::string>(), p))
                    app.settings.history.push_back(p);
            }
    } else {
        std::ifstream old(std::string(config) + "settings.txt");
        int sound = 1, themeIndex = 0;
        if (old >> sound >> themeIndex) {
            app.settings.sound = sound != 0;
            app.settings.theme = std::clamp(themeIndex, 0, 11);
        }
        std::ifstream h(std::string(config) + "history.txt");
        std::string s;
        while (std::getline(h, s) && app.settings.history.size() < 12) {
            std::string p;
            if (normalizeSdPath(s, p))
                app.settings.history.push_back(p);
        }
    }
    app.path = app.settings.path;
}
void saveSettings() {
    nlohmann::json j = {{"theme", app.settings.theme},
                        {"motion", app.settings.motion},
                        {"sound", app.settings.sound},
                        {"extract", app.settings.extract},
                        {"deleteOriginal", app.settings.deleteOriginal},
                        {"path", app.settings.path},
                        {"favorites", app.settings.favorites},
                        {"history", app.settings.history}};
    std::string p = std::string(config) + "studio.json", tmp = p + ".tmp", backup = p + ".bak";
    std::ofstream f(tmp);
    f << j.dump(2);
    f.close();
    if (!f) {
        notify("Could not save settings");
        return;
    }
    remove(backup.c_str());
    rename(p.c_str(), backup.c_str());
    if (rename(tmp.c_str(), p.c_str()) != 0) {
        rename(backup.c_str(), p.c_str());
        notify("Could not save settings");
    }
}
void listFolders() {
    app.folders.clear();
    DIR *d = opendir(app.path.c_str());
    if (d) {
        while (auto e = readdir(d)) {
            std::string n = e->d_name;
            if (!safeName(n))
                continue;
            struct stat st{};
            if (lstat((app.path + n).c_str(), &st) == 0 && S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode))
                app.folders.push_back(n);
        }
        closedir(d);
    } else
        notify("Cannot open this folder");
    std::sort(app.folders.begin(), app.folders.end(),
              [](const std::string &a, const std::string &b) { return lower(a) < lower(b); });
    app.cursor = app.offset = 0;
}
void rememberPath() {
    auto &h = app.settings.history;
    h.erase(std::remove(h.begin(), h.end(), app.path), h.end());
    h.insert(h.begin(), app.path);
    if (h.size() > 12)
        h.resize(12);
    app.settings.path = app.path;
    saveSettings();
}
std::string keyboard(const char *prompt, const std::string &initial) {
    SwkbdConfig k;
    if (R_FAILED(swkbdCreate(&k, 0)))
        return initial;
    swkbdConfigMakePresetDefault(&k);
    swkbdConfigSetHeaderText(&k, prompt);
    swkbdConfigSetInitialText(&k, initial.c_str());
    swkbdConfigSetStringLenMax(&k, 200);
    char b[801]{};
    Result r = swkbdShow(&k, b, sizeof(b));
    swkbdClose(&k);
    return R_SUCCEEDED(r) ? b : initial;
}
uint64_t freeBytes() {
    struct statvfs st{};
    return statvfs(app.path.c_str(), &st) == 0 ? (uint64_t)st.f_bavail * st.f_frsize : 0;
}
} // namespace rd
