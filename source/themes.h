#ifndef THEMES_H
#define THEMES_H

#include <SDL2/SDL.h>
#include <string>

// Global UI Color Variables used across the app
inline SDL_Color uiBg;
inline SDL_Color uiPanel;
inline SDL_Color uiPanelHov;
inline SDL_Color uiBorder;
inline SDL_Color uiText;
inline SDL_Color uiTextDim;
inline SDL_Color uiAccent;
inline SDL_Color uiSuccess;
inline SDL_Color uiWarning;
inline SDL_Color uiDanger;
inline SDL_Color uiInfo;

inline int currentThemeIdx = 0;
const int NUM_THEMES = 12;

struct Theme {
    std::string name;
    SDL_Color bg;
    SDL_Color panel;
    SDL_Color panelHov;
    SDL_Color border;
    SDL_Color text;
    SDL_Color textDim;
    SDL_Color accent;
    SDL_Color success;
    SDL_Color warning;
    SDL_Color danger;
    SDL_Color info;
};

// 12 Low-Density, High-Contrast Console Themes
// Backgrounds and panels now share similar luminance to ensure 
// the universal text color never washes out on either surface.
inline const Theme CONSOLE_THEMES[NUM_THEMES] = {
    // 0: NES (Deep Charcoal & Soft Red)
    {
        "NES",
        { 35,  35,  40,  255 }, // bg: Dark charcoal
        { 50,  50,  55,  255 }, // panel: Mid-charcoal
        { 65,  65,  70,  255 }, // panelHov
        { 160, 45,  45,  255 }, // border: Muted classic red
        { 225, 225, 230, 255 }, // text: Soft white (highly readable on dark)
        { 140, 140, 145, 255 }, // textDim
        { 160, 45,  45,  255 }, // accent
        { 175, 135, 45,  255 }, // success: Gold
        { 180, 120, 35,  255 }, // warning
        { 160, 45,  45,  255 }, // danger
        { 45,  110, 175, 255 }  // info
    },
    // 1: SNES (Dim Lavender Grey - Light Mode)
    {
        "SNES",
        { 165, 165, 175, 255 }, // bg: Muted, low-brightness grey
        { 180, 180, 190, 255 }, // panel: Slightly lighter grey
        { 195, 195, 205, 255 }, // panelHov
        { 110, 90,  160, 255 }, // border: Muted purple
        { 25,  25,  30,  255 }, // text: Near-black for perfect light-mode contrast
        { 90,  90,  100, 255 }, // textDim
        { 110, 90,  160, 255 }, // accent
        { 25,  95,  55,  255 }, // success: Dark green
        { 150, 90,  25,  255 }, // warning
        { 160, 45,  55,  255 }, // danger
        { 35,  85,  150, 255 }  // info
    },
    // 2: Game Boy Color (Dimmed Teal)
    {
        "GBC",
        { 20,  50,  50,  255 }, // bg: Very dark teal
        { 35,  65,  65,  255 }, // panel
        { 50,  80,  80,  255 }, // panelHov
        { 155, 40,  90,  255 }, // border: Desaturated berry
        { 220, 230, 230, 255 }, // text
        { 110, 140, 140, 255 }, // textDim
        { 155, 40,  90,  255 }, // accent
        { 190, 155, 45,  255 }, // success
        { 200, 140, 35,  255 }, // warning
        { 155, 40,  90,  255 }, // danger
        { 40,  120, 175, 255 }  // info
    },
    // 3: GBA Yellow (Dim Mustard - Light Mode)
    {
        "GBA (Yellow)",
        { 170, 150, 40,  255 }, // bg: Low-intensity mustard
        { 185, 165, 50,  255 }, // panel
        { 200, 180, 60,  255 }, // panelHov
        { 140, 45,  35,  255 }, // border
        { 20,  20,  25,  255 }, // text: Deep black for high contrast
        { 100, 85,  25,  255 }, // textDim
        { 140, 45,  35,  255 }, // accent
        { 20,  85,  35,  255 }, // success
        { 140, 80,  20,  255 }, // warning
        { 140, 45,  35,  255 }, // danger
        { 30,  75,  140, 255 }  // info
    },
    // 4: GBA Cobalt (Dark Cobalt)
    {
        "GBA (Cobalt)",
        { 25,  40,  90,  255 }, // bg: Deep desaturated blue
        { 40,  55,  105, 255 }, // panel
        { 55,  70,  120, 255 }, // panelHov
        { 150, 150, 160, 255 }, // border
        { 225, 230, 240, 255 }, // text
        { 110, 125, 165, 255 }, // textDim
        { 150, 150, 160, 255 }, // accent
        { 55,  165, 135, 255 }, // success
        { 195, 150, 45,  255 }, // warning
        { 170, 55,  55,  255 }, // danger
        { 190, 190, 200, 255 }  // info
    },
    // 5: GBA Pearl Pink (Dim Rose - Light Mode)
    {
        "GBA (Pearl Pink)",
        { 165, 120, 135, 255 }, // bg: Low-brightness dusty rose
        { 180, 135, 150, 255 }, // panel
        { 195, 150, 165, 255 }, // panelHov
        { 130, 45,  65,  255 }, // border
        { 25,  15,  20,  255 }, // text: Extremely dark burgundy
        { 100, 60,  75,  255 }, // textDim
        { 130, 45,  65,  255 }, // accent
        { 25,  85,  45,  255 }, // success
        { 150, 90,  25,  255 }, // warning
        { 130, 40,  50,  255 }, // danger
        { 30,  70,  135, 255 }  // info
    },
    // 6: Famicom (Dim Cream - Light Mode)
    // Fixed: Cream BG and Cream Panels ensure the dark text is readable everywhere. 
    // Crimson is moved exclusively to borders and accents.
    {
        "Famicom",
        { 185, 175, 150, 255 }, // bg: Muted tan/cream
        { 200, 190, 165, 255 }, // panel: Slightly lighter cream
        { 215, 205, 180, 255 }, // panelHov
        { 150, 40,  50,  255 }, // border: Classic Famicom crimson
        { 30,  20,  20,  255 }, // text: Dark red-tinted black for perfect clarity
        { 120, 100, 90,  255 }, // textDim
        { 150, 40,  50,  255 }, // accent
        { 30,  100, 45,  255 }, // success: Legible dark green
        { 160, 110, 30,  255 }, // warning
        { 150, 40,  50,  255 }, // danger
        { 40,  95,  160, 255 }  // info
    },
    // 7: Sega Genesis (Deep Black & Power LED Red)
    {
        "Sega Genesis",
        { 20,  20,  20,  255 }, // bg: Matte black shell
        { 38,  38,  38,  255 }, // panel: Dark grey plastic
        { 55,  55,  55,  255 }, // panelHov
        { 170, 30,  30,  255 }, // border: Power LED Red
        { 230, 230, 230, 255 }, // text: Crisp white
        { 130, 130, 130, 255 }, // textDim
        { 170, 30,  30,  255 }, // accent
        { 50,  140, 190, 255 }, // success: Sonic 16-bit Blue
        { 190, 150, 35,  255 }, // warning: Golden Rings
        { 170, 30,  30,  255 }, // danger
        { 140, 140, 150, 255 }  // info
    },
    // 8: GameCube (Dark Indigo)
    {
        "GameCube",
        { 40,  30,  75,  255 }, // bg: Very dark desaturated indigo
        { 55,  45,  95,  255 }, // panel
        { 70,  60,  115, 255 }, // panelHov
        { 155, 125, 205, 255 }, // border
        { 225, 225, 235, 255 }, // text
        { 130, 115, 160, 255 }, // textDim
        { 35,  140, 95,  255 }, // accent: A-Button Green
        { 195, 155, 35,  255 }, // success: C-Stick Yellow
        { 190, 120, 35,  255 }, // warning
        { 170, 50,  50,  255 }, // danger
        { 55,  135, 190, 255 }  // info
    },
    // 9: Xbox (Very Dark Matte Green)
    {
        "Xbox",
        { 18,  22,  18,  255 }, // bg: Barely-green matte black
        { 32,  38,  32,  255 }, // panel
        { 45,  52,  45,  255 }, // panelHov
        { 45,  150, 45,  255 }, // border: Toned-down neon green
        { 220, 230, 220, 255 }, // text
        { 100, 125, 100, 255 }, // textDim
        { 45,  150, 45,  255 }, // accent
        { 50,  165, 50,  255 }, // success
        { 185, 145, 35,  255 }, // warning
        { 165, 50,  35,  255 }, // danger
        { 40,  125, 165, 255 }  // info
    },
    // 10: Nintendo Switch (Dark Slate)
    {
        "Nintendo Switch",
        { 40,  42,  45,  255 }, // bg: Slate grey
        { 55,  58,  62,  255 }, // panel
        { 70,  73,  78,  255 }, // panelHov
        { 185, 60,  60,  255 }, // border: Muted Joycon Red
        { 230, 230, 230, 255 }, // text
        { 135, 140, 145, 255 }, // textDim
        { 40,  150, 195, 255 }, // accent: Muted Joycon Blue
        { 40,  165, 130, 255 }, // success
        { 195, 155, 40,  255 }, // warning
        { 185, 60,  60,  255 }, // danger
        { 40,  150, 195, 255 }  // info
    },
    // 11: PlayStation 5 (Dark Midnight Core & Silver Borders)
    // Fixed: Now a true dark mode. Dark midnight core allows pure readability 
    // for the white text, while muted silver handles the borders.
    {
        "PlayStation 5",
        { 25,  27,  33,  255 }, // bg: Deep midnight core
        { 42,  45,  52,  255 }, // panel: Elevated midnight
        { 58,  62,  70,  255 }, // panelHov
        { 180, 185, 195, 255 }, // border: Matte silver plate
        { 230, 235, 245, 255 }, // text: Crisp off-white
        { 120, 125, 135, 255 }, // textDim
        { 40,  120, 220, 255 }, // accent: DualSense Blue
        { 40,  135, 205, 255 }, // success
        { 190, 140, 40,  255 }, // warning
        { 180, 55,  55,  255 }, // danger
        { 100, 150, 200, 255 }  // info
    }
};

inline void applyTheme(int idx) {
    if (idx < 0 || idx >= NUM_THEMES) idx = 0;
    currentThemeIdx = idx;
    
    uiBg       = CONSOLE_THEMES[idx].bg;
    uiPanel    = CONSOLE_THEMES[idx].panel;
    uiPanelHov = CONSOLE_THEMES[idx].panelHov;
    uiBorder   = CONSOLE_THEMES[idx].border;
    uiText     = CONSOLE_THEMES[idx].text;
    uiTextDim  = CONSOLE_THEMES[idx].textDim;
    uiAccent   = CONSOLE_THEMES[idx].accent;
    uiSuccess  = CONSOLE_THEMES[idx].success;
    uiWarning  = CONSOLE_THEMES[idx].warning;
    uiDanger   = CONSOLE_THEMES[idx].danger;
    uiInfo     = CONSOLE_THEMES[idx].info;
}

inline void cycleTheme() {
    applyTheme((currentThemeIdx + 1) % NUM_THEMES);
}

#endif