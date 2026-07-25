#ifndef NUTSPICE_UI_H
#define NUTSPICE_UI_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>

namespace nutspice
{

    // ---- Layout ---------------------------------------------------------------
    extern const int SCREEN_WIDTH;
    extern const int SCREEN_HEIGHT;

    extern SDL_Rect circuitArea;
    extern SDL_Rect plotArea;
    extern SDL_Rect fileMenuRect;
    extern SDL_Rect editMenuRect;

    // ---- Globals (owned by main.cpp) ------------------------------------------
    extern SDL_Window *window;
    extern SDL_Renderer *renderer;
    extern TTF_Font *font;

    // ---- PODs -----------------------------------------------------------------
    struct Button
    {
        SDL_Rect rect;
        std::string text;
        SDL_Color color;
        bool isActive;
    };

    struct TextBox
    {
        SDL_Rect rect;
        std::string text;
        bool isActive;
    };

    // ---- Helpers --------------------------------------------------------------
    inline bool isMouseOver(const SDL_Rect &rect, int x, int y)
    {
        return x >= rect.x && x <= rect.x + rect.w &&
               y >= rect.y && y <= rect.y + rect.h;
    }

    bool isPointNearLine(int px, int py, int x1, int y1, int x2, int y2, int threshold);
    void getPerpendicularPoints(int x1, int y1, int x2, int y2, int offset,
                                int &outX1, int &outY1, int &outX2, int &outY2);

    // All rendering goes through currentTheme (see Theme.h).
    void renderText(const std::string &text, int x, int y,
                    SDL_Color color = currentTheme.text);
    void renderButton(const Button &button);
    void renderTextBox(const TextBox &box);

} // namespace nutspice

#endif // NUTSPICE_UI_H
