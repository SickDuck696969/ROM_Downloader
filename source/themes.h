#pragma once
#include <SDL2/SDL.h>

// Base UI Colors
inline SDL_Color uiBg;
inline SDL_Color uiPanel;
inline SDL_Color uiPanelHov;
inline SDL_Color uiBorder;
inline SDL_Color uiText;
inline SDL_Color uiTextDim;

// Extended Multi-Color Action Palette
inline SDL_Color uiAccent;  // Primary / Button 1
inline SDL_Color uiSuccess; // Success / Button 2
inline SDL_Color uiWarning; // Yellows/Oranges / Button 3
inline SDL_Color uiDanger;  // Reds/Pinks / Button 4
inline SDL_Color uiInfo;    // Blues/Cyans / Button 5

inline int currentThemeIdx = 0;
const int TOTAL_THEMES = 12; // Now we have 12 themes!

inline void applyTheme(int themeIdx) {
    currentThemeIdx = themeIdx;
    switch(themeIdx) {
        case 0: // Super Famicom / SNES (Light)
            uiBg       = { 185, 187, 190, 255}; 
            uiPanel    = { 160, 162, 168, 255};
            uiPanelHov = { 145, 148, 155, 255};
            uiBorder   = { 120, 120, 125, 255};
            uiText     = {  35,  35,  40, 255}; 
            uiTextDim  = {  95,  95, 100, 255};
            uiAccent   = { 200,  40,  40, 255}; // Red
            uiSuccess  = { 230, 180,  30, 255}; // Yellow
            uiWarning  = {  40, 160,  60, 255}; // Green
            uiDanger   = { 200,  40,  40, 255}; 
            uiInfo     = {  30,  80, 180, 255}; // Blue
            break;
            
        case 1: // GBA SP Yellow (Light)
            uiBg       = { 205, 175,  35, 255}; 
            uiPanel    = { 185, 155,  25, 255}; 
            uiPanelHov = { 170, 140,  15, 255};
            uiBorder   = { 160, 130,  20, 255};
            uiText     = {  40,  40,  40, 255};
            uiTextDim  = { 120, 100,  30, 255}; 
            uiAccent   = { 190,  50,  35, 255}; 
            uiSuccess  = {  50, 140,  50, 255}; 
            uiWarning  = { 230, 200,  50, 255}; 
            uiDanger   = { 190,  50,  35, 255}; 
            uiInfo     = {  50, 130, 200, 255}; 
            break;

        case 2: // GameCube (Dark)
            uiBg       = {  65,  50, 115, 255}; 
            uiPanel    = {  45,  35,  85, 255}; 
            uiPanelHov = {  55,  45,  95, 255};
            uiBorder   = {  35,  25,  70, 255};
            uiText     = { 230, 230, 235, 255}; 
            uiTextDim  = { 140, 130, 180, 255};
            uiAccent   = {  30, 170,  70, 255}; 
            uiSuccess  = { 200,  40,  40, 255}; 
            uiWarning  = { 220, 170,  40, 255}; 
            uiDanger   = { 200,  40,  40, 255}; 
            uiInfo     = { 150, 150, 160, 255}; 
            break;
            
        case 3: // Game Boy DMG (Light/Mid)
            uiBg       = { 155, 188,  15, 255}; 
            uiPanel    = { 139, 172,  15, 255}; 
            uiPanelHov = { 110, 140,  15, 255};
            uiBorder   = {  48,  98,  48, 255};
            uiText     = {  15,  56,  15, 255}; 
            uiTextDim  = {  48,  98,  48, 255}; 
            uiAccent   = { 155,  25,  60, 255}; 
            uiSuccess  = {  15,  56,  15, 255}; 
            uiWarning  = { 100,  20,  40, 255}; 
            uiDanger   = { 155,  25,  60, 255}; 
            uiInfo     = {  15,  56,  15, 255}; 
            break;

        case 4: // Original Xbox (Dark)
            uiBg       = {  18,  18,  20, 255}; 
            uiPanel    = {  28,  28,  32, 255}; 
            uiPanelHov = {  40,  40,  45, 255};
            uiBorder   = {  45,  45,  50, 255};
            uiText     = { 220, 220, 220, 255};
            uiTextDim  = { 110, 110, 115, 255};
            uiAccent   = {  16, 124,  16, 255}; 
            uiSuccess  = { 200,  30,  30, 255}; 
            uiWarning  = { 220, 160,  20, 255}; 
            uiDanger   = { 200,  30,  30, 255}; 
            uiInfo     = {  30,  80, 200, 255}; 
            break;

        case 5: // PlayStation 2 (Dark)
            uiBg       = {  10,  12,  18, 255}; 
            uiPanel    = {  20,  25,  35, 255}; 
            uiPanelHov = {  30,  35,  48, 255};
            uiBorder   = {  40,  45,  65, 255};
            uiText     = { 235, 235, 245, 255};
            uiTextDim  = { 100, 110, 150, 255};
            uiAccent   = { 100, 130, 230, 255}; 
            uiSuccess  = { 220,  80,  80, 255}; 
            uiWarning  = {  60, 200, 120, 255}; 
            uiDanger   = { 210, 100, 180, 255}; 
            uiInfo     = {  80, 180, 240, 255}; 
            break;

        case 6: // Nintendo Switch (Dark)
            uiBg       = {  43,  43,  43, 255}; 
            uiPanel    = {  55,  55,  55, 255}; 
            uiPanelHov = {  70,  70,  70, 255};
            uiBorder   = {  80,  80,  80, 255};
            uiText     = { 255, 255, 255, 255};
            uiTextDim  = { 160, 160, 160, 255};
            uiAccent   = { 255,  60,  60, 255}; 
            uiSuccess  = {   0, 195, 227, 255}; 
            uiWarning  = { 240, 200,  50, 255}; 
            uiDanger   = { 255,  60,  60, 255}; 
            uiInfo     = {   0, 195, 227, 255}; 
            break;

        case 7: // Sega Dreamcast (Light)
            uiBg       = { 235, 235, 240, 255}; 
            uiPanel    = { 215, 215, 220, 255}; 
            uiPanelHov = { 200, 200, 205, 255};
            uiBorder   = { 180, 180, 185, 255};
            uiText     = {  40,  40,  45, 255};
            uiTextDim  = { 120, 120, 125, 255};
            uiAccent   = { 255, 100,  20, 255}; 
            uiSuccess  = { 220,  50,  50, 255}; 
            uiWarning  = { 230, 190,  30, 255}; 
            uiDanger   = {  40, 180,  80, 255}; 
            uiInfo     = {  50, 100, 220, 255}; 
            break;

        case 8: // Famicom (Light)
            uiBg       = { 240, 235, 225, 255}; // Cream Shell
            uiPanel    = { 225, 215, 205, 255}; // Slightly darker cream
            uiPanelHov = { 210, 200, 190, 255};
            uiBorder   = { 160,  30,  40, 255}; // Crimson Red Border
            uiText     = {  40,  30,  30, 255}; // Very dark red/black text
            uiTextDim  = { 150, 100, 100, 255};
            uiAccent   = { 160,  30,  40, 255}; // Famicom Crimson
            uiSuccess  = {  40,  40,  40, 255}; // Black
            uiWarning  = { 210, 170,  50, 255}; // Gold controller plate
            uiDanger   = { 160,  30,  40, 255}; 
            uiInfo     = {  50,  50,  50, 255}; 
            break;

        case 9: // Nintendo 64 (Dark)
            uiBg       = {  25,  25,  28, 255}; 
            uiPanel    = {  40,  40,  45, 255}; 
            uiPanelHov = {  55,  55,  60, 255};
            uiBorder   = {  65,  65,  70, 255};
            uiText     = { 230, 230, 230, 255};
            uiTextDim  = { 130, 130, 130, 255};
            uiAccent   = {  20,  60, 200, 255}; 
            uiSuccess  = {  20, 160,  40, 255}; 
            uiWarning  = { 240, 200,  20, 255}; 
            uiDanger   = { 200,  20,  20, 255}; 
            uiInfo     = { 160, 160, 170, 255}; 
            break;

        case 10: // Tellarknight Astral (Light)
            uiBg       = { 240, 245, 250, 255}; // Silver/White Armor
            uiPanel    = { 220, 225, 235, 255}; 
            uiPanelHov = { 200, 205, 215, 255}; 
            uiBorder   = { 212, 175,  55, 255}; // Gold Trim
            uiText     = {  20,  25,  45, 255}; // Deep Space Blue Text
            uiTextDim  = { 120, 130, 150, 255}; 
            uiAccent   = {  50, 180, 255, 255}; // Constellar Cyan
            uiSuccess  = { 240, 100, 180, 255}; // Caduceus Magenta
            uiWarning  = { 212, 175,  55, 255}; // Gold
            uiDanger   = { 220,  40, 110, 255}; 
            uiInfo     = {  20,  25,  45, 255}; 
            break;

        case 11: // Tellarknight Cosmic (Dark)
            uiBg       = {  15,  10,  30, 255}; // Void Space Purple
            uiPanel    = {  25,  20,  45, 255}; // Dark Nebula Blue
            uiPanelHov = {  40,  30,  65, 255}; 
            uiBorder   = { 212, 175,  55, 255}; // Radiant Gold Trim
            uiText     = { 245, 245, 255, 255}; // Starlight White Text
            uiTextDim  = { 140, 130, 170, 255}; // Muted Purple Text
            uiAccent   = {  50, 180, 255, 255}; // Constellar Cyan (Swords/Wings)
            uiSuccess  = { 240, 100, 180, 255}; // Caduceus Magenta (Gems)
            uiWarning  = { 245, 215,  80, 255}; // Brilliant Gold
            uiDanger   = { 220,  40,  80, 255}; 
            uiInfo     = {  70, 120, 255, 255}; 
            break;
    }
}

inline void cycleTheme() {
    applyTheme((currentThemeIdx + 1) % TOTAL_THEMES);
}