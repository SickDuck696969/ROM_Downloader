#include "app.hpp"
#include "themes.h"
#include <SDL2/SDL_image.h>
#include <algorithm>
#include <cmath>
#include <map>
namespace rd {
namespace {
struct Cached {
    SDL_Texture *texture;
    int w, h;
    uint64_t used;
};
std::map<std::string, Cached> labels;
std::map<std::string, SDL_Texture *> covers;
uint64_t serial = 0, themeStart = 0;
Theme previous = CONSOLE_THEMES[0];
float reveal = 1;
SDL_Color mix(SDL_Color a, SDL_Color b, float t) {
    return {(Uint8)(a.r + (b.r - a.r) * t), (Uint8)(a.g + (b.g - a.g) * t), (Uint8)(a.b + (b.b - a.b) * t),
            255};
}
void line(int x, int y, int w, SDL_Color color) {
    rect(x, y, w, 1, color);
}
void caption(const std::string &s, int x, int y, int w = 1100) {
    text(s, x, y, w, uiTextDim, app.small);
}
void twoLines(const std::string &value, int x, int y, int width) {
    int size = 0;
    TTF_SizeUTF8(app.body, value.c_str(), &size, nullptr);
    if (size <= width) {
        text(value, x, y, width, uiText);
        return;
    }
    size_t split = value.find_last_of(' ');
    while (split != std::string::npos) {
        TTF_SizeUTF8(app.body, value.substr(0, split).c_str(), &size, nullptr);
        if (size <= width)
            break;
        split = split ? value.find_last_of(' ', split - 1) : std::string::npos;
    }
    if (split == std::string::npos) {
        text(value, x, y, width, uiText);
        return;
    }
    text(value.substr(0, split), x, y, width, uiText);
    text(value.substr(split + 1), x, y + 28, width, uiText);
}
void pill(const std::string &s, int x, int y, int w) {
    rect(x, y, w, 30, uiPanelHov);
    text(s, x + 10, y + 4, w - 20, uiText, app.small);
}
void footer(const std::string &s, const std::string &second = "") {
    line(40, 647, 1200, uiBorder);
    text(s, 48, 659, 1184, uiText, app.small);
    if (!second.empty())
        text(second, 48, 688, 1184, uiTextDim, app.small);
}
void heading(const std::string &s, const std::string &detail) {
    text(s, 48, 100, 1180, uiText, app.title);
    caption(detail, 50, 156);
}
void cover(const std::string &name, int x, int y, int w, int h) {
    auto it = covers.find(name);
    if (it == covers.end()) {
        if (covers.size() >= shelfPageSize * 2) {
            for (auto &c : covers)
                if (c.second)
                    SDL_DestroyTexture(c.second);
            covers.clear();
        }
        auto *t = IMG_LoadTexture(app.renderer, (std::string(config) + "covers/" + name + ".png").c_str());
        it = covers.emplace(name, t).first;
    }
    if (it->second) {
        int sw = 0, sh = 0;
        SDL_QueryTexture(it->second, nullptr, nullptr, &sw, &sh);
        if (sw > 0 && sh > 0) {
            float scale = std::min((float)w / sw, (float)h / sh);
            SDL_Rect r{x + (w - (int)(sw * scale)) / 2, y + (h - (int)(sh * scale)) / 2, (int)(sw * scale),
                       (int)(sh * scale)};
            SDL_RenderCopy(app.renderer, it->second, nullptr, &r);
            return;
        }
    }
    const char *cart[] = {"01111110", "11000011", "11011011", "11011011",
                          "11000011", "11111111", "11011011", "01111110"};
    const char *handheld[] = {"01111110", "11000011", "11000011", "11000011",
                              "11111111", "10110101", "10011111", "01111110"};
    auto key = lower(name);
    bool portable = key.find("boy") != std::string::npos || key.find("ds") != std::string::npos ||
                    key.find("psp") != std::string::npos;
    int pixel = std::max(1, std::min(w, h) / 8);
    int ox = x + (w - pixel * 8) / 2, oy = y + (h - pixel * 8) / 2;
    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col)
            if ((portable ? handheld : cart)[row][col] == '1')
                rect(ox + col * pixel, oy + row * pixel, pixel, pixel, uiAccent);
}
void empty(const std::string &title, const std::string &detail) {
    rect(48, 230, 1184, 270, uiPanel);
    text(title, 80, 295, 1100, uiText, app.title);
    caption(detail, 82, 365);
}
void scroll(int total, int visible, int start) {
    if (total <= visible)
        return;
    rect(1230, 218, 3, 408, uiPanelHov);
    int h = std::max(18, 408 * visible / total);
    rect(1230, 218 + (408 - h) * start / std::max(1, total - visible), 3, h, uiAccent);
}
void row(int i, int start, const std::string &title, const std::string &detail, const std::string &marker,
         int width = 1168) {
    int y = 218 + (i - start) * 57;
    bool selected = i == app.cursor;
    rect(48, y, width, 53, selected ? uiPanelHov : uiPanel);
    if (selected) {
        int fy = y;
        rect(48, fy, 5, 53, uiAccent);
    }
    text(marker, 64, y + 13, 58, selected ? uiAccent : uiTextDim, app.small);
    static std::string lastTitle;
    static uint64_t selectedAt = 0;
    int titleWidth = 0;
    TTF_SizeUTF8(app.body, title.c_str(), &titleWidth, nullptr);
    if (selected && title != lastTitle) {
        lastTitle = title;
        selectedAt = SDL_GetTicks64();
    }
    if (selected && app.settings.motion && titleWidth > width - 100) {
        // Pause at both ends so a focused long filename can be read in full.
        double span = titleWidth - (width - 100);
        double travel = span / 42.0;
        double phase = std::fmod((SDL_GetTicks64() - selectedAt) / 1000.0, travel * 2 + 3.0);
        double offset = phase < 1.5            ? 0
                        : phase < travel + 1.5 ? (phase - 1.5) * 42
                        : phase < travel + 3.0 ? span
                                               : span - (phase - travel - 3.0) * 42;
        SDL_Rect clip{130, y + 4, width - 100, 28};
        SDL_RenderSetClipRect(app.renderer, &clip);
        text(title, 130 - (int)offset, y + 4, titleWidth + 1, uiText, app.body);
        SDL_RenderSetClipRect(app.renderer, nullptr);
    } else {
        text(title, 130, y + 4, width - 100, uiText, app.body);
    }
    text(detail, 130, y + 30, width - 100, uiTextDim, app.small);
}
int startFor(int count) {
    if (app.cursor < app.offset)
        app.offset = app.cursor;
    if (app.cursor >= app.offset + 7)
        app.offset = app.cursor - 6;
    app.offset = std::clamp(app.offset, 0, std::max(0, count - 7));
    float target = 218 + (app.cursor - app.offset) * 57;
    if (!app.focusY || !app.settings.motion)
        app.focusY = target;
    else
        app.focusY += (target - app.focusY) * .28f;
    return app.offset;
}
void shelf() {
    auto systems = shelfCategories(app.categories, app.shelfTab);
    for (int tab = 0; tab < 2; ++tab) {
        int x = 48 + tab * 280;
        bool active = tab == app.shelfTab;
        rect(x, 104, 264, 54, active ? uiPanelHov : uiPanel);
        if (active)
            rect(x, 154, 264, 4, uiAccent);
        text(shelfTabs[tab], x + 18, 114, 228, active ? uiAccent : uiText, app.body);
    }
    caption(std::to_string(systems.size()) + (app.shelfTab == 0 ? " systems" : " collections"), 50, 169, 500);
    if (systems.empty()) {
        empty(app.categories.empty() ? "No catalog" : "No collections in this tab",
              "L/R Change tab    Y Refresh");
        footer("L/R Tabs    Y Refresh    + Settings");
        return;
    }
    app.shelfCursor = std::clamp(app.shelfCursor, 0, (int)systems.size() - 1);
    app.cursor = app.shelfCursor;
    if (app.compact) {
        int start = startFor(systems.size());
        for (int i = start; i < std::min(start + 7, (int)systems.size()); ++i) {
            const auto &c = app.categories[systems[i]];
            row(i, start, c.name, std::to_string(c.items.size()) + " files", std::to_string(i + 1));
        }
        scroll(systems.size(), 7, start);
    } else {
        int selected = app.shelfCursor, start = selected / shelfPageSize * shelfPageSize;
        for (int i = start; i < std::min(start + shelfPageSize, (int)systems.size()); ++i) {
            const auto &c = app.categories[systems[i]];
            int local = i - start, x = 48 + local % shelfColumns * 300, y = 204 + local / shelfColumns * 144;
            bool focus = i == selected;
            rect(x + 4, y + 4, 284, 136, uiBorder);
            rect(x, y, 284, 136, focus ? uiPanelHov : uiPanel);
            if (focus) {
                rect(x, y, 284, 4, uiAccent);
                rect(x, y + 132, 284, 4, uiAccent);
                rect(x, y, 4, 136, uiAccent);
                rect(x + 280, y, 4, 136, uiAccent);
            }
            cover(c.name, x + 18, y + 8, 248, 120);
            for (int row = 0; row < 46; ++row)
                rect(x + 8, y + 82 + row, 268, 1, {0, 0, 0, (Uint8)(90 + row * 3)});
            text(c.name, x + 18, y + 98, 248, {255, 255, 255, 255}, app.body);
            auto badge = uiPanel;
            badge.a = 235;
            rect(x + 8, y + 8, 100, 27, badge);
            caption(std::to_string(c.items.size()) + " files", x + 14, y + 8, 90);
        }
        caption("Page " + std::to_string(start / shelfPageSize + 1) + " / " +
                    std::to_string(((int)systems.size() + shelfPageSize - 1) / shelfPageSize),
                1000, 169, 232);
    }
    footer("A Open    L/R Tabs    Y Refresh    + Settings", "D-pad Move");
}
void library() {
    if (app.categories.empty())
        return;
    const auto &c = app.categories[app.category];
    heading(c.name,
            app.query.empty() ? std::to_string(app.visible.size()) + " files" : "Search: " + app.query);
    pill(std::to_string(app.selected.size()) + " IN QUEUE", 48, 184, 180);
    pill(app.favoritesOnly ? "FAVORITES" : "ALL FILES", 240, 184, 170);
    pill(app.descending ? "Z - A" : "A - Z", 422, 184, 100);
    int start = startFor(app.visible.size()), width = app.compact ? 1168 : 836;
    for (int i = start; i < std::min(start + 7, (int)app.visible.size()); ++i) {
        const auto &item = c.items[app.visible[i]];
        bool selected = app.selected.count(item.key());
        bool favorite = app.settings.favorites.count(item.key());
        row(i, start, item.name, (favorite ? "Favorite  /  " : "") + item.category, selected ? "[x]" : "[ ]",
            width);
    }
    if (app.visible.empty())
        empty("No files match", "Y edit or clear search  /  L toggle favorites");
    else if (!app.compact) {
        const auto &item = c.items[app.visible[app.cursor]];
        rect(908, 218, 308, 395, uiPanel);
        caption("QUEUE", 930, 240, 262);
        text(std::to_string(app.selected.size()) + " FILES", 930, 274, 262, uiAccent, app.title);
        line(930, 338, 262, uiBorder);
        caption("DESTINATION", 930, 358, 262);
        twoLines(app.path, 930, 390, 262);
        caption("FOCUSED FILE", 930, 468, 262);
        twoLines(item.name, 930, 500, 262);
        caption("X Review download", 930, 576, 262);
    }
    scroll(app.visible.size(), 7, start);
    footer("A Select    X Download    Y Search    B Systems    + Settings",
           "L Favorites    R Sort    R3 Favorite    - Select all    ZL/ZR Page");
}
void folder() {
    heading("DESTINATION", app.path);
    pill(bytes(freeBytes()) + " free on SD", 48, 184, 300);
    int start = startFor(app.folders.size());
    for (int i = start; i < std::min(start + 7, (int)app.folders.size()); ++i)
        row(i, start, app.folders[i], "Folder", "DIR");
    if (app.folders.empty())
        empty("This folder is empty", "X chooses this location. R creates a new folder.");
    scroll(app.folders.size(), 7, start);
    footer("A  Open     L  Parent     R  New folder     Y  Recent paths     X  Use folder     B  Back");
}
void history() {
    heading("RECENT FOLDERS", "");
    int start = startFor(app.settings.history.size());
    for (int i = start; i < std::min(start + 7, (int)app.settings.history.size()); ++i)
        row(i, start, app.settings.history[i], "Saved destination", std::to_string(i + 1));
    if (app.settings.history.empty())
        empty("No recent destinations yet", "Choose a folder and start a queue to save it here.");
    scroll(app.settings.history.size(), 7, start);
    footer("A  Use location     B  Folder browser");
}
std::string status(Status s) {
    switch (s) {
    case Status::Saved:
        return "DONE";
    case Status::Skipped:
        return "SKIP";
    case Status::Failed:
        return "FAIL";
    case Status::Cancelled:
        return "STOP";
    default:
        return "WAIT";
    }
}
void queue(bool results) {
    heading(results ? "RESULTS" : "DOWNLOAD QUEUE", app.path);
    int start = startFor(app.jobs.size());
    int saved = 0, failed = 0;
    for (const auto &j : app.jobs) {
        if (j.status == Status::Saved || j.status == Status::Skipped)
            ++saved;
        if (j.status == Status::Failed || j.status == Status::Cancelled)
            ++failed;
    }
    caption(results ? std::to_string(saved) + " saved / existing  |  " + std::to_string(failed) +
                          " failed / cancelled"
                    : std::to_string(app.jobs.size()) +
                          " files  |  Skip existing  |  ZIP: " + app.settings.extractionLabel(),
            50, 190);
    for (int i = start; i < std::min(start + 7, (int)app.jobs.size()); ++i)
        row(i, start, app.jobs[i].item.name, results ? app.jobs[i].detail : app.jobs[i].item.category,
            results ? status(app.jobs[i].status) : std::to_string(i + 1));
    scroll(app.jobs.size(), 7, start);
    footer(results ? "A / B  Return to collection     X  Retry failed downloads"
                   : "A Download    R Folder    X Remove    Y Extraction    B Files");
}
void transfer() {
    auto &p = app.progress;
    bool paused = pauseWork;
    heading(p.extracting  ? "EXTRACTING"
            : app.loading ? "UPDATING CATALOG"
                          : "DOWNLOADING",
            p.title.empty() ? "Preparing background task" : p.title);
    float fraction = p.total ? std::clamp((float)p.now / p.total, 0.f, 1.f) : 0;
    static std::string lastOperation;
    static float eased = 0;
    std::string operation = p.title + (p.extracting ? "/extract" : "/download");
    if (operation != lastOperation) {
        lastOperation = operation;
        eased = 0;
    }
    eased = app.settings.motion ? eased + (fraction - eased) * .18f : fraction;
    int x = app.compact ? 48 : 420, w = app.compact ? 1184 : 812;
    if (!app.compact) {
        rect(48, 235, 340, 330, uiPanel);
        caption("FILE", 76, 265, 280);
        text(app.loading ? "CATALOG" : std::to_string(p.index) + " / " + std::to_string(p.count), 76, 310,
             280, uiAccent, app.title);
        caption("DESTINATION", 76, 410, 280);
        twoLines(app.path, 76, 454, 280);
    }
    rect(x, 235, w, 330, uiPanel);
    text(paused    ? "PAUSED"
         : p.total ? std::to_string((int)(fraction * 100)) + "%"
                   : "CONNECTING",
         x + 28, 265, w - 56, uiText, app.title);
    rect(x + 28, 343, w - 56, 18, uiBg);
    if (p.total) {
        int segments = 32, step = (w - 56) / segments;
        int filled = (int)(segments * eased + .5f);
        for (int i = 0; i < segments; ++i)
            rect(x + 28 + i * step, 343, step - 4, 18, i < filled ? uiAccent : uiPanelHov);
    } else {
        int range = w - 180;
        int shift = app.settings.motion ? (int)((SDL_GetTicks64() / 5) % std::max(1, range)) : 0;
        rect(x + 28 + shift, 343, 100, 18, uiAccent);
    }
    caption(p.extracting ? std::to_string(p.now) + " / " + std::to_string(p.total) + " entries"
                         : bytes(p.now) + (p.total ? " / " + bytes(p.total) : " received"),
            x + 28, 386, w - 56);
    if (!p.extracting) {
        std::string eta = "Estimating time remaining";
        if (p.speed > 0 && p.total > p.now) {
            int seconds = (p.total - p.now) / p.speed;
            eta = std::to_string(seconds / 60) + "m " + std::to_string(seconds % 60) + "s remaining";
        }
        caption(paused ? "Press A to resume" : bytes((uint64_t)p.speed) + "/s  /  " + eta, x + 28, 426,
                w - 56);
    }
    caption(p.detail, x + 28, 477, w - 56);
    if (app.screen == Screen::Cancel) {
        rect(180, 275, 920, 220, uiBg);
        rect(180, 275, 6, 220, uiDanger);
        text("STOP THIS QUEUE?", 220, 305, 840, uiText, app.title);
        caption("Finished files stay saved. The active partial file is removed.", 220, 371, 840);
        text("A  Stop queue       B  Continue", 220, 427, 840, uiText, app.body);
    }
    footer(app.loading ? "B Cancel update" : "A Pause / resume    B Cancel");
}
void settings() {
    heading("SETTINGS", "");
    const std::string titles[] = {"Palette",         "Motion",
                                  "Sound effects",   "Extract ZIP after download",
                                  "Refresh catalog", "Exit to Homebrew Menu"};
    const std::string details[] = {CONSOLE_THEMES[app.settings.theme].name,
                                   app.settings.motion ? "On" : "Reduced motion",
                                   app.settings.sound ? "On" : "Off",
                                   app.settings.extractionLabel() + " / Arcade excluded",
                                   "Download latest catalog",
                                   "Exit"};
    int start = startFor(6);
    for (int i = 0; i < 6; ++i)
        row(i, start, titles[i], details[i], std::to_string(i + 1));
    footer("A Change    Left/Right Adjust    B Back");
}
} // namespace
void theme(int index) {
    previous = {"",        uiBg,     uiPanel,   uiPanelHov, uiBorder, uiText,
                uiTextDim, uiAccent, uiSuccess, uiWarning,  uiDanger, uiInfo};
    app.settings.theme = index;
    themeStart = SDL_GetTicks64();
    if (!app.settings.motion)
        applyTheme(index);
}
void animateTheme() {
    float t = app.settings.motion ? std::min(1.f, (SDL_GetTicks64() - themeStart) / 240.f) : 1;
    auto &target = CONSOLE_THEMES[app.settings.theme];
    uiBg = mix(previous.bg, target.bg, t);
    uiPanel = mix(previous.panel, target.panel, t);
    uiPanelHov = mix(previous.panelHov, target.panelHov, t);
    uiBorder = mix(previous.border, target.border, t);
    uiText = mix(previous.text, target.text, t);
    uiTextDim = mix(previous.textDim, mix(target.textDim, target.text, .45f), t);
    uiAccent = mix(previous.accent, target.accent, t);
    uiSuccess = mix(previous.success, target.success, t);
    uiWarning = mix(previous.warning, target.warning, t);
    uiDanger = mix(previous.danger, target.danger, t);
    uiInfo = mix(previous.info, target.info, t);
}
void rect(int x, int y, int w, int h, SDL_Color c) {
    if (w <= 0 || h <= 0)
        return;
    SDL_SetRenderDrawColor(app.renderer, c.r, c.g, c.b, c.a);
    SDL_Rect r{x, y, w, h};
    SDL_RenderFillRect(app.renderer, &r);
}
void text(const std::string &value, int x, int y, int width, SDL_Color color, TTF_Font *font) {
    if (value.empty() || width <= 0)
        return;
    if (!font)
        font = app.body;
    std::string s = value;
    int tw = 0;
    TTF_SizeUTF8(font, s.c_str(), &tw, nullptr);
    if (tw > width) {
        while (!s.empty()) {
            size_t p = s.size() - 1;
            while (p > 0 && ((unsigned char)s[p] & 0xc0) == 0x80)
                --p;
            s.resize(p);
            TTF_SizeUTF8(font, (s + "...").c_str(), &tw, nullptr);
            if (tw <= width)
                break;
        }
        s += "...";
    }
    std::string key = std::to_string((uintptr_t)font) + s;
    auto it = labels.find(key);
    if (it == labels.end()) {
        if (labels.size() >= (app.compact ? 80u : 220u)) {
            auto oldest = std::min_element(labels.begin(), labels.end(), [](const auto &a, const auto &b) {
                return a.second.used < b.second.used;
            });
            SDL_DestroyTexture(oldest->second.texture);
            labels.erase(oldest);
        }
        auto *surf = TTF_RenderUTF8_Solid(font, s.c_str(), {255, 255, 255, 255});
        if (!surf)
            return;
        auto *texture = SDL_CreateTextureFromSurface(app.renderer, surf);
        Cached c{texture, surf->w, surf->h, ++serial};
        SDL_FreeSurface(surf);
        if (!texture)
            return;
        it = labels.emplace(key, c).first;
    }
    it->second.used = ++serial;
    SDL_SetTextureColorMod(it->second.texture, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(it->second.texture, 255);
    SDL_Rect dst{x, y, it->second.w, it->second.h};
    SDL_RenderCopy(app.renderer, it->second.texture, nullptr, &dst);
}
void render() {
    animateTheme();
    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);
    rect(0, 0, 1280, 720, uiBg);
    // Pixel-size accents stay aligned to whole screen pixels.
    rect(0, 0, 1280, 8, uiAccent);
    text("ROM DOWNLOADER", 48, 24, 620, uiText, app.body);
    pill(app.compact ? "APPLET" : "FULL RAM", 744, 28, 236);
    text(CONSOLE_THEMES[app.settings.theme].name, 1000, 34, 232, uiTextDim, app.small);
    line(48, 78, 1184, uiBorder);
    reveal = app.settings.motion ? std::min(1.f, (SDL_GetTicks64() - app.transition) / 180.f) : 1;
    int shift = (int)((1 - reveal) * (1 - reveal) * 14);
    SDL_Rect view{0, shift, 1280, 720};
    SDL_RenderSetViewport(app.renderer, &view);
    if (app.loading)
        transfer();
    else
        switch (app.screen) {
        case Screen::Shelf:
            shelf();
            break;
        case Screen::Library:
            library();
            break;
        case Screen::Folder:
            folder();
            break;
        case Screen::History:
            history();
            break;
        case Screen::Review:
            queue(false);
            break;
        case Screen::Results:
            queue(true);
            break;
        case Screen::Settings:
            settings();
            break;
        case Screen::Transfer:
        case Screen::Cancel:
            transfer();
            break;
        }
    SDL_RenderSetViewport(app.renderer, nullptr);
    if (SDL_GetTicks64() < app.toastUntil) {
        rect(280, 89, 720, 48, uiText);
        text(app.toast, 300, 101, 680, uiBg, app.small);
    }
    SDL_RenderPresent(app.renderer);
}
void clearUi() {
    for (auto &pair : labels)
        SDL_DestroyTexture(pair.second.texture);
    labels.clear();
    for (auto &pair : covers)
        if (pair.second)
            SDL_DestroyTexture(pair.second);
    covers.clear();
}
} // namespace rd
