#include "Placement.h"
#include "Theme.h"
#include "Parser.h"
#include "Passives.h"
#include "Sources.h"
#include "DependentSources.h"

#include <SDL2/SDL.h>
#include <algorithm>
#include <iostream>
#include <sstream>

namespace nutspice
{

    // ---- Global GUI state ------------------------------------------------------
    std::unordered_map<std::string, int> nodeMap;
    std::unordered_map<int, std::string> reverseNodeMap;
    int nextNodeNumber = 1;
    std::map<int, SDL_Point> nodePositions;

    Component *selectedComponent = nullptr;
    PlacementMode currentPlacementMode = PLACE_NONE;
    bool isPlacingComponent = false;
    SDL_Point placementStartPoint = {0, 0};
    PlacementState placementState;

    TextBox valueBox = {{10, SCREEN_HEIGHT - 60, 100, 40}, "Value", false};
    TextBox nameBox = {{300, 210, 180, 30}, "Name", false};
    TextBox stepBox = {400, 210, 150, 30, "0.01"};
    TextBox stopBox = {400, 260, 150, 30, "1"};
    TextBox sweepSourceBox = {400, 310, 150, 30, ""};
    TextBox sweepStartBox = {400, 350, 150, 30, "0"};
    TextBox sweepStopBox = {400, 390, 150, 30, "5"};
    TextBox sweepStepBox = {400, 430, 150, 30, "0.1"};
    TextBox ampBox, freqBox, phaseBox, offsetBox;
    TextBox v1Box, v2Box, tdBox, trBox, tfBox, pwBox, perBox;
    TextBox *activeTextBox = nullptr;

    // Anonymous namespace holds the running component counter used to generate
    // auto-incrementing component names (R1, R2, ...).
    namespace
    {
        int compCount = 1;
    }

    // ---- Node helpers ----------------------------------------------------------
    int getOrCreateNode(const std::string &nodeName)
    {
        if (nodeName == "GND" || nodeName == "0")
            return 0;
        auto it = nodeMap.find(nodeName);
        if (it != nodeMap.end())
            return it->second;
        int newNodeNum = nextNodeNumber++;
        nodeMap[nodeName] = newNodeNum;
        reverseNodeMap[newNodeNum] = nodeName;
        nodePositions[newNodeNum] = {100 + newNodeNum * 100, 100};
        return newNodeNum;
    }

    std::string getNodeName(int nodeNumber)
    {
        if (nodeNumber == 0)
            return "GND";
        auto it = reverseNodeMap.find(nodeNumber);
        return (it != reverseNodeMap.end()) ? it->second : "UNKNOWN";
    }

    int findOrCreateNodeAt(int x, int y)
    {
        const int THRESHOLD = 15;
        for (const auto &kv : nodePositions)
        {
            int dx = x - kv.second.x;
            int dy = y - kv.second.y;
            if (dx * dx + dy * dy < THRESHOLD * THRESHOLD)
                return kv.first;
        }
        int newNode = nextNodeNumber++;
        nodePositions[newNode] = {x, y};
        reverseNodeMap[newNode] = std::to_string(newNode);
        nodeMap[std::to_string(newNode)] = newNode;
        return newNode;
    }

    void calculateNodePositions(const Circuit &circuit)
    {
        nodePositions.clear();
        nodePositions[0] = {circuitArea.x + circuitArea.w / 2,
                            circuitArea.y + circuitArea.h - 50};

        int x = circuitArea.x + 100;
        int y = circuitArea.y + 100;
        const int nodesPerRow = 5;
        for (int i = 1; i < circuit.nextNodeNumber; i++)
        {
            nodePositions[i] = {x, y};
            x += 150;
            if (i % nodesPerRow == 0)
            {
                x = circuitArea.x + 100;
                y += 100;
            }
        }
    }

    // ---- Selection -------------------------------------------------------------
    void handleComponentSelection(const Circuit *circuit, int x, int y)
    {
        selectedComponent = nullptr;
        for (auto *comp : circuit->getComponents())
        {
            SDL_Rect bounds = comp->getBoundingBox(nodePositions);
            if (x >= bounds.x && x <= bounds.x + bounds.w &&
                y >= bounds.y && y <= bounds.y + bounds.h)
            {
                selectedComponent = comp;
                break;
            }
        }
    }

    // ---- Placement -------------------------------------------------------------
    void resetPlacementState()
    {
        placementState = PlacementState{};
        isPlacingComponent = false;
    }

