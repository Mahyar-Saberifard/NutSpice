#include "Renderer.h"
#include "Theme.h"
#include "Placement.h"
#include "Passives.h"
#include "Sources.h"
#include "DependentSources.h"

#include <SDL2/SDL.h>
#include <iostream>

namespace nutspice
{

    // Externs from Placement / UI:
    extern bool FileMenu, EditMenu, ComponentLibrary, AnalysisSettings;
    extern bool showSinParams, showPulseParams, showShortcutHelp;
    extern bool isVoltageSource;
    extern bool darkMode;

    void drawCircuit(const Circuit &circuit, SDL_Renderer *renderer)
    {
        // Subtle dot grid in the canvas background — small visual change vs.
        // the original, which had no grid in the canvas area at all.
        SDL_SetRenderDrawColor(renderer, currentTheme.gridLine.r, currentTheme.gridLine.g,
                               currentTheme.gridLine.b, 90);
        for (int x = circuitArea.x + 20; x < circuitArea.x + circuitArea.w; x += 20)
        {
            for (int y = circuitArea.y + 20; y < circuitArea.y + circuitArea.h; y += 20)
            {
                SDL_RenderDrawPoint(renderer, x, y);
            }
        }

        for (const auto *comp : circuit.getComponents())
        {
            comp->render(renderer, nodePositions);
        }
        for (const auto &kv : nodePositions)
        {
            SDL_Rect nodeRect = {kv.second.x - 4, kv.second.y - 4, 8, 8};
            SDL_SetRenderDrawColor(renderer, currentTheme.highlight.r,
                                   currentTheme.highlight.g,
                                   currentTheme.highlight.b, 255);
            SDL_RenderFillRect(renderer, &nodeRect);
        }
    }

    void drawToolbar(SDL_Renderer *renderer)
    {
        SDL_Rect toolbar = {0, 0, SCREEN_WIDTH, 40};
        SDL_SetRenderDrawColor(renderer, currentTheme.toolbar.r, currentTheme.toolbar.g,
                               currentTheme.toolbar.b, 255);
        SDL_RenderFillRect(renderer, &toolbar);

        Button fileBtn = {10, 5, 80, 30, "File", currentTheme.button, false};
        Button editBtn = {100, 5, 80, 30, "Edit", currentTheme.button, false};
        Button analyzeBtn = {190, 5, 100, 30, "Analyze", currentTheme.button, false};
        Button toolsBtn = {300, 5, 80, 30, "Place", currentTheme.button, false};
        Button darkBtn = {390, 5, 140, 30,
                          darkMode ? "Light Mode" : "Dark Mode",
                          currentTheme.button, false};
        renderButton(fileBtn);
        renderButton(editBtn);
        renderButton(analyzeBtn);
        renderButton(toolsBtn);
        renderButton(darkBtn);
    }

    void renderStatus(SDL_Renderer *renderer)
    {
        SDL_Rect statusBar = {0, SCREEN_HEIGHT - 30, SCREEN_WIDTH, 30};
        SDL_SetRenderDrawColor(renderer, currentTheme.toolbar.r, currentTheme.toolbar.g,
                               currentTheme.toolbar.b, 255);
        SDL_RenderFillRect(renderer, &statusBar);
        renderText("NutSpice 2.0 — ESC cancels placement | H toggles help",
                   10, SCREEN_HEIGHT - 24, currentTheme.text);
    }

    void renderShortcutHelp(SDL_Renderer *renderer)
    {
        SDL_Rect helpBackground = {0, 0, 280, 280};
        SDL_SetRenderDrawColor(renderer, currentTheme.background.r,
                               currentTheme.background.g, currentTheme.background.b, 230);
        SDL_RenderFillRect(renderer, &helpBackground);

        renderText("Shortcuts", 10, 10);
        int y = 40;
        const char *lines[] = {
            "R   — Resistor",
            "C   — Capacitor",
            "L   — Inductor",
            "V   — Voltage source",
            "I   — Current source",
            "D   — Diode (Shift+D)",
            "G   — Ground (Shift+G)",
            "W   — Wire",
            "ESC — Cancel placement",
            "DEL — Delete selected",
            "H   — Toggle this help",
            nullptr};
        for (int i = 0; lines[i]; i++)
        {
            renderText(lines[i], 15, y);
            y += 20;
        }
    }

