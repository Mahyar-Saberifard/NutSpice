#ifndef NUTSPICE_PLACEMENT_H
#define NUTSPICE_PLACEMENT_H

#include "Component.h"
#include "Circuit.h"
#include "UI.h"

#include <SDL2/SDL.h>
#include <map>
#include <string>
#include <unordered_map>

namespace nutspice
{

    // ---- Global GUI state (was a flat pile of globals in the original) --------
    extern std::unordered_map<std::string, int> nodeMap;
    extern std::unordered_map<int, std::string> reverseNodeMap;
    extern int nextNodeNumber;
    extern std::map<int, SDL_Point> nodePositions;

    extern Component *selectedComponent;
    extern PlacementMode currentPlacementMode;
    extern bool isPlacingComponent;
    extern SDL_Point placementStartPoint;

    // For dependent-source placement we need up to 4 click points.  The state
    // machine tracks which click we are waiting for.
    struct PlacementState
    {
        int clicksSoFar = 0;
        int outNode1 = -1;
        int outNode2 = -1;
        int ctrlNode1 = -1;
        int ctrlNode2 = -1;
        // For CCVS/CCCS: the name of the controlling voltage source, entered
        // via the value text box after the second click.
        std::string controllingSourceName;
    };
    extern PlacementState placementState;

    // Text boxes used by the GUI.  Defined here so multiple translation units
    // can refer to them.
    extern TextBox valueBox;
    extern TextBox nameBox;
    extern TextBox stepBox;
    extern TextBox stopBox;
    extern TextBox sweepSourceBox;
    extern TextBox sweepStartBox;
    extern TextBox sweepStopBox;
    extern TextBox sweepStepBox;
    extern TextBox ampBox, freqBox, phaseBox, offsetBox;
    extern TextBox v1Box, v2Box, tdBox, trBox, tfBox, pwBox, perBox;
    extern TextBox *activeTextBox;

    // ---- Node helpers ----------------------------------------------------------
    int getOrCreateNode(const std::string &nodeName);
    std::string getNodeName(int nodeNumber);
    int findOrCreateNodeAt(int x, int y);
    void calculateNodePositions(const Circuit &circuit);

    // ---- Selection / placement ------------------------------------------------
    void handleComponentSelection(const Circuit *circuit, int x, int y);
    void handleComponentPlacement(Circuit *circuit, int x, int y);
    void resetPlacementState();

} // namespace nutspice

#endif // NUTSPICE_PLACEMENT_H
