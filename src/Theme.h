#ifndef NUTSPICE_THEME_H
#define NUTSPICE_THEME_H

#include <SDL2/SDL.h>
#include <vector>

namespace nutspice
{

    struct ThemeColors
    {
        SDL_Color background;
        SDL_Color text;
        SDL_Color circuitBg;
        SDL_Color plotBg;
        SDL_Color button;
        SDL_Color buttonText;
        SDL_Color toolbar;
        SDL_Color gridLine;  // NEW: dedicated grid colour (was hard-coded alpha trick)
        SDL_Color highlight; // NEW: accent for selected items / placement preview
    };

    extern ThemeColors lightTheme;
    extern ThemeColors darkTheme;
    extern ThemeColors currentTheme;

    extern const SDL_Color WHITE;
    extern const SDL_Color BLACK;
    extern const SDL_Color RED;
    extern const SDL_Color GREEN;
    extern const SDL_Color BLUE;
    extern const SDL_Color GRAY;

    // Palette used when assigning auto-colours to plot signals.
    extern const std::vector<SDL_Color> colorPalette;

    void applyDarkMode(bool dark);

} // namespace nutspice

#endif // NUTSPICE_THEME_H
