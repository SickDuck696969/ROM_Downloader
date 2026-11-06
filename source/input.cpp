#include "app.hpp"
#include "themes.h"
#include <algorithm>
#include <sys/stat.h>
namespace rd {
namespace {
bool priorPause = false;
int settingsCursor = 0, settingsOffset = 0;
void move(int delta, int count) {
    app.cursor = std::clamp(app.cursor + delta, 0, std::max(0, count - 1));
}
void parent() {
    if (app.path != "sdmc:/") {
        auto p = app.path.substr(0, app.path.size() - 1);
        auto slash = p.find_last_of('/');
        app.path = p.substr(0, slash + 1);
        listFolders();
    }
}
void settings() {
    app.returnScreen = app.screen;
    settingsCursor = app.cursor;
    settingsOffset = app.offset;
    go(Screen::Settings);
}
void backLibrary() {
    go(Screen::Library);
    app.cursor = app.libraryCursor;
    rebuild();
}
} // namespace
void input(u64 down, u64 held) {
    if (down && app.settings.sound && app.nav)
        Mix_PlayChannel(-1, app.nav, 0);
    if (app.loading) {
        if (down & HidNpadButton_B) {
            cancelWork = true;
            notify("Cancelling catalog request");
        }
        return;
    }
    if (app.screen == Screen::Cancel) {
        if (down & HidNpadButton_A) {
            cancelWork = true;
            pauseWork = false;
            go(Screen::Transfer);
        } else if (down & HidNpadButton_B) {
            pauseWork = priorPause;
            go(Screen::Transfer);
        }
        return;
    }
    if (app.screen == Screen::Transfer) {
        if (down & HidNpadButton_A)
            pauseWork = !pauseWork.load();
        if (down & HidNpadButton_B) {
            priorPause = pauseWork;
            pauseWork = true;
            go(Screen::Cancel);
        }
        return;
    }
    if (app.screen == Screen::Settings) {
        if (down & (HidNpadButton_Down | HidNpadButton_StickLDown))
            move(1, 6);
        if (down & (HidNpadButton_Up | HidNpadButton_StickLUp))
            move(-1, 6);
        if (down & HidNpadButton_B) {
            go(app.returnScreen);
            app.cursor = settingsCursor;
            app.offset = settingsOffset;
            return;
        }
        if (down & (HidNpadButton_A | HidNpadButton_Left | HidNpadButton_Right)) {
            switch (app.cursor) {
            case 0:
                theme((app.settings.theme + ((down & HidNpadButton_Left) ? NUM_THEMES - 1 : 1)) % NUM_THEMES);
                break;
            case 1:
                app.settings.motion = !app.settings.motion;
                break;
            case 2:
                app.settings.sound = !app.settings.sound;
                break;
            case 3:
                app.settings.cycleExtraction();
                break;
            case 4:
                go(Screen::Shelf);
                startCatalog(true);
                return;
            case 5:
                app.running = false;
                return;
            }
            saveSettings();
        }
        return;
    }
    if (down & HidNpadButton_Plus) {
        settings();
        return;
    }
    int delta = 0;
    if (down & (HidNpadButton_Up | HidNpadButton_StickLUp))
        --delta;
    if (down & (HidNpadButton_Down | HidNpadButton_StickLDown))
        ++delta;
    if (app.screen == Screen::Shelf) {
        if (down & (HidNpadButton_L | HidNpadButton_R)) {
            app.shelfPositions[app.shelfTab] = app.shelfCursor;
            app.shelfTab = 1 - app.shelfTab;
            app.shelfCursor = app.shelfPositions[app.shelfTab];
            app.offset = 0;
            app.focusY = 0;
        }
        auto systems = shelfCategories(app.categories, app.shelfTab);
        app.cursor = app.shelfCursor;
        move(delta * (app.compact ? 1 : shelfColumns), systems.size());
        if (!app.compact) {
            if (down & (HidNpadButton_Left | HidNpadButton_StickLLeft))
                move(-1, systems.size());
            if (down & (HidNpadButton_Right | HidNpadButton_StickLRight))
                move(1, systems.size());
        }
        app.shelfCursor = app.cursor;
        if (down & HidNpadButton_Y)
            startCatalog(true);
        if ((down & HidNpadButton_A) && !systems.empty()) {
            app.category = systems[app.cursor];
            app.query.clear();
            app.favoritesOnly = false;
            go(Screen::Library);
            rebuild();
        }
    } else if (app.screen == Screen::Library) {
        if (down & HidNpadButton_ZL)
            delta -= 7;
        if (down & HidNpadButton_ZR)
            delta += 7;
        move(delta, app.visible.size());
        if ((down & HidNpadButton_A) && !app.visible.empty()) {
            auto key = app.categories[app.category].items[app.visible[app.cursor]].key();
            if (!app.selected.erase(key))
                app.selected.insert(key);
        }
        if (down & HidNpadButton_Y) {
            app.query = keyboard("Search this collection", app.query);
            app.cursor = 0;
            rebuild();
        }
        if (down & HidNpadButton_L) {
            app.favoritesOnly = !app.favoritesOnly;
            app.cursor = 0;
            rebuild();
        }
        if (down & HidNpadButton_R) {
            app.descending = !app.descending;
            rebuild();
        }
        if ((down & HidNpadButton_StickR) && !app.visible.empty()) {
            auto key = app.categories[app.category].items[app.visible[app.cursor]].key();
            if (!app.settings.favorites.erase(key))
                app.settings.favorites.insert(key);
            saveSettings();
            rebuild();
        }
        if (down & HidNpadButton_Minus) {
            bool all = !app.visible.empty();
            for (int i : app.visible)
                if (!app.selected.count(app.categories[app.category].items[i].key()))
                    all = false;
            for (int i : app.visible) {
                auto key = app.categories[app.category].items[i].key();
                if (all)
                    app.selected.erase(key);
                else
                    app.selected.insert(key);
            }
            notify(all ? "Visible items deselected" : "Visible items selected");
        }
        if (down & HidNpadButton_X) {
            if (app.selected.empty() && !app.visible.empty())
                app.selected.insert(app.categories[app.category].items[app.visible[app.cursor]].key());
            app.jobs.clear();
            for (const auto &c : app.categories)
                for (const auto &item : c.items)
                    if (app.selected.count(item.key()))
                        app.jobs.push_back({item, Status::Waiting, {}});
            if (app.jobs.empty()) {
                notify("Select files with A first");
                return;
            }
            app.libraryCursor = app.cursor;
            go(Screen::Review);
        }
        if (down & HidNpadButton_B) {
            go(Screen::Shelf);
            app.cursor = app.shelfCursor;
        }
    } else if (app.screen == Screen::Folder) {
        move(delta, app.folders.size());
        if (down & HidNpadButton_L)
            parent();
        if ((down & HidNpadButton_A) && !app.folders.empty()) {
            app.path += app.folders[app.cursor] + "/";
            listFolders();
        }
        if (down & HidNpadButton_R) {
            auto n = keyboard("New folder name");
            if (safeName(n)) {
                if (mkdir((app.path + n).c_str(), 0777) == 0) {
                    app.path += n + "/";
                    listFolders();
                } else
                    notify("Could not create folder");
            } else if (!n.empty())
                notify("Invalid folder name");
        }
        if (down & HidNpadButton_Y)
            go(Screen::History);
        if (down & HidNpadButton_X) {
            struct stat st{};
            if (stat(app.path.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
                notify("Choose an existing folder");
                return;
            }
            go(Screen::Review);
        }
        if (down & HidNpadButton_B)
            go(Screen::Review);
    } else if (app.screen == Screen::History) {
        move(delta, app.settings.history.size());
        if ((down & HidNpadButton_A) && !app.settings.history.empty()) {
            app.path = app.settings.history[app.cursor];
            go(Screen::Folder);
            listFolders();
        }
        if (down & HidNpadButton_B) {
            go(Screen::Folder);
            listFolders();
        }
    } else if (app.screen == Screen::Review) {
        move(delta, app.jobs.size());
        if (down & HidNpadButton_Y) {
            app.settings.cycleExtraction();
            saveSettings();
        }
        if ((down & HidNpadButton_X) && !app.jobs.empty()) {
            app.selected.erase(app.jobs[app.cursor].item.key());
            app.jobs.erase(app.jobs.begin() + app.cursor);
            move(0, app.jobs.size());
        }
        if (down & HidNpadButton_B)
            backLibrary();
        if (down & HidNpadButton_R) {
            go(Screen::Folder);
            listFolders();
        }
        if ((down & HidNpadButton_A) && !app.jobs.empty()) {
            struct stat destination{};
            if (stat(app.path.c_str(), &destination) != 0 || !S_ISDIR(destination.st_mode)) {
                notify("Choose a destination folder");
                go(Screen::Folder);
                listFolders();
                return;
            }
            rememberPath();
            startDownloads();
        }
    } else if (app.screen == Screen::Results) {
        move(delta, app.jobs.size());
        if (down & HidNpadButton_X) {
            bool retry = false;
            for (auto &j : app.jobs)
                if (j.status == Status::Failed || j.status == Status::Cancelled)
                    retry = true;
            if (retry)
                startDownloads(true);
            else
                notify("No failed downloads to retry");
        }
        if (down & (HidNpadButton_A | HidNpadButton_B)) {
            for (const auto &j : app.jobs)
                if (j.status == Status::Saved || j.status == Status::Skipped)
                    app.selected.erase(j.item.key());
            backLibrary();
        }
    }
}
} // namespace rd
