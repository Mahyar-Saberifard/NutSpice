#ifndef NUTSPICE_CIRCUIT_H
#define NUTSPICE_CIRCUIT_H

#include "Component.h"
#include "Theme.h"

#include <SDL2/SDL.h>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace nutspice
{

    struct PlotSignal
    {
        std::vector<double> time;
        std::vector<double> values;
        std::string name;
        SDL_Color color;
        bool selected = false;
        bool visible = true;

        void changeColor(SDL_Color newColor) { color = newColor; }
        void toggleSelected() { selected = !selected; }
        void toggleVisibility() { visible = !visible; }
    };

    struct CircuitHeader
    {
        char magic[8] = {'N', 'U', 'T', 'S', 'P', 'I', 'C', 'E'};
        uint32_t version = 1;
        uint32_t checksum = 0;

        template <class Archive>
        void serialize(Archive &archive) { archive(magic, version, checksum); }
    };

    class Circuit
    {
    public:
        std::vector<PlotSignal> plotSignals;
        int selectedSignalIndex = -1;
        std::vector<Component *> components;
        int maxNode = 0;
        static double currentTimeStep;
        static double currentTime;
        std::vector<std::pair<int, int>> wireConnections;
        std::map<int, SDL_Point> nodePositions;
        std::unordered_map<std::string, int> nodeMap;
        std::unordered_map<int, std::string> reverseNodeMap;
        int nextNodeNumber = 1;

        enum CircuitMode
        {
            DC,
            TRANSIENT,
            SWEEP
        };
        CircuitMode currentMode = DC;

        Circuit() = default;
        Circuit(const Circuit &other);
        Circuit &operator=(const Circuit &other);
        ~Circuit();

        Circuit *clone() const { return new Circuit(*this); }

        // ---- Node management --------------------------------------------------
        int getOrCreateNode(const std::string &name);
        std::string getNodeName(int nodeNum) const;
        void updateNodeConnections();
        void updateNodePositions();
        void calculateNodePositions();

        const std::unordered_map<std::string, int> &getNodeMap() const { return nodeMap; }
        const std::unordered_map<int, std::string> &getReverseNodeMap() const { return reverseNodeMap; }

        // ---- Component management --------------------------------------------
        bool hasComponent(const std::string &name) const;
        bool hasGround() const;
        const std::vector<Component *> &getComponents() const { return components; }
        std::vector<std::string> listComponents(ComponentType filterType =
                                                    static_cast<ComponentType>(-1)) const;
        void addComponent(Component *comp);
        bool deleteComponent(const std::string &name);

        // ---- Wires ------------------------------------------------------------
        void addWire(int node1, int node2) { wireConnections.emplace_back(node1, node2); }
        const std::vector<std::pair<int, int>> &getWires() const { return wireConnections; }

        // ---- Rename -----------------------------------------------------------
        bool renameNode(const std::string &oldName, const std::string &newName);
        bool renameComponent(const std::string &oldName, const std::string &newName);

        std::vector<std::string> listNodes() const;

        // ---- Analysis ---------------------------------------------------------
        void analyzeDC();
        void analyzeTransient(double tStep, double tStop);
        void analyzeDCSweep(const std::string &sourceName, double start, double stop, double step);
        void printTransientResults(const std::vector<double> &times,
                                   const std::vector<double> &voltages,
                                   const std::vector<double> &currents,
                                   int numNodes);
        void updateAnalysisMode();

        // ---- Plot signals -----------------------------------------------------
        void addPlotSignal(const std::string &name,
                           const std::vector<double> &times,
                           const std::vector<double> &values,
                           SDL_Color color = {255, 0, 0, 255});
        void changeSignalColor(int index, SDL_Color newColor);
        void selectSignal(int index);
        void drawLTspiceStylePlot(SDL_Renderer *renderer, const SDL_Rect &area);

        // ---- Persistence ------------------------------------------------------
        void saveToFile(const std::string &filename) const;
        bool loadFromFile(const std::string &filename);

        template <class Archive>
        void save(Archive &archive) const;
        template <class Archive>
        void load(Archive &archive);

    private:
        // Shared helper: counts voltage-source-class components and builds the
        // name→index map needed by CCVS / CCCS.  Returns the number of extra
        // variables (voltage sources + inductors).
        int prepareVariableSpace(std::unordered_map<std::string, int> &vsIndexMap);
        void resolveDependentSourceIndices(const std::unordered_map<std::string, int> &vsIndexMap);

        static std::vector<double> solveSystem(std::vector<std::vector<double>> &A,
                                               std::vector<double> &b);
    };

    extern const std::string CIRCUIT_EXTENSION;
    std::string ensureExtension(const std::string &filename);

} // namespace nutspice

#endif // NUTSPICE_CIRCUIT_H
