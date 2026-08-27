#include "Platform.h"
#include "Theme.h"
#include "UI.h"
#include "Component.h"
#include "Circuit.h"
#include "Passives.h"
#include "Sources.h"
#include "DependentSources.h"
#include "Parser.h"
#include "Placement.h"
#include "Renderer.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace nutspice
{

    // ---- Shared GUI flags (declared extern in Renderer.cpp / Placement.h) ------
    bool FileMenu = false;
    bool EditMenu = false;
    bool ComponentLibrary = false;
    bool AnalysisSettings = false;
    bool showShortcutHelp = false;
    bool FileDialog = false;
    bool SaveAsDialog = false;
    bool showSinParams = false;
    bool showPulseParams = false;
    bool isVoltageSource = false;
    bool darkMode = false;

    bool running = true;
    SDL_Event event;
    std::string inputText;
    bool textInputActive = false;

    std::vector<double> voltages;
    std::vector<double> currents;
    std::vector<double> Vtimes;
    std::vector<double> Itimes;

    // Plot state — used by Circuit::drawLTspiceStylePlot via the mouse wheel.
    double timeZoom = 1.0;
    double valueZoom = 1.0;
    double timePan = 0.0;
    double valuePan = 0.0;

    // Undo / redo stacks (kept here because they hold Circuit*).
    std::vector<Circuit *> undoStack;
    std::vector<Circuit *> redoStack;
    std::string copiedComponent;

    // Forward declarations of small helpers used inside run().
    bool plotSignalsEmpty(const Circuit &c);
    std::string placementModeLabel(PlacementMode m);

    // ---- Helpers ---------------------------------------------------------------
    void saveUndoState(Circuit *&currentCircuit)
    {
        for (auto *c : redoStack)
            delete c;
        redoStack.clear();
        Circuit *copy = new Circuit(*currentCircuit);
        copy->nodePositions = currentCircuit->nodePositions;
        undoStack.push_back(copy);
        if (undoStack.size() > 20)
        {
            delete undoStack.front();
            undoStack.erase(undoStack.begin());
        }
    }

    void undo(Circuit *&currentCircuit)
    {
        if (undoStack.empty())
            return;
        Circuit *redoState = new Circuit(*currentCircuit);
        redoState->nodePositions = currentCircuit->nodePositions;
        redoStack.push_back(redoState);
        delete currentCircuit;
        currentCircuit = new Circuit(*undoStack.back());
        currentCircuit->nodePositions = undoStack.back()->nodePositions;
        undoStack.pop_back();
    }

    void redo(Circuit *&currentCircuit)
    {
        if (redoStack.empty())
            return;
        Circuit *undoState = new Circuit(*currentCircuit);
        undoState->nodePositions = currentCircuit->nodePositions;
        undoStack.push_back(undoState);
        delete currentCircuit;
        currentCircuit = new Circuit(*redoStack.back());
        currentCircuit->nodePositions = redoStack.back()->nodePositions;
        redoStack.pop_back();
    }

    void resetGlobalState()
    {
        nodeMap.clear();
        reverseNodeMap.clear();
        nextNodeNumber = 1;
        nodePositions.clear();
        voltages.clear();
        currents.clear();
        Vtimes.clear();
        Itimes.clear();
    }

    bool initSDL()
    {
        if (SDL_Init(SDL_INIT_VIDEO) < 0)
        {
            std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
            return false;
        }
        if (TTF_Init() == -1)
        {
            std::cerr << "SDL_ttf could not initialize! TTF_Error: " << TTF_GetError() << std::endl;
            return false;
        }
        if (IMG_Init(IMG_INIT_PNG) == 0)
        {
            std::cerr << "SDL_image could not initialize! IMG_Error: " << IMG_GetError() << std::endl;
            // Non-fatal — we just won't have an icon.
        }

        window = SDL_CreateWindow("NutSpice 2.0", SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED,
                                  SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
        if (!window)
        {
            std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
            return false;
        }

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer)
        {
            std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
            return false;
        }

        // Cross-platform font lookup — no more hard-coded Windows path.
        std::string fontPath = locateFont({});
        if (fontPath.empty())
        {
            std::cerr << "No usable font found on this system." << std::endl;
            return false;
        }
        font = TTF_OpenFont(fontPath.c_str(), 18);
        if (!font)
        {
            std::cerr << "Failed to load font '" << fontPath
                      << "': " << TTF_GetError() << std::endl;
            return false;
        }

        // Optional window icon — silently ignored if the asset isn't found.
        std::string iconPath = locateAsset("Icon1.png");
        if (!iconPath.empty())
        {
            SDL_Surface *icon = IMG_Load(iconPath.c_str());
            if (icon)
            {
                SDL_SetWindowIcon(window, icon);
                SDL_FreeSurface(icon);
            }
        }
        return true;
    }

    // =============================================================================
    //  Main loop
    // =============================================================================
    int run(int argc, char *argv[])
    {
        if (!initSDL())
            return 1;

        std::string currentCircuitFile;
        Circuit *circuit = new Circuit();

        std::vector<std::string> circuitFiles = listFiles(".", {".txt", ".cir"});

        nodePositions[0] = {100, 500};
        reverseNodeMap[0] = "GND";
        nextNodeNumber = 1;

        while (running)
        {
            if (undoStack.empty())
                saveUndoState(circuit);

            while (SDL_PollEvent(&event))
            {
                if (event.type == SDL_QUIT)
                {
                    running = false;
                }
                else if (event.type == SDL_MOUSEBUTTONDOWN)
                {
                    int x, y;
                    SDL_GetMouseState(&x, &y);

                    // Dark-mode toggle button.
                    if (x >= 390 && x <= 530 && y >= 5 && y <= 35)
                    {
                        darkMode = !darkMode;
                        applyDarkMode(darkMode);
                        continue;
                    }

                    // Top toolbar.
                    if (y <= 40)
                    {
                        if (x >= 10 && x <= 90)
                        {
                            FileMenu = !FileMenu;
                            EditMenu = false;
                            ComponentLibrary = false;
                            AnalysisSettings = false;
                            continue;
                        }
                        else if (x >= 100 && x <= 180)
                        {
                            EditMenu = !EditMenu;
                            FileMenu = false;
                            ComponentLibrary = false;
                            AnalysisSettings = false;
                            continue;
                        }
                        else if (x >= 190 && x <= 290)
                        {
                            AnalysisSettings = !AnalysisSettings;
                            FileMenu = false;
                            EditMenu = false;
                            ComponentLibrary = false;
                            continue;
                        }
                        else if (x >= 300 && x <= 380)
                        {
                            ComponentLibrary = !ComponentLibrary;
                            FileMenu = false;
                            EditMenu = false;
                            AnalysisSettings = false;
                            continue;
                        }
                    }

                    // File-menu items.
                    if (FileMenu)
                    {
                        if (x >= fileMenuRect.x + 10 && x <= fileMenuRect.x + 130)
                        {
                            int item = (y - fileMenuRect.y - 40) / 40;
                            if (item == 0)
                            { // New
                                resetGlobalState();
                                delete circuit;
                                circuit = new Circuit();
                                currentCircuitFile.clear();
                                FileMenu = false;
                            }
                            else if (item == 1)
                            { // Open
                                FileDialog = true;
                                circuitFiles = listFiles(".", {".txt", ".cir"});
                                FileMenu = false;
                            }
                            else if (item == 2)
                            { // Save
                                if (!currentCircuitFile.empty())
                                    saveCircuitToFile(*circuit, currentCircuitFile);
                                else
                                    SaveAsDialog = true;
                                FileMenu = false;
                            }
                            else if (item == 3)
                            { // Save As
                                SaveAsDialog = true;
                                FileMenu = false;
                            }
                            else if (item == 4)
                            { // Exit
                                running = false;
                            }
                        }
                        continue;
                    }
                    if (EditMenu)
                    {
                        if (x >= editMenuRect.x + 10 && x <= editMenuRect.x + 130)
                        {
                            int item = (y - editMenuRect.y - 40) / 40;
                            if (item == 0 && !undoStack.empty())
                                undo(circuit);
                            else if (item == 1 && !redoStack.empty())
                                redo(circuit);
                            else if (item == 2 && selectedComponent)
                            {
                                copiedComponent = selectedComponent->getInfo();
                            }
                            else if (item == 3 && !copiedComponent.empty())
                            {
                                // Paste is intentionally minimal — full re-parse
                                // of the copied info line.
                                std::istringstream iss(copiedComponent);
                                std::string type, name, n1, n2, valStr;
                                iss >> type >> name >> n1 >> n2 >> valStr;
                                static int pasteCount = 1;
                                std::string newName = name + "_copy" + std::to_string(pasteCount++);
                                Component *newComp = nullptr;
                                if (type == "Resistor")
                                    newComp = new Resistor(newName, getOrCreateNode(n1), getOrCreateNode(n2), std::stod(valStr));
                                else if (type == "Capacitor")
                                    newComp = new Capacitor(newName, getOrCreateNode(n1), getOrCreateNode(n2), std::stod(valStr));
                                else if (type == "Inductor")
                                    newComp = new Inductor(newName, getOrCreateNode(n1), getOrCreateNode(n2), std::stod(valStr));
                                else if (type == "Voltage")
                                    newComp = new VoltageSource(newName, getOrCreateNode(n1), getOrCreateNode(n2), std::stod(valStr));
                                else if (type == "Current")
                                    newComp = new CurrentSource(newName, getOrCreateNode(n1), getOrCreateNode(n2), std::stod(valStr));
                                else if (type == "Ground")
                                    newComp = new Ground(newName, getOrCreateNode(n1));
                                if (newComp)
                                {
                                    saveUndoState(circuit);
                                    circuit->addComponent(newComp);
                                    calculateNodePositions(*circuit);
                                }
                            }
                            else if (item == 4 && selectedComponent)
                            {
                                saveUndoState(circuit);
                                circuit->deleteComponent(selectedComponent->name);
                                selectedComponent = nullptr;
                                calculateNodePositions(*circuit);
                            }
                            EditMenu = false;
                        }
                        continue;
                    }
                    if (AnalysisSettings)
                    {
                        SDL_Rect w = {250, 150, 360, 380};
                        if (x >= w.x + 10 && x <= w.x + 110 && y >= w.y + 330 && y <= w.y + 370)
                        {
                            try
                            {
                                if (circuit->currentMode == Circuit::TRANSIENT)
                                {
                                    circuit->analyzeTransient(std::stod(stepBox.text),
                                                              std::stod(stopBox.text));
                                }
                                else
                                {
                                    circuit->analyzeDC();
                                }
                                AnalysisSettings = false;
                            }
                            catch (const std::exception &e)
                            {
                                std::cerr << "Analysis error: " << e.what() << std::endl;
                            }
                        }
                        else if (x >= w.x + 130 && x <= w.x + 230 &&
                                 y >= w.y + 330 && y <= w.y + 370)
                        {
                            AnalysisSettings = false;
                        }
                        else if (x >= w.x + 250 && x <= w.x + 350 &&
                                 y >= w.y + 330 && y <= w.y + 370)
                        {
                            try
                            {
                                circuit->currentMode = Circuit::SWEEP;
                                circuit->analyzeDCSweep(sweepSourceBox.text,
                                                        std::stod(sweepStartBox.text),
                                                        std::stod(sweepStopBox.text),
                                                        std::stod(sweepStepBox.text));
                                AnalysisSettings = false;
                            }
                            catch (const std::exception &e)
                            {
                                std::cerr << "DC Sweep error: " << e.what() << std::endl;
                            }
                        }
                        else if (x >= 400 && x <= 550 && y >= 210 && y <= 240)
                        {
                            textInputActive = true;
                            activeTextBox = &stepBox;
                            inputText = stepBox.text;
                        }
                        else if (x >= 400 && x <= 550 && y >= 260 && y <= 290)
                        {
                            textInputActive = true;
                            activeTextBox = &stopBox;
                            inputText = stopBox.text;
                        }
                        else if (x >= 400 && x <= 550 && y >= 310 && y <= 340)
                        {
                            textInputActive = true;
                            activeTextBox = &sweepSourceBox;
                            inputText = sweepSourceBox.text;
                        }
                        else if (x >= 400 && x <= 550 && y >= 350 && y <= 380)
                        {
                            textInputActive = true;
                            activeTextBox = &sweepStartBox;
                            inputText = sweepStartBox.text;
                        }
                        else if (x >= 400 && x <= 550 && y >= 390 && y <= 420)
                        {
                            textInputActive = true;
                            activeTextBox = &sweepStopBox;
                            inputText = sweepStopBox.text;
                        }
                        else if (x >= 400 && x <= 550 && y >= 430 && y <= 460)
                        {
                            textInputActive = true;
                            activeTextBox = &sweepStepBox;
                            inputText = sweepStepBox.text;
                        }
                        continue;
                    }
                    if (FileDialog)
                    {
                        SDL_Rect dialog = {200, 150, 400, 400};
                        bool fileClicked = false;
                        for (size_t i = 0; i < circuitFiles.size(); i++)
                        {
                            SDL_Rect fileRect = {dialog.x + 20, dialog.y + 60 + (int)i * 30, 360, 25};
                            if (x >= fileRect.x && x <= fileRect.x + fileRect.w &&
                                y >= fileRect.y && y <= fileRect.y + fileRect.h)
                            {
                                resetGlobalState();
                                delete circuit;
                                circuit = new Circuit();
                                processCircuitFile(circuitFiles[i], *circuit);
                                currentCircuitFile = circuitFiles[i];
                                calculateNodePositions(*circuit);
                                circuit->updateAnalysisMode();
                                FileDialog = false;
                                fileClicked = true;
                                break;
                            }
                        }
                        if (!fileClicked)
                        {
                            if (x >= dialog.x + 100 && x <= dialog.x + 200 &&
                                y >= dialog.y + 330 && y <= dialog.y + 370)
                            {
                                FileDialog = false;
                            }
                            else if (x >= dialog.x + 220 && x <= dialog.x + 320 &&
                                     y >= dialog.y + 330 && y <= dialog.y + 370)
                            {
                                FileDialog = false;
                            }
                        }
                        continue;
                    }
                    if (SaveAsDialog)
                    {
                        SDL_Rect dialog = {250, 200, 300, 200};
                        if (x >= dialog.x + 50 && x <= dialog.x + 150 &&
                            y >= dialog.y + 120 && y <= dialog.y + 160)
                        {
                            saveCircuitToFile(*circuit, nameBox.text);
                            currentCircuitFile = nameBox.text;
                            SaveAsDialog = false;
                        }
                        else if (x >= dialog.x + 170 && x <= dialog.x + 270 &&
                                 y >= dialog.y + 120 && y <= dialog.y + 160)
                        {
                            SaveAsDialog = false;
                        }
                        else if (x >= dialog.x + 100 && x <= dialog.x + 280 &&
                                 y >= dialog.y + 60 && y <= dialog.y + 90)
                        {
                            textInputActive = true;
                            activeTextBox = &nameBox;
                            inputText = nameBox.text;
                        }
                        continue;
                    }

                    // Value box click → activate text input.
                    if (x >= valueBox.rect.x && x <= valueBox.rect.x + valueBox.rect.w &&
                        y >= valueBox.rect.y && y <= valueBox.rect.y + valueBox.rect.h)
                    {
                        textInputActive = true;
                        activeTextBox = &valueBox;
                        inputText = valueBox.text;
                        continue;
                    }
                    textInputActive = false;
                    activeTextBox = nullptr;

                    // Component-library panel click.
                    if (ComponentLibrary)
                    {
                        handleComponentLibraryClick(x, y);
                        continue;
                    }

                    // Circuit canvas click.
                    if (x >= circuitArea.x && x <= circuitArea.x + circuitArea.w &&
                        y >= circuitArea.y && y <= circuitArea.y + circuitArea.h)
                    {
                        if (currentPlacementMode != PLACE_NONE)
                        {
                            saveUndoState(circuit);
                            handleComponentPlacement(circuit, x, y);
                        }
                        else
                        {
                            handleComponentSelection(circuit, x, y);
                        }
                        continue;
                    }
                }
                else if (event.type == SDL_KEYDOWN && textInputActive)
                {
                    if (event.key.keysym.sym == SDLK_RETURN)
                    {
                        textInputActive = false;
                        if (activeTextBox)
                            activeTextBox->text = inputText;
                    }
                    else if (event.key.keysym.sym == SDLK_BACKSPACE && !inputText.empty())
                    {
                        inputText.pop_back();
                    }
                    else if (event.key.keysym.sym == SDLK_ESCAPE)
                    {
                        textInputActive = false;
                        inputText.clear();
                    }
                }
                else if (event.type == SDL_TEXTINPUT && textInputActive)
                {
                    inputText += event.text.text;
                }
                else if (event.type == SDL_MOUSEWHEEL)
                {
                    // Use the GLOBAL zoom/pan (the original shadowed them with
                    // local redeclarations and zoom never worked).
                    if (SDL_GetModState() & KMOD_CTRL)
                    {
                        if (event.wheel.y > 0)
                            valueZoom *= 1.2;
                        else if (event.wheel.y < 0)
                            valueZoom /= 1.2;
                    }
                    else
                    {
                        if (event.wheel.y > 0)
                        {
                            timeZoom *= 1.2;
                            valueZoom *= 1.2;
                        }
                        else if (event.wheel.y < 0)
                        {
                            timeZoom /= 1.2;
                            valueZoom /= 1.2;
                        }
                    }
                    timeZoom = std::max(0.1, std::min(timeZoom, 20.0));
                    valueZoom = std::max(0.1, std::min(valueZoom, 20.0));
                }
                else if (event.type == SDL_KEYDOWN)
                {
                    if (event.key.keysym.sym == SDLK_ESCAPE && currentPlacementMode != PLACE_NONE)
                    {
                        currentPlacementMode = PLACE_NONE;
                        resetPlacementState();
                    }
                    else if (event.key.keysym.sym == SDLK_h)
                    {
                        showShortcutHelp = !showShortcutHelp;
                    }
                    else if (event.key.keysym.sym == SDLK_DELETE && selectedComponent)
                    {
                        saveUndoState(circuit);
                        circuit->deleteComponent(selectedComponent->name);
                        selectedComponent = nullptr;
                        calculateNodePositions(*circuit);
                    }
                    else if (event.key.keysym.mod & KMOD_SHIFT)
                    {
                        switch (event.key.keysym.sym)
                        {
                        case SDLK_d:
                            currentPlacementMode = PLACE_DIODE;
                            resetPlacementState();
                            break;
                        case SDLK_g:
                            currentPlacementMode = PLACE_GROUND;
                            resetPlacementState();
                            break;
                        default:
                            break;
                        }
                    }
                    else
                    {
                        switch (event.key.keysym.sym)
                        {
                        case SDLK_r:
                            currentPlacementMode = PLACE_RESISTOR;
                            resetPlacementState();
                            break;
                        case SDLK_c:
                            currentPlacementMode = PLACE_CAPACITOR;
                            resetPlacementState();
                            break;
                        case SDLK_l:
                            currentPlacementMode = PLACE_INDUCTOR;
                            resetPlacementState();
                            break;
                        case SDLK_v:
                            currentPlacementMode = PLACE_VOLTAGE_SOURCE;
                            resetPlacementState();
                            break;
                        case SDLK_i:
                            currentPlacementMode = PLACE_CURRENT_SOURCE;
                            resetPlacementState();
                            break;
                        case SDLK_w:
                            currentPlacementMode = PLACE_WIRE;
                            resetPlacementState();
                            break;
                        default:
                            break;
                        }
                    }
                }
            }

            // Live-update the active text box with the current input text.
            if (textInputActive && activeTextBox)
            {
                activeTextBox->text = inputText;
            }

            // ---- Render -------------------------------------------------------
            SDL_SetRenderDrawColor(renderer, currentTheme.background.r,
                                   currentTheme.background.g, currentTheme.background.b, 255);
            SDL_RenderClear(renderer);

            SDL_SetRenderDrawColor(renderer, currentTheme.circuitBg.r,
                                   currentTheme.circuitBg.g, currentTheme.circuitBg.b, 255);
            SDL_RenderFillRect(renderer, &circuitArea);
            SDL_SetRenderDrawColor(renderer, currentTheme.text.r,
                                   currentTheme.text.g, currentTheme.text.b, 255);
            SDL_RenderDrawRect(renderer, &circuitArea);

            SDL_SetRenderDrawColor(renderer, currentTheme.plotBg.r,
                                   currentTheme.plotBg.g, currentTheme.plotBg.b, 255);
            SDL_RenderFillRect(renderer, &plotArea);
            SDL_SetRenderDrawColor(renderer, currentTheme.text.r,
                                   currentTheme.text.g, currentTheme.text.b, 255);
            SDL_RenderDrawRect(renderer, &plotArea);

            if (circuit)
                drawCircuit(*circuit, renderer);
            if (!plotSignalsEmpty(*circuit))
                circuit->drawLTspiceStylePlot(renderer, plotArea);

            drawToolbar(renderer);
            renderTextBox(valueBox);
            renderStatus(renderer);

            if (selectedComponent && !FileMenu && !EditMenu && !AnalysisSettings &&
                !FileDialog && !SaveAsDialog)
            {
                showComponentProperties(renderer, selectedComponent);
            }
            if (FileMenu)
                showFileMenu(renderer, fileMenuRect);
            if (EditMenu)
                showEditMenu(renderer, editMenuRect);
            if (ComponentLibrary)
                renderComponentLibrary(renderer);
            if (AnalysisSettings)
                showAnalysisSettings(renderer);
            if (FileDialog)
                showFileDialog(renderer, circuitFiles);
            if (SaveAsDialog)
                showSaveAsDialog(renderer, nameBox.text);
            if (showSinParams)
                showSinParametersDialog(renderer);
            if (showPulseParams)
                showPulseParametersDialog(renderer);
            if (showShortcutHelp)
                renderShortcutHelp(renderer);

            // Placement-mode status line.
            if (currentPlacementMode != PLACE_NONE)
            {
                std::string modeName = placementModeLabel(currentPlacementMode);
                std::string msg = "Placing " + modeName +
                                  (isPlacingComponent ? " - click next point (ESC to cancel)"
                                                      : " - click first connection point");
                renderText(msg, 10, SCREEN_HEIGHT - 60, currentTheme.highlight);
            }

            SDL_RenderPresent(renderer);
        }

        for (auto *c : undoStack)
            delete c;
        for (auto *c : redoStack)
            delete c;
        TTF_CloseFont(font);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();

        delete circuit;
        return 0;
    }

    // Small helper used by the render section.
    bool plotSignalsEmpty(const Circuit &c)
    {
        return c.plotSignals.empty();
    }

    std::string placementModeLabel(PlacementMode m)
    {
        switch (m)
        {
        case PLACE_RESISTOR:
            return "Resistor";
        case PLACE_CAPACITOR:
            return "Capacitor";
        case PLACE_INDUCTOR:
            return "Inductor";
        case PLACE_VOLTAGE_SOURCE:
            return "Voltage Source";
        case PLACE_CURRENT_SOURCE:
            return "Current Source";
        case PLACE_DIODE:
            return "Diode";
        case PLACE_GROUND:
            return "Ground";
        case PLACE_WIRE:
            return "Wire";
        case PLACE_VCVS:
            return "VCVS (4 clicks: out+, out-, ctrl+, ctrl-)";
        case PLACE_VCCS:
            return "VCCS (4 clicks: out+, out-, ctrl+, ctrl-)";
        case PLACE_CCVS:
            return "CCVS (2 clicks + ctrl V-source name in Value box)";
        case PLACE_CCCS:
            return "CCCS (2 clicks + ctrl V-source name in Value box)";
        case PLACE_SIN_VOLTAGE_SOURCE:
            return "Sinusoidal Voltage Source";
        case PLACE_SIN_CURRENT_SOURCE:
            return "Sinusoidal Current Source";
        case PLACE_PULSE_VOLTAGE_SOURCE:
            return "Pulse Voltage Source";
        case PLACE_PULSE_CURRENT_SOURCE:
            return "Pulse Current Source";
        case PLACE_NONE:
            return "";
        }
        return "";
    }

} // namespace nutspice

int main(int argc, char *argv[])
{
    return nutspice::run(argc, argv);
}
