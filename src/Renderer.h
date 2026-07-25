#ifndef NUTSPICE_RENDERER_H
#define NUTSPICE_RENDERER_H

#include "Circuit.h"
#include "UI.h"

#include <SDL2/SDL.h>
#include <string>
#include <vector>

namespace nutspice
{

    // Draws every component in `circuit` plus the node markers.
    void drawCircuit(const Circuit &circuit, SDL_Renderer *renderer);

    // Draws the top toolbar (File / Edit / Analyze / Place / Dark-mode toggle).
    void drawToolbar(SDL_Renderer *renderer);

    // Status line at the bottom of the window.
    void renderStatus(SDL_Renderer *renderer);

    // Floating shortcut-help panel.
    void renderShortcutHelp(SDL_Renderer *renderer);

    // Component-library panel (the "Place" drop-down).
    void renderComponentLibrary(SDL_Renderer *renderer);
    void handleComponentLibraryClick(int x, int y);

    // Drop-down menus.
    void showFileMenu(SDL_Renderer *renderer, SDL_Rect menuRect);
    void showEditMenu(SDL_Renderer *renderer, SDL_Rect menuRect);
    void showAnalysisSettings(SDL_Renderer *renderer);
    void showFileDialog(SDL_Renderer *renderer, const std::vector<std::string> &files);
    void showSaveAsDialog(SDL_Renderer *renderer, const std::string &currentFilename);
    void showComponentProperties(SDL_Renderer *renderer, Component *component);

    // Parameter dialogs for sin / pulse sources.
    void showSinParametersDialog(SDL_Renderer *renderer);
    void showPulseParametersDialog(SDL_Renderer *renderer);

} // namespace nutspice

#endif // NUTSPICE_RENDERER_H