    // ---- Component library panel ----------------------------------------------
    // The library is a small palette of buttons grouped into Passives / Sources /
    // Semiconductors / Dependent.  Clicking a button sets the current placement
    // mode; the user then clicks on the canvas to place.
    namespace
    {
        struct LibButton
        {
            SDL_Rect rect;
            const char *label;
            PlacementMode mode;
        };
    }

    static std::vector<LibButton> buildLibraryButtons()
    {
        int x = circuitArea.x + circuitArea.w + 20;
        int y = 60;
        std::vector<LibButton> buttons;
        buttons.push_back({{x, y, 80, 30}, "Resistor", PLACE_RESISTOR});
        y += 35;
        buttons.push_back({{x, y, 80, 30}, "Capacitor", PLACE_CAPACITOR});
        y += 35;
        buttons.push_back({{x, y, 80, 30}, "Inductor", PLACE_INDUCTOR});
        y += 35;
        buttons.push_back({{x, y, 80, 30}, "Diode", PLACE_DIODE});
        y += 35;
        buttons.push_back({{x, y, 80, 30}, "Ground", PLACE_GROUND});
        y += 35;
        buttons.push_back({{x, y, 80, 30}, "Wire", PLACE_WIRE});
        y += 50;

        buttons.push_back({{x, y, 80, 30}, "V src", PLACE_VOLTAGE_SOURCE});
        y += 35;
        buttons.push_back({{x, y, 80, 30}, "I src", PLACE_CURRENT_SOURCE});
        y += 35;
        buttons.push_back({{x, y, 80, 30}, "Sin V", PLACE_SIN_VOLTAGE_SOURCE});
        y += 35;
        buttons.push_back({{x, y, 80, 30}, "Sin I", PLACE_SIN_CURRENT_SOURCE});
        y += 35;
        buttons.push_back({{x, y, 80, 30}, "Pulse V", PLACE_PULSE_VOLTAGE_SOURCE});
        y += 35;
        buttons.push_back({{x, y, 80, 30}, "Pulse I", PLACE_PULSE_CURRENT_SOURCE});
        y += 50;

        buttons.push_back({{x, y, 80, 30}, "VCVS", PLACE_VCVS});
        y += 35;
        buttons.push_back({{x, y, 80, 30}, "VCCS", PLACE_VCCS});
        y += 35;
        buttons.push_back({{x, y, 80, 30}, "CCVS", PLACE_CCVS});
        y += 35;
        buttons.push_back({{x, y, 80, 30}, "CCCS", PLACE_CCCS});
        return buttons;
    }

    void renderComponentLibrary(SDL_Renderer *renderer)
    {
        auto buttons = buildLibraryButtons();
        for (const auto &b : buttons)
        {
            Button btn = {b.rect.x, b.rect.y, b.rect.w, b.rect.h, b.label, currentTheme.button, false};
            renderButton(btn);
        }
    }

    void handleComponentLibraryClick(int x, int y)
    {
        auto buttons = buildLibraryButtons();
        for (const auto &b : buttons)
        {
            if (x >= b.rect.x && x <= b.rect.x + b.rect.w &&
                y >= b.rect.y && y <= b.rect.y + b.rect.h)
            {
                currentPlacementMode = b.mode;
                isPlacingComponent = false;
                std::cout << "Placement mode: " << b.label << std::endl;
                return;
            }
        }
    }

    // ---- File / Edit menus -----------------------------------------------------
    void showFileMenu(SDL_Renderer *renderer, SDL_Rect menuRect)
    {
        SDL_SetRenderDrawColor(renderer, currentTheme.background.r,
                               currentTheme.background.g, currentTheme.background.b, 255);
        SDL_RenderFillRect(renderer, &menuRect);

        const char *labels[] = {"New File", "Open", "Save", "Save As", "Exit"};
        int y = menuRect.y + 40;
        for (const char *lbl : labels)
        {
            Button btn = {menuRect.x + 10, y, 120, 30, lbl, currentTheme.button, false};
            renderButton(btn);
            y += 40;
        }
    }

