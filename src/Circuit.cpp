#include "Circuit.h"
#include "Theme.h"
#include "UI.h"
#include "Passives.h"
#include "Sources.h"
#include "DependentSources.h"
#include "Parser.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

#include <cereal/archives/binary.hpp>

namespace nutspice
{

    double Circuit::currentTimeStep = 0.0;
    double Circuit::currentTime = 0.0;

    const std::string CIRCUIT_EXTENSION = ".cir";

    std::string ensureExtension(const std::string &filename)
    {
        if (filename.find('.') == std::string::npos)
        {
            return filename + CIRCUIT_EXTENSION;
        }
        return filename;
    }

    // =============================================================================
    //  Rule of five
    // =============================================================================
    Circuit::Circuit(const Circuit &other)
        : plotSignals(other.plotSignals),
          selectedSignalIndex(other.selectedSignalIndex),
          maxNode(other.maxNode),
          wireConnections(other.wireConnections),
          nodePositions(other.nodePositions),
          nodeMap(other.nodeMap),
          reverseNodeMap(other.reverseNodeMap),
          nextNodeNumber(other.nextNodeNumber),
          currentMode(other.currentMode)
    {
        components.reserve(other.components.size());
        for (const auto &comp : other.components)
        {
            components.push_back(comp->clone());
        }
    }

    Circuit &Circuit::operator=(const Circuit &other)
    {
        if (this != &other)
        {
            for (auto *comp : components)
                delete comp;
            components.clear();
            for (const auto &comp : other.components)
            {
                components.push_back(comp->clone());
            }
            plotSignals = other.plotSignals;
            selectedSignalIndex = other.selectedSignalIndex;
            maxNode = other.maxNode;
            wireConnections = other.wireConnections;
            nodePositions = other.nodePositions;
            nodeMap = other.nodeMap;
            reverseNodeMap = other.reverseNodeMap;
            nextNodeNumber = other.nextNodeNumber;
            currentMode = other.currentMode;
        }
        return *this;
    }

    Circuit::~Circuit()
    {
        for (auto *comp : components)
            delete comp;
    }

    // =============================================================================
    //  Node management
    // =============================================================================
    int Circuit::getOrCreateNode(const std::string &name)
    {
        if (name == "GND" || name == "0")
            return 0;
        auto it = nodeMap.find(name);
        if (it != nodeMap.end())
            return it->second;
        int nodeNum = nextNodeNumber++;
        nodeMap[name] = nodeNum;
        reverseNodeMap[nodeNum] = name;
        return nodeNum;
    }

    std::string Circuit::getNodeName(int nodeNum) const
    {
        if (nodeNum == 0)
            return "GND";
        auto it = reverseNodeMap.find(nodeNum);
        return (it != reverseNodeMap.end()) ? it->second : "UNKNOWN";
    }

    void Circuit::updateNodeConnections()
    {
        std::unordered_set<int> connectedNodes;
        connectedNodes.insert(0);
        for (const auto &comp : components)
        {
            connectedNodes.insert(comp->node1);
            connectedNodes.insert(comp->node2);
        }

        std::unordered_map<std::string, int> newNodeMap;
        std::unordered_map<int, std::string> newReverseNodeMap;
        int newNext = 1;

        for (const auto &kv : nodeMap)
        {
            if (connectedNodes.count(kv.second))
            {
                int mapped = (kv.second == 0) ? 0 : newNext++;
                newNodeMap[kv.first] = mapped;
                newReverseNodeMap[mapped] = kv.first;
            }
        }

        for (auto &comp : components)
        {
            const std::string &n1name = reverseNodeMap[comp->node1];
            const std::string &n2name = reverseNodeMap[comp->node2];
            comp->node1 = (newNodeMap.find(n1name) != newNodeMap.end()) ? newNodeMap[n1name] : 0;
            comp->node2 = (newNodeMap.find(n2name) != newNodeMap.end()) ? newNodeMap[n2name] : 0;
        }

        nodeMap.swap(newNodeMap);
        reverseNodeMap.swap(newReverseNodeMap);
        nextNodeNumber = newNext;
    }

    void Circuit::updateNodePositions()
    {
        int x = 100, y = 100;
        nodePositions.clear();
        nodePositions[0] = {100, 500};
        for (int i = 1; i < nextNodeNumber; i++)
        {
            nodePositions[i] = {x, y};
            x += 100;
            if (x > 700)
            {
                x = 100;
                y += 100;
            }
        }
    }

