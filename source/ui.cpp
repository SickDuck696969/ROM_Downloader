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
void cartridge(int x, int y, int w, int h, int n, bool selected) {
    rect(x + 5, y + 5, w, h, uiBorder);
    rect(x, y, w, h, selected ? uiPanelHov : uiPanel);
    rect(x + 12, y + 12, w - 24, 8, uiAccent);
    rect(x + 20, y + 32, w - 40, h - 69, uiBg);
    for (int i = 0; i < 7; ++i)
        rect(x + 25 + i * 7, y + h - 25, 3, 14, uiBorder);
    text(std::to_string(n + 1), x + w - 65, y + h - 32, 48, uiTextDim, app.body);
}
void cover(const std::string &name, int x, int y, int w, int h) {
    auto it = covers.find(name);
    if (it == covers.end()) {
        if (covers.size() >= 9) {
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
    // A procedural cartridge label gives uncached collections their own identity.
    for (int i = 0; i < 5; ++i) {
        int bw = 15 + ((name.size() * 13 + i * 17) % 55);
        rect(x + 14 + i * (w - 28) / 5, y + h / 2 - bw / 2, 12, bw, uiAccent);
    }
    text(name, x + 12, y + h - 35, w - 24, uiText, app.small);
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
        int fy = app.settings.motion ? (int)app.focusY : y;
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
    heading("YOUR CARTRIDGE SHELF", app.compact
                                        ? "Pocket view  /  compact collections, full download controls"
                                        : "Choose a collection. Build a queue. Make it yours.");
    if (app.categories.empty()) {
        empty("A shelf waiting to be filled", "Y refresh catalog  /  + studio settings");
        return;
    }
    if (app.compact) {
        app.cursor = app.shelfCursor;
        int start = startFor(app.categories.size());
        for (int i = start; i < std::min(start + 7, (int)app.categories.size()); ++i)
            row(i, start, app.categories[i].name, std::to_string(app.categories[i].items.size()) + " files",
                std::to_string(i + 1));
        scroll(app.categories.size(), 7, start);
    } else {
        int selected = std::clamp(app.shelfCursor, 0, (int)app.categories.size() - 1);
        const auto &c = app.categories[selected];
        rect(48, 215, 326, 408, uiPanel);
        caption("NOW BROWSING", 70, 238, 280);
        cartridge(83, 282, 250, 198, selected, true);
        cover(c.name, 108, 318, 200, 112);
        text(c.name, 70, 510, 286, uiText, app.body);
        caption(std::to_string(c.items.size()) + " files in collection", 70, 548, 280);
        pill("A  EXPLORE", 70, 582, 160);
        int start = selected / 6 * 6;
        for (int i = start; i < std::min(start + 6, (int)app.categories.size()); ++i) {
            int local = i - start, x = 414 + (local % 3) * 276, y = 218 + (local / 3) * 210;
            bool focus = i == selected;
            float pulse = app.settings.motion ? (std::sin(SDL_GetTicks64() / 230.0) + 1) * 1.5f : 0;
            int lift = focus ? (int)(5 + pulse) : 0;
            cartridge(x, y - lift, 250, 180, i, focus);
            twoLines(app.categories[i].name, x + 28, y + 40 - lift, 196);
            caption(std::to_string(app.categories[i].items.size()) + " files", x + 28, y + 105 - lift, 196);
            if (focus)
                rect(x, y + 183, 250, 3, uiAccent);
        }
        caption(std::to_string(selected + 1) + " / " + std::to_string(app.categories.size()) + " collections",
                420, 625, 750);
    }
    footer("A  Open collection     Y  Refresh catalog     +  Studio settings",
           "ZL + ZR  Change theme anywhere");
}
void library() {
    if (app.categories.empty())
        return;
    const auto &c = app.categories[app.category];
    heading(c.name, app.query.empty() ? "Find your next file. Selections stay with you across collections."
                                      : "Search: " + app.query);
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
        caption("FILE INSPECTOR", 930, 241, 262);
        cartridge(949, 290, 220, 140, app.visible[app.cursor], true);
        auto dot = item.name.find_last_of('.');
        text(dot == std::string::npos ? "FILE" : item.name.substr(dot + 1), 984, 336, 150, uiAccent,
             app.title);
        text(item.name, 930, 456, 262, uiText, app.body);
        caption(app.settings.favorites.count(item.key()) ? "In your favorites" : "R-stick click to favorite",
                930, 500, 262);
        caption("X  Choose destination", 930, 551, 262);
        caption(std::to_string(app.visible.size()) + " matching files", 930, 582, 262);
    }
    scroll(app.visible.size(), 7, start);
    footer("A  Select     X  Queue     Y  Search     L  Favorites     R  Sort     B  Shelf",
           "-  Select visible / none     R-stick  Favorite     ZL / ZR  Page     +  Settings");
}
void folder() {
    heading("CHOOSE A DESTINATION", app.path);
    pill(bytes(freeBytes()) + " free on SD", 48, 184, 300);
    int start = startFor(app.folders.size());
    for (int i = start; i < std::min(start + 7, (int)app.folders.size()); ++i)
        row(i, start, app.folders[i], "Folder", "DIR");
    if (app.folders.empty())
        empty("This folder is empty", "X chooses this location. R creates a new folder.");
    scroll(app.folders.size(), 7, start);
    footer("A  Open     L  Parent     R  New folder     Y  Recent paths     X  Review queue     B  Files");
}
void history() {
    heading("RECENT DESTINATIONS", "Your last 12 locations, most recent first");
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
    heading(results ? "YOUR QUEUE, COMPLETED" : "ONE LAST LOOK", app.path);
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
                          " files  |  Existing files are skipped  |  ZIP extraction: " +
                          app.settings.extractionLabel(),
            50, 190);
    for (int i = start; i < std::min(start + 7, (int)app.jobs.size()); ++i)
        row(i, start, app.jobs[i].item.name, results ? app.jobs[i].detail : app.jobs[i].item.category,
            results ? status(app.jobs[i].status) : std::to_string(i + 1));
    scroll(app.jobs.size(), 7, start);
    footer(results ? "A / B  Return to collection     X  Retry failed downloads"
                   : "A  Start queue     X  Remove item     Y  Extraction mode     B  Destination");
}
void transfer() {
    auto &p = app.progress;
    bool paused = pauseWork;
    heading(p.extracting  ? "UNPACKING YOUR FILES"
            : app.loading ? "SYNCING COLLECTIONS"
                          : "ON ITS WAY",
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
        cartridge(73, 255, 288, 270, std::max(0, p.index - 1), true);
        text(paused ? "II" : p.extracting ? "ZIP" : "DL", 130, 345, 190, uiAccent, app.title);
        caption(app.loading ? "CATALOG" : std::to_string(p.index) + " / " + std::to_string(p.count), 100, 550,
                250);
    }
    rect(x, 235, w, 330, uiPanel);
    text(paused    ? "PAUSED"
         : p.total ? std::to_string((int)(fraction * 100)) + "%"
                   : "CONNECTING",
         x + 28, 265, w - 56, uiText, app.title);
    rect(x + 28, 343, w - 56, 18, uiBg);
    if (p.total) {
        int fill = (int)((w - 56) * eased);
        rect(x + 28, 343, fill, 18, uiAccent);
        if (app.settings.motion && !paused && fill > 32) {
            int sweep = (SDL_GetTicks64() / 9) % (fill - 28);
            rect(x + 28 + sweep, 343, 28, 18, mix(uiAccent, uiText, .25f));
        }
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
    caption(p.extracting
                ? (app.settings.deleteOriginal ? "Delete ZIP only after full success; keep on errors or skips"
                                               : "Original archive stays on your SD card")
                : "Files are saved only after a successful transfer",
            x + 28, 521, w - 56);
    if (app.screen == Screen::Cancel) {
        rect(180, 275, 920, 220, uiBg);
        rect(180, 275, 6, 220, uiDanger);
        text("STOP THIS QUEUE?", 220, 305, 840, uiText, app.title);
        caption("Finished files stay saved. The active partial file is removed.", 220, 371, 840);
        text("A  Stop queue       B  Continue", 220, 427, 840, uiText, app.body);
    }
    footer(app.loading ? "B  Cancel update     ZL + ZR  Change theme"
                       : "A  Pause / resume     B  Stop queue     ZL + ZR  Change theme");
}
void settings() {
    heading("MAKE IT YOURS", "Live palettes, comfortable motion, and your preferred workflow");
    const std::string titles[] = {"Palette",         "Motion",
                                  "Sound effects",   "Extract ZIP after download",
                                  "Refresh catalog", "Exit to Homebrew Menu"};
    const std::string details[] = {CONSOLE_THEMES[app.settings.theme].name,
                                   app.settings.motion ? "Smooth movement + animated accents"
                                                       : "Reduced motion",
                                   app.settings.sound ? "On" : "Off",
                                   app.settings.extractionLabel() + " / Arcade excluded",
                                   "Refresh online; cached catalog remains available offline",
                                   "Finish your session"};
    int start = startFor(6);
    for (int i = 0; i < 6; ++i)
        row(i, start, titles[i], details[i], std::to_string(i + 1));
    footer("A  Change / activate     Left / Right  Change palette     B  Back",
           "ZL + ZR  Cycle palettes from any screen");
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
        auto *surf = TTF_RenderUTF8_Blended(font, s.c_str(), {255, 255, 255, 255});
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
    // Offset printing marks and a segmented masthead define the studio identity.
    rect(0, 0, 1280, 7, uiAccent);
    text("ROM / STUDIO", 48, 29, 400, uiText, app.body);
    caption("02", 424, 34, 70);
    pill(app.compact ? "APPLET / POCKET" : "FULL RAM / SHELF", 744, 28, 236);
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