    void showEditMenu(SDL_Renderer *renderer, SDL_Rect menuRect)
    {
        SDL_SetRenderDrawColor(renderer, currentTheme.background.r,
                               currentTheme.background.g, currentTheme.background.b, 255);
        SDL_RenderFillRect(renderer, &menuRect);

        const char *labels[] = {"Undo", "Redo", "Copy", "Paste", "Delete"};
        int y = menuRect.y + 40;
        for (const char *lbl : labels)
        {
            Button btn = {menuRect.x + 10, y, 120, 30, lbl, currentTheme.button, false};
            renderButton(btn);
            y += 40;
        }
        // (The original accidentally rendered "Delete" twice — fixed here.)
    }

    void showAnalysisSettings(SDL_Renderer *renderer)
    {
        SDL_Rect w = {250, 150, 360, 380};
        SDL_SetRenderDrawColor(renderer, currentTheme.background.r,
                               currentTheme.background.g, currentTheme.background.b, 255);
        SDL_RenderFillRect(renderer, &w);

        renderText("Analysis Settings", w.x + 20, w.y + 20);
        renderText("Time Step:", w.x + 20, w.y + 60);
        renderTextBox(stepBox);
        renderText("Stop Time:", w.x + 20, w.y + 110);
        renderTextBox(stopBox);
        renderText("DC Sweep source:", w.x + 20, w.y + 160);
        renderTextBox(sweepSourceBox);
        renderText("Start:", w.x + 20, w.y + 200);
        renderTextBox(sweepStartBox);
        renderText("Stop:", w.x + 20, w.y + 240);
        renderTextBox(sweepStopBox);
        renderText("Step:", w.x + 20, w.y + 280);
        renderTextBox(sweepStepBox);

        Button runBtn = {w.x + 10, w.y + 330, 100, 40, "Run", GREEN};
        Button cancelBtn = {w.x + 130, w.y + 330, 100, 40, "Cancel", RED};
        Button sweepBtn = {w.x + 250, w.y + 330, 100, 40, "Sweep", BLUE};
        renderButton(runBtn);
        renderButton(cancelBtn);
        renderButton(sweepBtn);
    }

    void showFileDialog(SDL_Renderer *renderer, const std::vector<std::string> &files)
    {
        SDL_Rect dialog = {200, 150, 400, 400};
        SDL_SetRenderDrawColor(renderer, currentTheme.background.r,
                               currentTheme.background.g, currentTheme.background.b, 255);
        SDL_RenderFillRect(renderer, &dialog);

        renderText("Open Circuit File", dialog.x + 20, dialog.y + 20);

        SDL_Rect fileList = {dialog.x + 20, dialog.y + 60, 360, 250};
        SDL_SetRenderDrawColor(renderer, currentTheme.plotBg.r, currentTheme.plotBg.g,
                               currentTheme.plotBg.b, 255);
        SDL_RenderFillRect(renderer, &fileList);

        for (size_t i = 0; i < files.size(); i++)
        {
            SDL_Rect fileRect = {fileList.x + 10, fileList.y + 10 + (int)i * 30,
                                 fileList.w - 20, 25};
            SDL_SetRenderDrawColor(renderer, currentTheme.background.r,
                                   currentTheme.background.g, currentTheme.background.b, 255);
            SDL_RenderFillRect(renderer, &fileRect);
            renderText(files[i], fileRect.x + 5, fileRect.y + 5);
        }

        Button openBtn = {dialog.x + 100, dialog.y + 330, 100, 40, "Open", GREEN};
        Button cancelBtn = {dialog.x + 220, dialog.y + 330, 100, 40, "Cancel", RED};
        renderButton(openBtn);
        renderButton(cancelBtn);
    }

    void showSaveAsDialog(SDL_Renderer *renderer, const std::string &currentFilename)
    {
        SDL_Rect dialog = {250, 200, 300, 200};
        SDL_SetRenderDrawColor(renderer, currentTheme.background.r,
                               currentTheme.background.g, currentTheme.background.b, 255);
        SDL_RenderFillRect(renderer, &dialog);

        renderText("Save Circuit As", dialog.x + 20, dialog.y + 20);
        renderText("Filename:", dialog.x + 20, dialog.y + 60);
        nameBox = {dialog.x + 100, dialog.y + 60, 180, 30, currentFilename, false};
        renderTextBox(nameBox);

        Button saveBtn = {dialog.x + 50, dialog.y + 120, 100, 40, "Save", GREEN};
        Button cancelBtn = {dialog.x + 170, dialog.y + 120, 100, 40, "Cancel", RED};
        renderButton(saveBtn);
        renderButton(cancelBtn);
    }