    void Circuit::calculateNodePositions()
    {
        nodePositions.clear();
        nodePositions[0] = {circuitArea.x + circuitArea.w / 2,
                            circuitArea.y + circuitArea.h - 50};

        int x = circuitArea.x + 100;
        int y = circuitArea.y + 100;
        const int nodesPerRow = 5;
        for (int i = 1; i < nextNodeNumber; i++)
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

    // =============================================================================
    //  Component management
    // =============================================================================
    bool Circuit::hasComponent(const std::string &name) const
    {
        for (const auto *comp : components)
        {
            if (comp->name == name)
                return true;
        }
        return false;
    }

    bool Circuit::hasGround() const
    {
        for (const auto *comp : components)
        {
            if (comp->type == GROUND)
                return true;
            if (comp->node1 == 0 || comp->node2 == 0)
                return true;
        }
        return false;
    }

    std::vector<std::string> Circuit::listComponents(ComponentType filterType) const
    {
        std::vector<std::string> result;
        for (const auto *comp : components)
        {
            if (filterType == static_cast<ComponentType>(-1) || comp->type == filterType)
            {
                result.push_back(comp->getInfo());
            }
        }
        return result;
    }

    void Circuit::addComponent(Component *comp)
    {
        if (hasComponent(comp->name))
        {
            throw std::runtime_error("Component " + comp->name + " already exists in the circuit");
        }
        comp->node1 = getOrCreateNode(comp->nodeName1);
        comp->node2 = getOrCreateNode(comp->nodeName2);

        if ((comp->type == RESISTOR || comp->type == CAPACITOR || comp->type == INDUCTOR) &&
            comp->value <= 0)
        {
            std::string typeStr = componentTypeLabel(comp->type);
            delete comp;
            throw std::runtime_error(typeStr + " value must be positive");
        }

        components.push_back(comp);
        if (comp->node1 > maxNode)
            maxNode = comp->node1;
        if (comp->node2 > maxNode)
            maxNode = comp->node2;

        if (comp->type == CAPACITOR || comp->type == INDUCTOR ||
            comp->type == SIN_VOLTAGE_SOURCE || comp->type == SIN_CURRENT_SOURCE ||
            comp->type == PULSE_VOLTAGE_SOURCE || comp->type == PULSE_CURRENT_SOURCE)
        {
            currentMode = TRANSIENT;
        }
    }

    bool Circuit::deleteComponent(const std::string &name)
    {
        for (auto it = components.begin(); it != components.end(); ++it)
        {
            if ((*it)->name == name)
            {
                delete *it;
                components.erase(it);
                updateNodeConnections();
                return true;
            }
        }
        return false;
    }

    bool Circuit::renameNode(const std::string &oldName, const std::string &newName)
    {
        if (oldName == "GND" || oldName == "0" || newName == "GND" || newName == "0")
            return false;
        if (nodeMap.find(newName) != nodeMap.end())
            return false;
        auto it = nodeMap.find(oldName);
        if (it == nodeMap.end())
            return false;
        int nodeNum = it->second;
        nodeMap.erase(it);
        nodeMap[newName] = nodeNum;
        reverseNodeMap[nodeNum] = newName;
        return true;
    }

    bool Circuit::renameComponent(const std::string &oldName, const std::string &newName)
    {
        if (oldName == newName)
            return false;
        if (hasComponent(newName))
            return false;
        for (auto *comp : components)
        {
            if (comp->name == oldName)
            {
                comp->name = newName;
                return true;
            }
        }
        return false;
    }

    std::vector<std::string> Circuit::listNodes() const
    {
        std::vector<std::string> nodes;
        nodes.push_back("GND (0)");
        for (int i = 1; i < nextNodeNumber; i++)
        {
            nodes.push_back(reverseNodeMap.at(i) + " (" + std::to_string(i) + ")");
        }
        return nodes;
    }

    // =============================================================================
    //  Solver helpers
    // =============================================================================
    int Circuit::prepareVariableSpace(std::unordered_map<std::string, int> &vsIndexMap)
    {
        int idx = 0;
        for (const auto *comp : components)
        {
            switch (comp->type)
            {
            case VOLTAGE_SOURCE:
            case SIN_VOLTAGE_SOURCE:
            case PULSE_VOLTAGE_SOURCE:
            case VCVS_SOURCE:
            case CCVS_SOURCE:
                vsIndexMap[comp->name] = idx++;
                break;
            default:
                break;
            }
        }
        return idx;
    }

    void Circuit::resolveDependentSourceIndices(const std::unordered_map<std::string, int> &vsIndexMap)
    {
        for (auto *comp : components)
        {
            if (comp->type == CCVS_SOURCE)
            {
                static_cast<CCVS *>(comp)->resolveIndices(vsIndexMap);
            }
            else if (comp->type == CCCS_SOURCE)
            {
                static_cast<CCCS *>(comp)->resolveIndices(vsIndexMap);
            }
        }
    }

    std::vector<double> Circuit::solveSystem(std::vector<std::vector<double>> &A,
                                             std::vector<double> &b)
    {
        int n = static_cast<int>(A.size());
        for (int i = 0; i < n; i++)
        {
            int maxRow = i;
            for (int k = i + 1; k < n; k++)
            {
                if (std::fabs(A[k][i]) > std::fabs(A[maxRow][i]))
                    maxRow = k;
            }
            if (maxRow != i)
            {
                std::swap(A[i], A[maxRow]);
                std::swap(b[i], b[maxRow]);
            }
            if (std::fabs(A[i][i]) < 1e-12)
            {
                throw std::runtime_error("Matrix is singular or nearly singular");
            }
            for (int k = i + 1; k < n; k++)
            {
                double factor = A[k][i] / A[i][i];
                for (int j = i; j < n; j++)
                    A[k][j] -= factor * A[i][j];
                b[k] -= factor * b[i];
            }
        }

        std::vector<double> x(n);
        for (int i = n - 1; i >= 0; i--)
        {
            x[i] = b[i];
            for (int j = i + 1; j < n; j++)
                x[i] -= A[i][j] * x[j];
            x[i] /= A[i][i];
        }
        return x;
    }

    // =============================================================================
    //  DC analysis
    // =============================================================================
    void Circuit::analyzeDC()
    {
        plotSignals.clear();
        selectedSignalIndex = -1;

        if (!hasGround())
            throw std::runtime_error("No ground node detected in the circuit");

        int numNodes = maxNode;
        int numVSources = 0;
        int numInductors = 0;

        for (const auto *comp : components)
        {
            switch (comp->type)
            {
            case VOLTAGE_SOURCE:
            case SIN_VOLTAGE_SOURCE:
            case PULSE_VOLTAGE_SOURCE:
            case VCVS_SOURCE: // <-- FIX: was missing
            case CCVS_SOURCE: // <-- FIX: was missing
                numVSources++;
                break;
            case INDUCTOR:
                numInductors++;
                break;
            default:
                break;
            }
        }

        // Build the controlling-voltage-source index map and resolve every CCVS /
        // CCCS that depends on one.  This used to be skipped entirely.
        std::unordered_map<std::string, int> vsIndexMap;
        prepareVariableSpace(vsIndexMap);
        resolveDependentSourceIndices(vsIndexMap);

        int numVars = numNodes + numVSources + numInductors;
        if (numVars == 0)
        {
            std::cout << "No variables to solve for in DC analysis" << std::endl;
            return;
        }

        int numExtra = numVSources + numInductors;
        std::vector<std::vector<double>> G(numNodes, std::vector<double>(numNodes, 0.0));
        std::vector<std::vector<double>> B(numNodes, std::vector<double>(numExtra, 0.0));
        std::vector<std::vector<double>> C(numExtra, std::vector<double>(numNodes, 0.0));
        std::vector<std::vector<double>> D(numExtra, std::vector<double>(numExtra, 0.0));
        std::vector<double> J(numNodes, 0.0);
        std::vector<double> E(numExtra, 0.0);

        for (int i = 0; i < numNodes; i++)
            G[i][i] = 1e-12;

        int vsCount = 0;
        for (auto *comp : components)
        {
            if (comp->type == INDUCTOR)
            {
                // DC steady-state: inductor = short (1e-12 ohm).
                double conductance = 1.0 / 1e-12;
                const auto *L = static_cast<const Inductor *>(comp);
                if (L->node1 != 0)
                {
                    G[L->node1 - 1][L->node1 - 1] += conductance;
                    if (L->node2 != 0)
                    {
                        G[L->node1 - 1][L->node2 - 1] -= conductance;
                        G[L->node2 - 1][L->node1 - 1] -= conductance;
                    }
                }
                if (L->node2 != 0)
                    G[L->node2 - 1][L->node2 - 1] += conductance;
            }
            else
            {
                comp->stamp({G, B, C, D, J, E, vsCount});
            }
        }

        std::vector<std::vector<double>> A(numVars, std::vector<double>(numVars, 0.0));
        std::vector<double> b(numVars, 0.0);
        for (int i = 0; i < numNodes; i++)
        {
            for (int j = 0; j < numNodes; j++)
                A[i][j] = G[i][j];
            for (int j = 0; j < numExtra; j++)
                A[i][numNodes + j] = B[i][j];
            b[i] = J[i];
        }
        for (int i = 0; i < numExtra; i++)
        {
            for (int j = 0; j < numNodes; j++)
                A[numNodes + i][j] = C[i][j];
            for (int j = 0; j < numExtra; j++)
                A[numNodes + i][numNodes + j] = D[i][j];
            b[numNodes + i] = E[i];
        }

        try
        {
            std::vector<double> x = solveSystem(A, b);

            std::cout << "\nDC Analysis Results:\n-------------------\nNode Voltages:\n  Node GND: 0.000000 V\n";

            std::vector<double> dcTime;
            for (double t = 0.0; t <= 1.0; t += 0.1)
                dcTime.push_back(t);

            for (int i = 0; i < numNodes; i++)
            {
                std::cout << "  Node " << getNodeName(i + 1) << ": "
                          << std::fixed << std::setprecision(6) << x[i] << " V\n";
                std::vector<double> dcV(dcTime.size(), x[i]);
                addPlotSignal("DC V(" + getNodeName(i + 1) + ")", dcTime, dcV,
                              colorPalette[i % colorPalette.size()]);
            }

            if (numVSources > 0)
            {
                std::cout << "\nCurrents through Voltage Sources:\n";
                for (int i = 0; i < numVSources; i++)
                {
                    std::cout << "  Source " << (i + 1) << ": "
                              << std::fixed << std::setprecision(6) << x[numNodes + i] << " A\n";
                    std::vector<double> dcI(dcTime.size(), x[numNodes + i]);
                    addPlotSignal("DC I(VSource" + std::to_string(i + 1) + ")", dcTime, dcI,
                                  colorPalette[(numNodes + i) % colorPalette.size()]);
                }
            }

            std::cout << "\nCurrents through Resistors:\n";
            int resistorCount = 0;
            for (const auto *comp : components)
            {
                if (comp->type == RESISTOR)
                {
                    double current = comp->getCurrent(x);
                    std::cout << "  " << comp->name << ": "
                              << std::fixed << std::setprecision(6) << current << " A\n";
                    std::vector<double> dcI(dcTime.size(), current);
                    addPlotSignal("DC I(" + comp->name + ")", dcTime, dcI,
                                  colorPalette[(numNodes + numVSources + resistorCount) % colorPalette.size()]);
                    resistorCount++;
                }
            }
            currentMode = DC;
        }
        catch (const std::runtime_error &e)
        {
            std::cerr << "Error in DC analysis: " << e.what() << std::endl;
        }

        std::cout << "DC analysis completed. Created " << plotSignals.size() << " plot signals." << std::endl;
    }

    // =============================================================================
    //  Transient analysis
    // =============================================================================
    void Circuit::analyzeTransient(double tStep, double tStop)
    {
        currentTimeStep = tStep;
        double t = 0.0;

        plotSignals.clear();
        selectedSignalIndex = -1;

        if (!hasGround())
            throw std::runtime_error("No ground node detected in the circuit");
        if (maxNode < 1)
            throw std::runtime_error("Circuit must have at least one non-ground node");

        int numNodes = maxNode;
        int numVSources = 0;
        int numInductors = 0;
        for (const auto *comp : components)
        {
            switch (comp->type)
            {
            case VOLTAGE_SOURCE:
            case SIN_VOLTAGE_SOURCE:
            case PULSE_VOLTAGE_SOURCE:
            case VCVS_SOURCE:
            case CCVS_SOURCE:
                numVSources++;
                break;
            case INDUCTOR:
                numInductors++;
                break;
            default:
                break;
            }
        }

        // Resolve CCVS/CCCS indices once up-front (the controlling source's index
        // is stable across time steps because every step restamps in the same
        // order).  Without this the original code never called resolveIndices.
        std::unordered_map<std::string, int> vsIndexMap;
        prepareVariableSpace(vsIndexMap);
        resolveDependentSourceIndices(vsIndexMap);

        int numExtra = numVSources + numInductors;
        int numVars = numNodes + numExtra;

        std::vector<std::vector<double>> nodeVoltagesOverTime(numNodes);
        std::vector<double> timePoints;
        std::vector<std::vector<double>> componentCurrentsOverTime;
        std::vector<std::string> currentSignalNames;

        for (const auto *comp : components)
        {
            if (comp->type == RESISTOR || comp->type == CAPACITOR ||
                comp->type == INDUCTOR || comp->type == DIODE)
            {
                componentCurrentsOverTime.emplace_back();
                currentSignalNames.push_back(comp->name + " Current");
            }
        }

        while (t <= tStop)
        {
            std::vector<std::vector<double>> G(numNodes, std::vector<double>(numNodes, 0.0));
            std::vector<std::vector<double>> B(numNodes, std::vector<double>(numExtra, 0.0));
            std::vector<std::vector<double>> C(numExtra, std::vector<double>(numNodes, 0.0));
            std::vector<std::vector<double>> D(numExtra, std::vector<double>(numExtra, 0.0));
            std::vector<double> J(numNodes, 0.0);
            std::vector<double> E(numExtra, 0.0);

            for (int i = 0; i < numNodes; i++)
                G[i][i] = 1e-12;

            int vsCount = 0;
            currentTime = t;
            for (auto *comp : components)
            {
                comp->stamp({G, B, C, D, J, E, vsCount});
            }

            std::vector<double> x;
            try
            {
                std::vector<std::vector<double>> A(numVars, std::vector<double>(numVars, 0.0));
                std::vector<double> b(numVars, 0.0);
                for (int i = 0; i < numNodes; i++)
                {
                    for (int j = 0; j < numNodes; j++)
                        A[i][j] = G[i][j];
                    for (int j = 0; j < numExtra; j++)
                        A[i][numNodes + j] = B[i][j];
                    b[i] = J[i];
                }
                for (int i = 0; i < numExtra; i++)
                {
                    for (int j = 0; j < numNodes; j++)
                        A[numNodes + i][j] = C[i][j];
                    for (int j = 0; j < numExtra; j++)
                        A[numNodes + i][numNodes + j] = D[i][j];
                    b[numNodes + i] = E[i];
                }

                x = solveSystem(A, b);

                timePoints.push_back(t);
                for (int i = 0; i < numNodes; i++)
                    nodeVoltagesOverTime[i].push_back(x[i]);

                size_t idx = 0;
                for (const auto *comp : components)
                {
                    if (comp->type == RESISTOR || comp->type == CAPACITOR ||
                        comp->type == INDUCTOR || comp->type == DIODE)
                    {
                        componentCurrentsOverTime[idx].push_back(comp->getCurrent(x));
                        idx++;
                    }
                }
            }
            catch (const std::runtime_error &e)
            {
                std::cerr << "Error at t=" << t << ": " << e.what() << std::endl;
                break;
            }

            for (auto *comp : components)
                comp->update(tStep, x);
            t += tStep;
        }

        for (int i = 0; i < numNodes; i++)
        {
            addPlotSignal("V(" + getNodeName(i + 1) + ")", timePoints, nodeVoltagesOverTime[i],
                          colorPalette[i % colorPalette.size()]);
        }
        for (size_t i = 0; i < componentCurrentsOverTime.size(); i++)
        {
            addPlotSignal("I(" + currentSignalNames[i] + ")", timePoints, componentCurrentsOverTime[i],
                          colorPalette[(numNodes + i) % colorPalette.size()]);
        }

        std::cout << "Transient analysis completed. Created " << plotSignals.size() << " plot signals." << std::endl;
    }

    // =============================================================================
    //  DC sweep
    // =============================================================================
    void Circuit::analyzeDCSweep(const std::string &sourceName, double start, double stop, double step)
    {
        if (!hasGround())
            throw std::runtime_error("No ground node in the circuit");

        Component *targetSource = nullptr;
        for (auto *comp : components)
        {
            if ((comp->type == VOLTAGE_SOURCE || comp->type == CURRENT_SOURCE) &&
                comp->name == sourceName)
            {
                targetSource = comp;
                break;
            }
        }
        if (!targetSource)
            throw std::runtime_error("Source " + sourceName + " not found or not sweepable");

        std::unordered_map<std::string, int> vsIndexMap;
        prepareVariableSpace(vsIndexMap);
        resolveDependentSourceIndices(vsIndexMap);

        std::vector<double> sweepPoints;
        std::vector<std::vector<double>> results;

        for (double value = start;
             (step > 0 && value <= stop) || (step < 0 && value >= stop);
             value += step)
        {

            targetSource->value = value;
            sweepPoints.push_back(value);

            int numNodes = maxNode;
            int numVSources = 0;
            int numInductors = 0;
            for (const auto *comp : components)
            {
                switch (comp->type)
                {
                case VOLTAGE_SOURCE:
                case SIN_VOLTAGE_SOURCE:
                case PULSE_VOLTAGE_SOURCE:
                case VCVS_SOURCE: // <-- FIX
                case CCVS_SOURCE: // <-- FIX
                    numVSources++;
                    break;
                case INDUCTOR:
                    numInductors++;
                    break;
                default:
                    break;
                }
            }
            int numExtra = numVSources + numInductors;
            int numVars = numNodes + numExtra;
            if (numVars == 0)
                throw std::runtime_error("No variables to solve in sweep analysis");

            std::vector<std::vector<double>> G(numNodes, std::vector<double>(numNodes, 0.0));
            std::vector<std::vector<double>> B(numNodes, std::vector<double>(numExtra, 0.0));
            std::vector<std::vector<double>> C(numExtra, std::vector<double>(numNodes, 0.0));
            std::vector<std::vector<double>> D(numExtra, std::vector<double>(numExtra, 0.0));
            std::vector<double> J(numNodes, 0.0);
            std::vector<double> E(numExtra, 0.0);
            for (int i = 0; i < numNodes; i++)
                G[i][i] = 1e-12;

            int vsCount = 0;
            for (auto *comp : components)
            {
                if (comp->type == INDUCTOR)
                {
                    double conductance = 1.0 / 1e-12;
                    const auto *L = static_cast<const Inductor *>(comp);
                    if (L->node1 != 0)
                    {
                        G[L->node1 - 1][L->node1 - 1] += conductance;
                        if (L->node2 != 0)
                        {
                            G[L->node1 - 1][L->node2 - 1] -= conductance;
                            G[L->node2 - 1][L->node1 - 1] -= conductance;
                        }
                    }
                    if (L->node2 != 0)
                        G[L->node2 - 1][L->node2 - 1] += conductance;
                }
                else
                {
                    comp->stamp({G, B, C, D, J, E, vsCount});
                }
            }

            std::vector<std::vector<double>> A(numVars, std::vector<double>(numVars, 0.0));
            std::vector<double> b(numVars, 0.0);
            for (int i = 0; i < numNodes; i++)
            {
                for (int j = 0; j < numNodes; j++)
                    A[i][j] = G[i][j];
                for (int j = 0; j < numExtra; j++)
                    A[i][numNodes + j] = B[i][j];
                b[i] = J[i];
            }
            for (int i = 0; i < numExtra; i++)
            {
                for (int j = 0; j < numNodes; j++)
                    A[numNodes + i][j] = C[i][j];
                for (int j = 0; j < numExtra; j++)
                    A[numNodes + i][numNodes + j] = D[i][j];
                b[numNodes + i] = E[i];
            }

            try
            {
                results.push_back(solveSystem(A, b));
            }
            catch (const std::runtime_error &e)
            {
                std::cerr << "Sweep value " << value << ": " << e.what() << std::endl;
                results.push_back(std::vector<double>(numVars, NAN));
            }
        }

        std::cout << "\nDC Sweep Analysis Results for " << sourceName << ":\n";
        std::cout << "-------------------------------------\n";
        std::cout << std::setw(15) << "Source (V/A)";
        for (int i = 0; i < maxNode; ++i)
            std::cout << std::setw(15) << ("V(" + getNodeName(i + 1) + ")");
        std::cout << "\n";

        for (size_t i = 0; i < sweepPoints.size(); ++i)
        {
            std::cout << std::setw(15) << std::fixed << std::setprecision(6) << sweepPoints[i];
            for (int j = 0; j < maxNode; ++j)
            {
                double v = results[i][j];
                if (std::isnan(v))
                    std::cout << std::setw(15) << "NaN";
                else
                    std::cout << std::setw(15) << std::fixed << std::setprecision(6) << v;
            }
            std::cout << "\n";
        }
    }

    // =============================================================================
    //  Misc
    // =============================================================================
    void Circuit::updateAnalysisMode()
    {
        currentMode = DC;
        for (const auto *comp : components)
        {
            if (comp->type == CAPACITOR || comp->type == INDUCTOR ||
                comp->type == SIN_VOLTAGE_SOURCE || comp->type == SIN_CURRENT_SOURCE ||
                comp->type == PULSE_VOLTAGE_SOURCE || comp->type == PULSE_CURRENT_SOURCE)
            {
                currentMode = TRANSIENT;
                break;
            }
        }
        std::cout << "Analysis mode set to: " << (currentMode == DC ? "DC" : "Transient") << std::endl;
    }

    void Circuit::printTransientResults(const std::vector<double> &times,
                                        const std::vector<double> &voltages,
                                        const std::vector<double> &currents,
                                        int numNodes)
    {
        std::cout << "\nTransient Analysis Results:\n--------------------------\n";
        std::cout << "Time (s)\tNode GND (V)\t";
        for (int i = 0; i < numNodes; i++)
            std::cout << "Node " << getNodeName(i + 1) << " (V)\t";
        std::cout << "Component Currents (A)\n";

        size_t numTimeSteps = (numNodes > 0) ? times.size() / numNodes : 0;
        size_t currentIndex = 0;

        for (size_t step = 0; step < numTimeSteps; step++)
        {
            std::cout << std::fixed << std::setprecision(6) << times[step * numNodes] << "\t0.000000\t";
            for (int node = 0; node < numNodes; node++)
                std::cout << voltages[step * numNodes + node] << "\t";
            for (const auto *comp : components)
            {
                if (comp->type == RESISTOR || comp->type == CAPACITOR ||
                    comp->type == INDUCTOR || comp->type == DIODE)
                {
                    if (currentIndex < currents.size())
                    {
                        std::cout << comp->name << ": " << currents[currentIndex] << "\t";
                        currentIndex++;
                    }
                }
            }
            std::cout << "\n";
        }
    }

    void Circuit::addPlotSignal(const std::string &name,
                                const std::vector<double> &times,
                                const std::vector<double> &values,
                                SDL_Color color)
    {
        PlotSignal s;
        s.name = name;
        s.time = times;
        s.values = values;
        s.color = color;
        plotSignals.push_back(std::move(s));
    }

    void Circuit::changeSignalColor(int index, SDL_Color newColor)
    {
        if (index >= 0 && index < static_cast<int>(plotSignals.size()))
            plotSignals[index].changeColor(newColor);
    }

    void Circuit::selectSignal(int index)
    {
        if (selectedSignalIndex >= 0 && selectedSignalIndex < static_cast<int>(plotSignals.size()))
            plotSignals[selectedSignalIndex].selected = false;
        if (index >= 0 && index < static_cast<int>(plotSignals.size()))
        {
            plotSignals[index].selected = true;
            selectedSignalIndex = index;
        }
        else
        {
            selectedSignalIndex = -1;
        }
    }

    // =============================================================================
    //  Plot rendering (kept close to the original; only cosmetic tweaks)
    // =============================================================================
    void Circuit::drawLTspiceStylePlot(SDL_Renderer *renderer, const SDL_Rect &area)
    {
        if (plotSignals.empty())
            return;

        SDL_SetRenderDrawColor(renderer, currentTheme.plotBg.r, currentTheme.plotBg.g,
                               currentTheme.plotBg.b, 255);
        SDL_RenderFillRect(renderer, &area);
        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g,
                               currentTheme.text.b, 255);
        SDL_RenderDrawRect(renderer, &area);

        double minTime = std::numeric_limits<double>::max();
        double maxTime = std::numeric_limits<double>::lowest();
        double minValue = std::numeric_limits<double>::max();
        double maxValue = std::numeric_limits<double>::lowest();

        for (const auto &sig : plotSignals)
        {
            if (!sig.visible || sig.time.empty())
                continue;
            minTime = std::min(minTime, sig.time.front());
            maxTime = std::max(maxTime, sig.time.back());
            auto vminmax = std::minmax_element(sig.values.begin(), sig.values.end());
            if (vminmax.first != sig.values.end())
                minValue = std::min(minValue, *vminmax.first);
            if (vminmax.second != sig.values.end())
                maxValue = std::max(maxValue, *vminmax.second);
        }

        if (minTime == std::numeric_limits<double>::max())
            minTime = 0;
        if (maxTime == std::numeric_limits<double>::lowest())
            maxTime = 1;
        if (minValue == std::numeric_limits<double>::max())
            minValue = 0;
        if (maxValue == std::numeric_limits<double>::lowest())
            maxValue = 1;

        double valueRange = maxValue - minValue;
        if (valueRange < 1e-12)
            valueRange = 1;
        minValue -= valueRange * 0.1;
        maxValue += valueRange * 0.1;
        double timeRange = maxTime - minTime;
        if (timeRange <= 0)
            timeRange = 1;

        // Grid lines (use the dedicated gridLine theme colour).
        if (true)
        {
            SDL_SetRenderDrawColor(renderer, currentTheme.gridLine.r, currentTheme.gridLine.g,
                                   currentTheme.gridLine.b, 80);
            double tStep = std::pow(10, std::floor(std::log10(timeRange)));
            if (tStep <= 0)
                tStep = 1;
            for (double t = std::ceil(minTime / tStep) * tStep; t <= maxTime; t += tStep)
            {
                int x = area.x + static_cast<int>((t - minTime) / timeRange * area.w);
                SDL_RenderDrawLine(renderer, x, area.y, x, area.y + area.h);
            }
            double vStep = std::pow(10, std::floor(std::log10(valueRange)));
            if (vStep <= 0)
                vStep = 1;
            for (double v = std::ceil(minValue / vStep) * vStep; v <= maxValue; v += vStep)
            {
                int y = area.y + area.h - static_cast<int>((v - minValue) / valueRange * area.h);
                SDL_RenderDrawLine(renderer, area.x, y, area.x + area.w, y);
            }
        }

        // Axes labels (time + a few value ticks).
        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g,
                               currentTheme.text.b, 255);
        renderText("Time (s)", area.x + area.w / 2 - 30, area.y + area.h + 5);
        renderText(std::to_string(minTime).substr(0, 6), area.x, area.y + area.h + 5);
        renderText(std::to_string(maxTime).substr(0, 6), area.x + area.w - 50, area.y + area.h + 5);

