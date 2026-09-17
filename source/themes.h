#ifndef THEMES_H
#ifndef THEMES_H
#define THEMES_H

#include <SDL2/SDL.h>
#include <string>

// Global UI Color Variables used across the app
inline SDL_Color uiBg;
inline SDL_Color uiPanel;
inline SDL_Color uiBorder;
inline SDL_Color uiText;
inline SDL_Color uiTextDim;
inline SDL_Color uiAccent;
inline SDL_Color uiSuccess;

inline int currentThemeIdx = 0;
const int NUM_THEMES = 10;

struct Theme {
    std::string name;
    SDL_Color bg;
    SDL_Color panel;
    SDL_Color border;
    SDL_Color text;
    SDL_Color textDim;
    SDL_Color accent;
    SDL_Color success;
};

// 10 Most Iconic Gaming Consoles (High-Contrast Palette Engine)
inline const Theme CONSOLE_THEMES[NUM_THEMES] = {
    // 0: NES (Nintendo Entertainment System)
    {
        "NES (1985)",
        { 32, 32, 36, 255 },   // Slate Background
        { 54, 54, 60, 255 },   // Matte Gray Panel
        { 218, 41, 28, 255 },  // Controller Red Border
        { 250, 250, 250, 255 },// Crisp White Text
        { 185, 190, 195, 255 },// Light Silver TextDim
        { 180, 30, 35, 255 },  // Deep Red Accent
        { 230, 80, 60, 255 }   // Bright Red-Orange Success
    },
    // 1: SNES (Super Nintendo Entertainment System)
    {
        "SNES (1990)",
        { 34, 34, 46, 255 },   // Dark Purple-Gray Background
        { 54, 52, 72, 255 },   // Console Gray Panel
        { 125, 95, 205, 255 }, // SNES Purple Border
        { 250, 250, 255, 255 },// Pure White Text
        { 190, 185, 215, 255 },// Lavender Gray TextDim
        { 95, 65, 165, 255 },  // Royal Purple Accent
        { 175, 135, 245, 255 } // Bright Violet Success
    },
    // 2: Game Boy (Original DMG-01 LCD Matrix)
    {
        "Game Boy (1989)",
        { 18, 34, 18, 255 },   // Deep Dot-Matrix Background
        { 32, 58, 32, 255 },   // Dark Olive Panel
        { 139, 172, 15, 255 }, // Bright LCD Border
        { 230, 250, 205, 255 },// Mint Light Text
        { 160, 195, 40, 255 }, // Olive LCD TextDim
        { 60, 105, 45, 255 },  // Mid-Green Accent
        { 185, 225, 30, 255 }  // Neon Matrix Success
    },
    // 3: SEGA Genesis / Mega Drive
    {
        "SEGA Genesis (1988)",
        { 18, 18, 22, 255 },   // High Definition Black
        { 36, 36, 44, 255 },   // Metallic Console Panel
        { 212, 160, 23, 255 }, // 16-BIT Gold Border
        { 250, 250, 250, 255 },// Pure White Text
        { 175, 180, 195, 255 },// Silver Gray TextDim
        { 25, 80, 185, 255 },  // Sonic Blue Accent
        { 245, 190, 35, 255 }  // Gold Ring Success
    },
    // 4: PlayStation (PS1 - Classic Gray)
    {
        "PlayStation (1994)",
        { 38, 40, 46, 255 },   // Original Grey Background
        { 60, 62, 70, 255 },   // PS1 Chassis Panel
        { 0, 115, 220, 255 },  // PS Blue Border
        { 255, 255, 255, 255 },// Pure White Text
        { 190, 195, 205, 255 },// Cool Gray TextDim
        { 20, 65, 140, 255 },  // Cobalt Blue Accent
        { 0, 205, 185, 255 }   // Teal Symbol Success
    },
    // 5: Nintendo 64 (N64 Charcoal)
    {
        "Nintendo 64 (1996)",
        { 24, 24, 28, 255 },   // Charcoal Black Background
        { 46, 48, 56, 255 },   // Slate Controller Panel
        { 230, 20, 30, 255 },  // N64 Logo Red Border
        { 250, 250, 250, 255 },// Pure White Text
        { 180, 185, 195, 255 },// Neutral Gray TextDim
        { 175, 25, 35, 255 },  // Dark Logo Red Accent
        { 0, 185, 90, 255 }    // N64 Logo Green Success
    },
    // 6: Nintendo GameCube (Indigo)
    {
        "GameCube (2001)",
        { 25, 20, 45, 255 },   // Deep Indigo Background
        { 48, 40, 82, 255 },   // GameCube Purple Panel
        { 125, 95, 215, 255 }, // Bright Purple Border
        { 250, 250, 250, 255 },// Pure White Text
        { 195, 185, 230, 255 },// Lavender TextDim
        { 85, 55, 160, 255 },  // Indigo Accent
        { 255, 195, 0, 255 }   // Solar Yellow Success
    },
    // 7: PlayStation 2 (Emotion Blue)
    {
        "PlayStation 2 (2000)",
        { 12, 14, 24, 255 },   // Midnight Black Background
        { 24, 30, 52, 255 },   // Deep Navy Panel
        { 0, 140, 230, 255 },  // Gradient Blue Border
        { 245, 248, 255, 255 },// Bright Ice White Text
        { 165, 190, 225, 255 },// Sky Tint TextDim
        { 15, 75, 170, 255 },  // Deep Ocean Accent
        { 0, 215, 255, 255 }   // Cyan Glow Success
    },
    // 8: Xbox (Original Neon & Black)
    {
        "Original Xbox (2001)",
        { 14, 20, 14, 255 },   // Deep Matrix Background
        { 28, 40, 28, 255 },   // Xbox Jewel Panel
        { 115, 195, 35, 255 }, // Neon Green Border
        { 250, 255, 250, 255 },// Pure White Text
        { 170, 210, 170, 255 },// Soft Mint TextDim
        { 45, 115, 30, 255 },  // Forest Green Accent
        { 130, 225, 40, 255 }  // Bright Neon Success
    },
    // 9: Nintendo Switch (OLED Modern)
    {
        "Nintendo Switch (2017)",
        { 20, 22, 26, 255 },   // Dark Joy-Con Background
        { 38, 42, 50, 255 },   // Slate Grey Panel
        { 255, 60, 75, 255 },  // Neon Red Border
        { 255, 255, 255, 255 },// Crisp White Text
        { 175, 185, 200, 255 },// Slate Blue TextDim
        { 0, 160, 230, 255 },  // Neon Blue Accent
        { 0, 225, 180, 255 }   // Mint Cyan Success
    }
};

inline void applyTheme(int idx) {
    if (idx < 0 || idx >= NUM_THEMES) idx = 0;
    currentThemeIdx = idx;
    
    uiBg      = CONSOLE_THEMES[idx].bg;
    uiPanel   = CONSOLE_THEMES[idx].panel;
    uiBorder  = CONSOLE_THEMES[idx].border;
    uiText    = CONSOLE_THEMES[idx].text;
    uiTextDim = CONSOLE_THEMES[idx].textDim;
    uiAccent  = CONSOLE_THEMES[idx].accent;
    uiSuccess = CONSOLE_THEMES[idx].success;
}

inline void cycleTheme() {
    applyTheme((currentThemeIdx + 1) % NUM_THEMES);
}

#endif