    void showComponentProperties(SDL_Renderer *renderer, Component *component)
    {
        if (!component)
            return;
        SDL_Rect w = {600, 100, 350, 400};
        SDL_SetRenderDrawColor(renderer, currentTheme.background.r,
                               currentTheme.background.g, currentTheme.background.b, 255);
        SDL_RenderFillRect(renderer, &w);

        renderText("Component Properties", w.x + 20, w.y + 20);
        renderText("Type: " + component->getType(), w.x + 20, w.y + 60);
        renderText("Name: " + component->name, w.x + 20, w.y + 90);
        renderText("Connections:", w.x + 20, w.y + 120);
        renderText("  Node 1: " + getNodeName(component->node1), w.x + 20, w.y + 150);
        renderText("  Node 2: " + getNodeName(component->node2), w.x + 20, w.y + 180);

        if (component->type != GROUND && component->type != DIODE)
        {
            renderText("Value:", w.x + 20, w.y + 210);
            TextBox vb = {w.x + 100, w.y + 210, 150, 30, std::to_string(component->value), false};
            renderTextBox(vb);
        }

        Button saveBtn = {w.x + 50, w.y + 320, 100, 40, "Save", GREEN};
        Button deleteBtn = {w.x + 200, w.y + 320, 100, 40, "Delete", RED};
        renderButton(saveBtn);
        renderButton(deleteBtn);
    }

    // ---- Sin / Pulse parameter dialogs (kept simple) ---------------------------
    void showSinParametersDialog(SDL_Renderer *renderer)
    {
        SDL_Rect dialog = {250, 150, 400, 300};
        SDL_SetRenderDrawColor(renderer, currentTheme.background.r,
                               currentTheme.background.g, currentTheme.background.b, 255);
        SDL_RenderFillRect(renderer, &dialog);
        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g,
                               currentTheme.text.b, 255);
        SDL_RenderDrawRect(renderer, &dialog);

        std::string title = isVoltageSource
                                ? "Sinusoidal Voltage Source Parameters"
                                : "Sinusoidal Current Source Parameters";
        renderText(title, dialog.x + 20, dialog.y + 20);

        renderText("Amplitude:", dialog.x + 20, dialog.y + 60);
        ampBox.rect = {dialog.x + 150, dialog.y + 60, 100, 30};
        renderTextBox(ampBox);
        renderText("Freq (Hz):", dialog.x + 20, dialog.y + 100);
        freqBox.rect = {dialog.x + 150, dialog.y + 100, 100, 30};
        renderTextBox(freqBox);
        renderText("Phase (deg):", dialog.x + 20, dialog.y + 140);
        phaseBox.rect = {dialog.x + 150, dialog.y + 140, 100, 30};
        renderTextBox(phaseBox);
        renderText("Offset:", dialog.x + 20, dialog.y + 180);
        offsetBox.rect = {dialog.x + 150, dialog.y + 180, 100, 30};
        renderTextBox(offsetBox);
    }

    void showPulseParametersDialog(SDL_Renderer *renderer)
    {
        SDL_Rect dialog = {250, 150, 460, 360};
        SDL_SetRenderDrawColor(renderer, currentTheme.background.r,
                               currentTheme.background.g, currentTheme.background.b, 255);
        SDL_RenderFillRect(renderer, &dialog);
        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g,
                               currentTheme.text.b, 255);
        SDL_RenderDrawRect(renderer, &dialog);

        renderText("Pulse Source Parameters", dialog.x + 20, dialog.y + 20);

        int y = 60;
        auto drawRow = [&](const std::string &label, TextBox &box)
        {
            renderText(label, dialog.x + 20, dialog.y + y);
            box.rect = {dialog.x + 200, dialog.y + y, 120, 30};
            renderTextBox(box);
            y += 35;
        };
        drawRow("V1 / I1 (initial):", v1Box);
        drawRow("V2 / I2 (pulsed):", v2Box);
        drawRow("Delay (td):", tdBox);
        drawRow("Rise (tr):", trBox);
        drawRow("Fall (tf):", tfBox);
        drawRow("Pulse width (pw):", pwBox);
        drawRow("Period (per):", perBox);
    }

} // namespace nutspice