        // Draw each visible signal.
        for (const auto &sig : plotSignals)
        {
            if (!sig.visible || sig.time.size() < 2)
                continue;
            SDL_SetRenderDrawColor(renderer, sig.color.r, sig.color.g, sig.color.b, 255);
            for (size_t i = 1; i < sig.time.size(); i++)
            {
                int x1 = area.x + static_cast<int>((sig.time[i - 1] - minTime) / timeRange * area.w);
                int y1 = area.y + area.h - static_cast<int>((sig.values[i - 1] - minValue) / valueRange * area.h);
                int x2 = area.x + static_cast<int>((sig.time[i] - minTime) / timeRange * area.w);
                int y2 = area.y + area.h - static_cast<int>((sig.values[i] - minValue) / valueRange * area.h);
                SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
            }
        }

        // Legend (small, top-left of plot area).
        int legendY = area.y + 10;
        for (size_t i = 0; i < plotSignals.size(); i++)
        {
            const auto &sig = plotSignals[i];
            SDL_Rect colorSwatch = {area.x + 10, legendY + 4, 12, 12};
            SDL_SetRenderDrawColor(renderer, sig.color.r, sig.color.g, sig.color.b, 255);
            SDL_RenderFillRect(renderer, &colorSwatch);
            renderText(sig.name, area.x + 28, legendY);
            legendY += 20;
            if (legendY > area.y + area.h - 20)
                break;
        }
    }

    // =============================================================================
    //  Serialization
    // =============================================================================
    template <class Archive>
    void Circuit::save(Archive &archive) const
    {
        CircuitHeader header;
        archive(header);
        archive(static_cast<int>(currentMode));

        size_t numComponents = components.size();
        archive(numComponents);

        for (const auto *comp : components)
        {
            std::string typeName = comp->getType();
            archive(typeName);

            if (auto r = dynamic_cast<const Resistor *>(comp))
                archive(*r);
            else if (auto c = dynamic_cast<const Capacitor *>(comp))
                archive(*c);
            else if (auto l = dynamic_cast<const Inductor *>(comp))
                archive(*l);
            else if (auto v = dynamic_cast<const VoltageSource *>(comp))
                archive(*v);
            else if (auto i = dynamic_cast<const CurrentSource *>(comp))
                archive(*i);
            else if (auto d = dynamic_cast<const Diode *>(comp))
                archive(*d);
            else if (auto g = dynamic_cast<const Ground *>(comp))
                archive(*g);
            else if (auto sv = dynamic_cast<const SinVoltageSource *>(comp))
                archive(*sv);
            else if (auto si = dynamic_cast<const SinCurrentSource *>(comp))
                archive(*si);
            else if (auto pv = dynamic_cast<const PulseVoltageSource *>(comp))
                archive(*pv);
            else if (auto pi = dynamic_cast<const PulseCurrentSource *>(comp))
                archive(*pi);
            else if (auto e = dynamic_cast<const VCVS *>(comp))
                archive(*e);
            else if (auto g2 = dynamic_cast<const VCCS *>(comp))
                archive(*g2);
            else if (auto h = dynamic_cast<const CCVS *>(comp))
                archive(*h);
            else if (auto f = dynamic_cast<const CCCS *>(comp))
                archive(*f);
            else if (auto w = dynamic_cast<const Wire *>(comp))
                archive(*w);
        }

        archive(wireConnections);
        archive(maxNode, nodePositions, nodeMap, reverseNodeMap, nextNodeNumber);
    }

    template <class Archive>
    void Circuit::load(Archive &archive)
    {
        CircuitHeader header;
        archive(header);
        if (std::strncmp(header.magic, "NUTSPICE", 8) != 0)
        {
            throw std::runtime_error("Invalid circuit file format");
        }

        for (auto *comp : components)
            delete comp;
        components.clear();

        int modeInt;
        archive(modeInt);
        currentMode = static_cast<CircuitMode>(modeInt);

        size_t numComponents;
        archive(numComponents);
        for (size_t i = 0; i < numComponents; i++)
        {
            std::string typeName;
            archive(typeName);
            Component *comp = nullptr;

            if (typeName == "Resistor")
            {
                auto *p = new Resistor("", 0, 0, 0);
                archive(*p);
                comp = p;
            }
            else if (typeName == "Capacitor")
            {
                auto *p = new Capacitor("", 0, 0, 0);
                archive(*p);
                comp = p;
            }
            else if (typeName == "Inductor")
            {
                auto *p = new Inductor("", 0, 0, 0);
                archive(*p);
                comp = p;
            }
            else if (typeName == "VoltageSource")
            {
                auto *p = new VoltageSource("", 0, 0, 0);
                archive(*p);
                comp = p;
            }
            else if (typeName == "CurrentSource")
            {
                auto *p = new CurrentSource("", 0, 0, 0);
                archive(*p);
                comp = p;
            }
            else if (typeName == "Diode")
            {
                auto *p = new Diode("", 0, 0);
                archive(*p);
                comp = p;
            }
            else if (typeName == "Ground")
            {
                auto *p = new Ground("", 0);
                archive(*p);
                comp = p;
            }
            else if (typeName == "SinVoltageSource")
            {
                auto *p = new SinVoltageSource("", 0, 0, 0, 0, 0, 0);
                archive(*p);
                comp = p;
            }
            else if (typeName == "SinCurrentSource")
            {
                auto *p = new SinCurrentSource("", 0, 0, 0, 0, 0, 0);
                archive(*p);
                comp = p;
            }
            else if (typeName == "PulseVoltageSource")
            {
                auto *p = new PulseVoltageSource("", 0, 0, 0, 0, 0, 0, 0, 0, 0);
                archive(*p);
                comp = p;
            }
            else if (typeName == "PulseCurrentSource")
            {
                auto *p = new PulseCurrentSource("", 0, 0, 0, 0, 0, 0, 0, 0, 0);
                archive(*p);
                comp = p;
            }
            else if (typeName == "VCVS")
            {
                auto *p = new VCVS("", 0, 0, 0, 0, 0);
                archive(*p);
                comp = p;
            }
            else if (typeName == "VCCS")
            {
                auto *p = new VCCS("", 0, 0, 0, 0, 0);
                archive(*p);
                comp = p;
            }
            else if (typeName == "CCVS")
            {
                auto *p = new CCVS("", 0, 0, "", 0);
                archive(*p);
                comp = p;
            }
            else if (typeName == "CCCS")
            {
                auto *p = new CCCS("", 0, 0, "", 0);
                archive(*p);
                comp = p;
            }
            else if (typeName == "Wire")
            {
                auto *p = new Wire("", 0, 0);
                archive(*p);
                comp = p;
            }

            if (comp)
                components.push_back(comp);
        }

        archive(wireConnections);
        archive(maxNode, nodePositions, nodeMap, reverseNodeMap, nextNodeNumber);
        updateAnalysisMode();
    }

    template void Circuit::save<::cereal::BinaryOutputArchive>(::cereal::BinaryOutputArchive &) const;
    template void Circuit::load<::cereal::BinaryInputArchive>(::cereal::BinaryInputArchive &);

    void Circuit::saveToFile(const std::string &filename) const
    {
        try
        {
            std::string actualFilename = ensureExtension(filename);
            std::ofstream ofs(actualFilename, std::ios::binary);
            ::cereal::BinaryOutputArchive archive(ofs);
            save(archive);
            std::cout << "Circuit saved successfully to: " << actualFilename << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error saving circuit: " << e.what() << std::endl;
        }
    }

    bool Circuit::loadFromFile(const std::string &filename)
    {
        try
        {
            std::string actualFilename = ensureExtension(filename);
            std::ifstream ifs(actualFilename, std::ios::binary);
            if (!ifs.is_open())
            {
                std::cerr << "Could not open file: " << actualFilename << std::endl;
                return false;
            }
            try
            {
                ::cereal::BinaryInputArchive archive(ifs);
                load(archive);
                std::cout << "Circuit loaded from binary: " << actualFilename << std::endl;
                return true;
            }
            catch (const std::exception &e)
            {
                std::cerr << "Binary load failed: " << e.what()
                          << "\nTrying text format..." << std::endl;
                ifs.close();
                ifs.open(actualFilename);
                if (!ifs.is_open())
                    return false;
                // Text format fallback uses the SPICE parser.
                extern void processCircuitFile(const std::string &filename, Circuit &circuit);
                processCircuitFile(actualFilename, *this);
                calculateNodePositions();
                updateAnalysisMode();
                return true;
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error loading circuit: " << e.what() << std::endl;
            return false;
        }
    }

} // namespace nutspice
