#ifndef NUTSPICE_COMPONENT_H
#define NUTSPICE_COMPONENT_H

#include <SDL2/SDL.h>

#include <cereal/archives/binary.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/vector.hpp>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace nutspice
{

    enum ComponentType
    {
        RESISTOR,
        CAPACITOR,
        INDUCTOR,
        VOLTAGE_SOURCE,
        CURRENT_SOURCE,
        DIODE,
        GROUND,
        SIN_VOLTAGE_SOURCE,
        PULSE_VOLTAGE_SOURCE,
        SIN_CURRENT_SOURCE,
        PULSE_CURRENT_SOURCE,
        VCVS_SOURCE,
        VCCS_SOURCE,
        CCCS_SOURCE,
        CCVS_SOURCE
    };

    enum PlacementMode
    {
        PLACE_NONE,
        PLACE_RESISTOR,
        PLACE_CAPACITOR,
        PLACE_INDUCTOR,
        PLACE_VOLTAGE_SOURCE,
        PLACE_CURRENT_SOURCE,
        PLACE_DIODE,
        PLACE_GROUND,
        PLACE_WIRE,
        PLACE_VCVS,
        PLACE_VCCS,
        PLACE_CCCS,
        PLACE_CCVS,
        PLACE_SIN_VOLTAGE_SOURCE,
        PLACE_SIN_CURRENT_SOURCE,
        PLACE_PULSE_VOLTAGE_SOURCE,
        PLACE_PULSE_CURRENT_SOURCE
    };

    // MNA stamp matrices used by every component.  See Circuit.cpp for the
    // final assembly: the system is
    //     [ G  B ] [v]   [J]
    //     [ C  D ] [i] = [E]
    // where G is numNodes×numNodes, B is numNodes×numExtra, C is numExtra×numNodes,
    // D is numExtra×numExtra.  `nextVariable` is the running index inside the
    // "extra" block (voltage sources + inductors + any internal unknowns).
    struct StampMatrices
    {
        std::vector<std::vector<double>> &G;
        std::vector<std::vector<double>> &B;
        std::vector<std::vector<double>> &C;
        std::vector<std::vector<double>> &D;
        std::vector<double> &J;
        std::vector<double> &E;
        int &nextVariable;
    };

    class Component
    {
    public:
        ComponentType type;
        std::string name;
        std::string nodeName1;
        std::string nodeName2;
        int node1 = 0;
        int node2 = 0;
        double value = 0.0;
        int posX = 0;
        int posY = 0;

        Component(ComponentType t, const std::string &n, int n1, int n2, double val)
            : type(t), name(n), node1(n1), node2(n2), value(val) {}

        virtual ~Component() = default;

        virtual void stamp(StampMatrices m) = 0;
        virtual void update(double /*dt*/, const std::vector<double> & /*nodeVoltages*/) {}
        virtual double getCurrent(const std::vector<double> & /*nodeVoltages*/) const { return 0.0; }

        virtual std::string getInfo() const;
        virtual std::string getType() = 0;

        virtual void setPosition(int x, int y)
        {
            posX = x;
            posY = y;
        }
        virtual std::pair<int, int> getPosition() const { return {posX, posY}; }

        virtual void render(SDL_Renderer *renderer,
                            const std::map<int, SDL_Point> &nodePositions) const = 0;
        virtual SDL_Rect getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const = 0;
        virtual Component *clone() const = 0;

        template <class Archive>
        void serialize(Archive &archive)
        {
            archive(type, name, nodeName1, nodeName2, node1, node2, value, posX, posY);
        }
    };

    // Helper used by Component::getInfo() and parsers.
    std::string componentTypeLabel(ComponentType t);

    // cereal needs an external serialize() for std::pair<int,int> and SDL_Point.
    namespace cereal
    {
        template <class Archive>
        void serialize(Archive &archive, std::pair<int, int> &pair)
        {
            archive(pair.first, pair.second);
        }
        template <class Archive>
        void serialize(Archive &archive, SDL_Point &point)
        {
            archive(point.x, point.y);
        }
    } // namespace cereal

} // namespace nutspice

#endif // NUTSPICE_COMPONENT_H