    void handleComponentPlacement(Circuit *circuit, int x, int y)
    {
        // For most components we use a 2-click flow (start point + end point).
        // For VCVS / VCCS we use a 4-click flow (out+, out-, ctrl+, ctrl-).
        // For CCVS / CCCS we use a 2-click flow + a name (entered via valueBox).

        const bool needsFourClicks =
            (currentPlacementMode == PLACE_VCVS || currentPlacementMode == PLACE_VCCS);

        if (!isPlacingComponent)
        {
            placementStartPoint = {x, y};
            isPlacingComponent = true;
            placementState.clicksSoFar = 1;
            placementState.outNode1 = findOrCreateNodeAt(x, y);
            return;
        }

        // Second click: output node2 (or wire endpoint).
        if (placementState.clicksSoFar == 1)
        {
            if (currentPlacementMode == PLACE_WIRE)
            {
                int n1 = placementState.outNode1;
                int n2 = findOrCreateNodeAt(x, y);
                if (n1 != n2)
                {
                    // saveUndoState is called by main.cpp before this function.
                    std::string name = "W" + std::to_string(compCount++);
                    circuit->addComponent(new Wire(name, n1, n2));
                    std::cout << "Created wire between node " << n1 << " and " << n2 << std::endl;
                }
                resetPlacementState();
                currentPlacementMode = PLACE_NONE;
                return;
            }

            if (currentPlacementMode == PLACE_GROUND)
            {
                std::string name = "GND" + std::to_string(compCount++);
                Component *g = new Ground(name, placementState.outNode1);
                g->nodeName1 = getNodeName(placementState.outNode1);
                g->nodeName2 = "GND";
                circuit->addComponent(g);
                resetPlacementState();
                currentPlacementMode = PLACE_NONE;
                return;
            }

            placementState.outNode2 = (currentPlacementMode == PLACE_GROUND) ? 0
                                                                             : findOrCreateNodeAt(x, y);
            placementState.clicksSoFar = 2;

            if (needsFourClicks)
            {
                std::cout << "Click control +node for the dependent source." << std::endl;
                return;
            }

            // For CCVS / CCCS we still need the controlling source name from
            // valueBox.text.  If the user hasn't entered one, we abort with a
            // helpful message rather than silently hard-coding "CCVS"/"CCCS".
            if (currentPlacementMode == PLACE_CCVS || currentPlacementMode == PLACE_CCCS)
            {
                if (valueBox.text.empty())
                {
                    std::cerr << "ERROR: enter the controlling voltage source's name "
                                 "in the Value box before placing a CCVS/CCCS."
                              << std::endl;
                    resetPlacementState();
                    currentPlacementMode = PLACE_NONE;
                    return;
                }
                placementState.controllingSourceName = valueBox.text;
                // Fall through to component creation below.
            }

            // Create the component now (2-click flow finished).
            // (Continue to the create-component block.)
        }

        if (needsFourClicks && placementState.clicksSoFar == 2)
        {
            placementState.ctrlNode1 = findOrCreateNodeAt(x, y);
            placementState.clicksSoFar = 3;
            std::cout << "Click control -node for the dependent source." << std::endl;
            return;
        }
        if (needsFourClicks && placementState.clicksSoFar == 3)
        {
            placementState.ctrlNode2 = findOrCreateNodeAt(x, y);
            placementState.clicksSoFar = 4;
            // Fall through to component creation below.
        }

        // ------------------------- create the component -----------------------
        if ((placementState.clicksSoFar == 2 && !needsFourClicks) ||
            (placementState.clicksSoFar == 4 && needsFourClicks))
        {

            std::string name;
            Component *newComp = nullptr;
            double value = valueBox.text.empty() ? 0.0 : parseSpiceValue(valueBox.text);
            int node1 = placementState.outNode1;
            int node2 = placementState.outNode2;
            std::string node1Name = getNodeName(node1);
            std::string node2Name = getNodeName(node2);

            switch (currentPlacementMode)
            {
            case PLACE_RESISTOR:
                name = "R" + std::to_string(compCount++);
                if (value <= 0)
                    value = 1000.0;
                newComp = new Resistor(name, node1, node2, value);
                break;
            case PLACE_CAPACITOR:
                name = "C" + std::to_string(compCount++);
                if (value <= 0)
                    value = 1e-6;
                newComp = new Capacitor(name, node1, node2, value);
                break;
            case PLACE_INDUCTOR:
                name = "L" + std::to_string(compCount++);
                if (value <= 0)
                    value = 1e-3;
                newComp = new Inductor(name, node1, node2, value);
                break;
            case PLACE_VOLTAGE_SOURCE:
                name = "V" + std::to_string(compCount++);
                if (value <= 0)
                    value = 5.0;
                newComp = new VoltageSource(name, node1, node2, value);
                break;
            case PLACE_CURRENT_SOURCE:
                name = "I" + std::to_string(compCount++);
                if (value <= 0)
                    value = 0.1;
                newComp = new CurrentSource(name, node1, node2, value);
                break;
            case PLACE_DIODE:
                name = "D" + std::to_string(compCount++);
                newComp = new Diode(name, node1, node2);
                break;
            case PLACE_SIN_VOLTAGE_SOURCE:
            {
                name = "VSIN" + std::to_string(compCount++);
                double amp = ampBox.text.empty() ? 1.0 : parseSpiceValue(ampBox.text);
                double freq = freqBox.text.empty() ? 1000.0 : parseSpiceValue(freqBox.text);
                double phase = phaseBox.text.empty() ? 0.0 : parseSpiceValue(phaseBox.text);
                double off = offsetBox.text.empty() ? 0.0 : parseSpiceValue(offsetBox.text);
                newComp = new SinVoltageSource(name, node1, node2, amp, freq, phase, off);
                break;
            }
            case PLACE_SIN_CURRENT_SOURCE:
            {
                name = "ISIN" + std::to_string(compCount++);
                double amp = ampBox.text.empty() ? 1.0 : parseSpiceValue(ampBox.text);
                double freq = freqBox.text.empty() ? 1000.0 : parseSpiceValue(freqBox.text);
                double phase = phaseBox.text.empty() ? 0.0 : parseSpiceValue(phaseBox.text);
                double off = offsetBox.text.empty() ? 0.0 : parseSpiceValue(offsetBox.text);
                newComp = new SinCurrentSource(name, node1, node2, amp, freq, phase, off);
                break;
            }
            case PLACE_PULSE_VOLTAGE_SOURCE:
            {
                name = "VPULSE" + std::to_string(compCount++);
                double v1 = v1Box.text.empty() ? 0.0 : parseSpiceValue(v1Box.text);
                double v2 = v2Box.text.empty() ? 5.0 : parseSpiceValue(v2Box.text);
                double td = tdBox.text.empty() ? 0.0 : parseSpiceValue(tdBox.text);
                double tr = trBox.text.empty() ? 1e-6 : parseSpiceValue(trBox.text);
                double tf = tfBox.text.empty() ? 1e-6 : parseSpiceValue(tfBox.text);
                double pw = pwBox.text.empty() ? 1e-3 : parseSpiceValue(pwBox.text);
                double per = perBox.text.empty() ? 2e-3 : parseSpiceValue(perBox.text);
                newComp = new PulseVoltageSource(name, node1, node2, v1, v2, td, tr, tf, pw, per);
                break;
            }
            case PLACE_PULSE_CURRENT_SOURCE:
            {
                name = "IPULSE" + std::to_string(compCount++);
                double i1 = v1Box.text.empty() ? 0.0 : parseSpiceValue(v1Box.text);
                double i2 = v2Box.text.empty() ? 0.1 : parseSpiceValue(v2Box.text);
                double td = tdBox.text.empty() ? 0.0 : parseSpiceValue(tdBox.text);
                double tr = trBox.text.empty() ? 1e-6 : parseSpiceValue(trBox.text);
                double tf = tfBox.text.empty() ? 1e-6 : parseSpiceValue(tfBox.text);
                double pw = pwBox.text.empty() ? 1e-3 : parseSpiceValue(pwBox.text);
                double per = perBox.text.empty() ? 2e-3 : parseSpiceValue(perBox.text);
                newComp = new PulseCurrentSource(name, node1, node2, i1, i2, td, tr, tf, pw, per);
                break;
            }
            case PLACE_VCVS:
            {
                name = "E" + std::to_string(compCount++);
                double gain = valueBox.text.empty() ? 1.0 : parseSpiceValue(valueBox.text);
                newComp = new VCVS(name, node1, node2,
                                   placementState.ctrlNode1,
                                   placementState.ctrlNode2, gain);
                break;
            }
            case PLACE_VCCS:
            {
                name = "G" + std::to_string(compCount++);
                double gm = valueBox.text.empty() ? 0.1 : parseSpiceValue(valueBox.text);
                newComp = new VCCS(name, node1, node2,
                                   placementState.ctrlNode1,
                                   placementState.ctrlNode2, gm);
                break;
            }
            case PLACE_CCVS:
            {
                name = "H" + std::to_string(compCount++);
                double gain = valueBox.text.empty() ? 1.0 : parseSpiceValue(valueBox.text);
                newComp = new CCVS(name, node1, node2,
                                   placementState.controllingSourceName, gain);
                break;
            }
            case PLACE_CCCS:
            {
                name = "F" + std::to_string(compCount++);
                double gain = valueBox.text.empty() ? 1.0 : parseSpiceValue(valueBox.text);
                newComp = new CCCS(name, node1, node2,
                                   placementState.controllingSourceName, gain);
                break;
            }
            default:
                break;
            }

            if (newComp)
            {
                newComp->nodeName1 = node1Name;
                newComp->nodeName2 = node2Name;
                circuit->addComponent(newComp);
                std::cout << "Added " << newComp->getType() << " " << name
                          << " between nodes " << node1 << " and " << node2 << std::endl;
            }

            resetPlacementState();
            currentPlacementMode = PLACE_NONE;
        }
    }

} // namespace nutspice
