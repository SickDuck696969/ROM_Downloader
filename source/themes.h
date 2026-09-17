#pragma once
#include <SDL2/SDL.h>

// Using C++17 inline to allow header-only inclusion without multiple definition linker errors
inline SDL_Color uiBg;
inline SDL_Color uiPanel;
inline SDL_Color uiPanelHov;
inline SDL_Color uiText;
inline SDL_Color uiTextDim;
inline SDL_Color uiAccent;
inline SDL_Color uiSuccess;
inline SDL_Color uiBorder;

inline int currentThemeIdx = 0;

inline void applyTheme(int themeIdx) {
    currentThemeIdx = themeIdx;
    switch(themeIdx) {
        case 0: // SNES Classic (Softened Light Grey)
            uiBg       = { 185, 187, 190, 255}; // Muted, darker plastic grey
            uiPanel    = { 160, 162, 168, 255};
            uiPanelHov = { 145, 148, 155, 255};
            uiText     = {  35,  35,  40, 255}; // Softer off-black
            uiTextDim  = {  95,  95, 100, 255};
            uiAccent   = { 135, 120, 175, 255}; // Deep SNES Purple
            uiSuccess  = {  80,  60, 130, 255};
            uiBorder   = { 120, 120, 125, 255};
            break;
            
        case 1: // GBA SP Yellow (Mustard / Goldenrod)
            uiBg       = { 205, 175,  35, 255}; // Darker, warmer gold (less blinding)
            uiPanel    = { 185, 155,  25, 255}; 
            uiPanelHov = { 170, 140,  15, 255};
            uiText     = {  40,  40,  40, 255};
            uiTextDim  = { 120, 100,  30, 255}; 
            uiAccent   = { 190,  50,  35, 255}; // Muted red button highlight
            uiSuccess  = {  50, 140,  50, 255}; 
            uiBorder   = { 160, 130,  20, 255};
            break;

        case 2: // GameCube (Deep Indigo Dark Mode)
            uiBg       = {  65,  50, 115, 255}; // Dark indigo shell
            uiPanel    = {  45,  35,  85, 255}; // Darker interior
            uiPanelHov = {  55,  45,  95, 255};
            uiText     = { 230, 230, 235, 255}; // Soft white text
            uiTextDim  = { 140, 130, 180, 255};
            uiAccent   = {  30, 170,  70, 255}; // 'A' Button Green
            uiSuccess  = { 200,  40,  40, 255}; // 'B' Button Red
            uiBorder   = {  35,  25,  70, 255};
            break;
            
        case 3: // Original Game Boy DMG (Retro Pea Green)
            uiBg       = { 155, 188,  15, 255}; // Iconic green screen
            uiPanel    = { 139, 172,  15, 255}; 
            uiPanelHov = { 110, 140,  15, 255};
            uiText     = {  15,  56,  15, 255}; // Darkest green LCD pixel
            uiTextDim  = {  48,  98,  48, 255}; // Mid-tone green
            uiAccent   = { 155,  25,  60, 255}; // Maroon A/B buttons
            uiSuccess  = {  15,  56,  15, 255}; 
            uiBorder   = {  48,  98,  48, 255};
            break;

        case 4: // Original Xbox (Black & Neon Green Dark Mode)
            uiBg       = {  18,  18,  20, 255}; // Almost black
            uiPanel    = {  28,  28,  32, 255}; // Charcoal panel
            uiPanelHov = {  40,  40,  45, 255};
            uiText     = { 220, 220, 220, 255};
            uiTextDim  = { 110, 110, 115, 255};
            uiAccent   = {  16, 124,  16, 255}; // Xbox Jewel Green
            uiSuccess  = {  20, 160,  20, 255};
            uiBorder   = {  45,  45,  50, 255};
            break;

        case 5: // PlayStation 2 (Midnight Blue Dark Mode)
            uiBg       = {  10,  12,  18, 255}; // Very dark blue/black
            uiPanel    = {  20,  25,  35, 255}; 
            uiPanelHov = {  30,  35,  48, 255};
            uiText     = { 235, 235, 245, 255};
            uiTextDim  = { 100, 110, 150, 255};
            uiAccent   = {  40,  85, 210, 255}; // PS2 Blue
            uiSuccess  = { 100, 110, 230, 255};
            uiBorder   = {  40,  45,  65, 255};
            break;

        case 6: // Switch OLED (High-Contrast Dark Mode)
            uiBg       = {  43,  43,  43, 255}; // Switch OS Dark Grey
            uiPanel    = {  55,  55,  55, 255}; 
            uiPanelHov = {  70,  70,  70, 255};
            uiText     = { 255, 255, 255, 255};
            uiTextDim  = { 160, 160, 160, 255};
            uiAccent   = {   0, 215, 215, 255}; // Joy-Con Neon Blue
            uiSuccess  = { 230,   0,  18, 255}; // Joy-Con Neon Red
            uiBorder   = {  80,  80,  80, 255};
            break;
    }
}

inline void cycleTheme() {
    // Increment and wrap around using modulo based on our 7 total themes
    applyTheme((currentThemeIdx + 1) % 7);
}