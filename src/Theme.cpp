#include "Theme.h"

namespace nutspice
{

    ThemeColors lightTheme = {
        /*background*/ {245, 245, 247, 255},
        /*text*/ {30, 30, 35, 255},
        /*circuitBg*/ {255, 255, 255, 255},
        /*plotBg*/ {255, 255, 255, 255},
        /*button*/ {225, 226, 230, 255},
        /*buttonText*/ {30, 30, 35, 255},
        /*toolbar*/ {232, 233, 236, 255},
        /*gridLine*/ {180, 182, 188, 255},
        /*highlight*/ {0, 120, 215, 255}};

    ThemeColors darkTheme = {
        /*background*/ {34, 38, 46, 255},
        /*text*/ {225, 228, 234, 255},
        /*circuitBg*/ {50, 56, 66, 255},
        /*plotBg*/ {42, 48, 58, 255},
        /*button*/ {70, 78, 90, 255},
        /*buttonText*/ {230, 232, 238, 255},
        /*toolbar*/ {28, 32, 40, 255},
        /*gridLine*/ {80, 88, 100, 255},
        /*highlight*/ {80, 180, 255, 255}};

    ThemeColors currentTheme = lightTheme;

    const SDL_Color WHITE = {255, 255, 255, 255};
    const SDL_Color BLACK = {0, 0, 0, 255};
    const SDL_Color RED = {220, 60, 60, 255};
    const SDL_Color GREEN = {60, 180, 90, 255};
    const SDL_Color BLUE = {60, 120, 220, 255};
    const SDL_Color GRAY = {160, 160, 170, 255};

    const std::vector<SDL_Color> colorPalette = {
        {255, 60, 60, 255},
        {60, 200, 100, 255},
        {60, 120, 255, 255},
        {255, 215, 60, 255},
        {220, 80, 220, 255},
        {60, 220, 220, 255},
        {255, 150, 60, 255},
        {170, 90, 220, 255},
        {200, 110, 80, 255},
        {90, 180, 60, 255}};

    void applyDarkMode(bool dark)
    {
        currentTheme = dark ? darkTheme : lightTheme;
    }

} // namespace nutspice
