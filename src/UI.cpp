#include "UI.h"
#include "Theme.h"

#include <cmath>

namespace nutspice
{

    const int SCREEN_WIDTH = 1280;
    const int SCREEN_HEIGHT = 720;

    SDL_Rect circuitArea = {50, 50, 800, 620};
    SDL_Rect plotArea = {870, 50, 360, 620};
    SDL_Rect fileMenuRect = {10, 45, 140, 250};
    SDL_Rect editMenuRect = {100, 45, 140, 250};

    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    TTF_Font *font = nullptr;

    bool isPointNearLine(int px, int py, int x1, int y1, int x2, int y2, int threshold)
    {
        float lineLength = std::sqrt(float((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1)));
        if (lineLength == 0.0f)
            return false;

        float u = ((px - x1) * (x2 - x1) + (py - y1) * (y2 - y1)) / (lineLength * lineLength);
        u = std::fmax(0.0f, std::fmin(1.0f, u));

        float closestX = x1 + u * (x2 - x1);
        float closestY = y1 + u * (y2 - y1);
        float distance = std::sqrt((px - closestX) * (px - closestX) +
                                   (py - closestY) * (py - closestY));
        return distance <= threshold;
    }

    void getPerpendicularPoints(int x1, int y1, int x2, int y2, int offset,
                                int &outX1, int &outY1, int &outX2, int &outY2)
    {
        int dx = x2 - x1;
        int dy = y2 - y1;
        float length = std::sqrt(float(dx * dx + dy * dy));
        if (length == 0.0f)
        {
            outX1 = x1;
            outY1 = y1;
            outX2 = x2;
            outY2 = y2;
            return;
        }
        float nx = -dy / length;
        float ny = dx / length;
        outX1 = static_cast<int>(x1 + nx * offset);
        outY1 = static_cast<int>(y1 + ny * offset);
        outX2 = static_cast<int>(x2 + nx * offset);
        outY2 = static_cast<int>(y2 + ny * offset);
    }

    void renderText(const std::string &text, int x, int y, SDL_Color color)
    {
        if (!font || text.empty())
            return;

        SDL_Surface *surface = TTF_RenderText_Blended(font, text.c_str(), color);
        if (!surface)
            return;
        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture)
        {
            SDL_Rect rect = {x, y, surface->w, surface->h};
            SDL_RenderCopy(renderer, texture, nullptr, &rect);
            SDL_DestroyTexture(texture);
        }
        SDL_FreeSurface(surface);
    }

    void renderButton(const Button &button)
    {
        SDL_SetRenderDrawColor(renderer, button.color.r, button.color.g, button.color.b, 255);
        SDL_RenderFillRect(renderer, &button.rect);
        SDL_SetRenderDrawColor(renderer,
                               currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 255);
        SDL_RenderDrawRect(renderer, &button.rect);
        renderText(button.text, button.rect.x + 6, button.rect.y + 6, currentTheme.buttonText);
    }

    void renderTextBox(const TextBox &box)
    {
        SDL_SetRenderDrawColor(renderer,
                               currentTheme.circuitBg.r, currentTheme.circuitBg.g,
                               currentTheme.circuitBg.b, 255);
        SDL_RenderFillRect(renderer, &box.rect);
        SDL_Color border = box.isActive ? currentTheme.highlight : currentTheme.text;
        SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, 255);
        SDL_RenderDrawRect(renderer, &box.rect);

        std::string displayText = box.text;
        if (displayText.empty())
        {
            if (box.rect.w < 100)
                displayText = "...";
            else if (box.rect.w < 150)
                displayText = "Enter...";
            else
                displayText = "Enter value...";
        }
        renderText(displayText, box.rect.x + 5, box.rect.y + 5);
    }

} // namespace nutspice
