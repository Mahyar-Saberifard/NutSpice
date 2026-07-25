#ifndef NUTSPICE_PASSIVES_H
#define NUTSPICE_PASSIVES_H

#include "Component.h"
#include <string>

namespace nutspice
{

    class Resistor : public Component
    {
    public:
        Resistor(const std::string &n, int n1, int n2, double val)
            : Component(RESISTOR, n, n1, n2, val) {}

        void stamp(StampMatrices m) override;
        double getCurrent(const std::vector<double> &nodeVoltages) const override;
        std::string getType() override { return "Resistor"; }
        void render(SDL_Renderer *renderer,
                    const std::map<int, SDL_Point> &nodePositions) const override;
        SDL_Rect getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const override;
        Component *clone() const override { return new Resistor(*this); }
    };

    class Capacitor : public Component
    {
    public:
        Capacitor(const std::string &n, int n1, int n2, double val)
            : Component(CAPACITOR, n, n1, n2, val) {}

        void stamp(StampMatrices m) override;
        void update(double dt, const std::vector<double> &nodeVoltages) override;
        double getCurrent(const std::vector<double> &nodeVoltages) const override;
        std::string getType() override { return "Capacitor"; }
        void render(SDL_Renderer *renderer,
                    const std::map<int, SDL_Point> &nodePositions) const override;
        SDL_Rect getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const override;
        Component *clone() const override { return new Capacitor(*this); }

        double previousVoltage = 0.0;
        double current = 0.0;

        template <class Archive>
        void serialize(Archive &archive)
        {
            archive(::cereal::base_class<Component>(this), previousVoltage, current);
        }
    };

    class Inductor : public Component
    {
    public:
        Inductor(const std::string &n, int n1, int n2, double val)
            : Component(INDUCTOR, n, n1, n2, val) {}

        void stamp(StampMatrices m) override;
        void update(double dt, const std::vector<double> &nodeVoltages) override;
        double getCurrent(const std::vector<double> &nodeVoltages) const override;
        std::string getType() override { return "Inductor"; }
        void render(SDL_Renderer *renderer,
                    const std::map<int, SDL_Point> &nodePositions) const override;
        SDL_Rect getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const override;
        Component *clone() const override { return new Inductor(*this); }

        double current = 0.0;
        double previousCurrent = 0.0;

        template <class Archive>
        void serialize(Archive &archive)
        {
            archive(::cereal::base_class<Component>(this), current, previousCurrent);
        }
    };

    class Diode : public Component
    {
    public:
        Diode(const std::string &n, int n1, int n2)
            : Component(DIODE, n, n1, n2, 0.7) {} // default forward voltage drop

        void stamp(StampMatrices m) override;
        void update(double dt, const std::vector<double> &nodeVoltages) override;
        double getCurrent(const std::vector<double> &nodeVoltages) const override;
        std::string getType() override { return "Diode"; }
        void render(SDL_Renderer *renderer,
                    const std::map<int, SDL_Point> &nodePositions) const override;
        SDL_Rect getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const override;
        Component *clone() const override { return new Diode(*this); }

        template <class Archive>
        void serialize(Archive &archive)
        {
            archive(::cereal::base_class<Component>(this));
        }
    };

    class Ground : public Component
    {
    public:
        Ground(const std::string &n, int n1)
            : Component(GROUND, n, n1, 0, 0.0) {}

        void stamp(StampMatrices /*m*/) override {} // ground is implicit in MNA
        std::string getType() override { return "Ground"; }
        void render(SDL_Renderer *renderer,
                    const std::map<int, SDL_Point> &nodePositions) const override;
        SDL_Rect getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const override;
        Component *clone() const override { return new Ground(*this); }

        template <class Archive>
        void serialize(Archive &archive)
        {
            archive(::cereal::base_class<Component>(this));
        }
    };

    class Wire : public Component
    {
    public:
        Wire(const std::string &n, int n1, int n2)
            : Component(RESISTOR, n, n1, n2, 1e-6) {}

        void stamp(StampMatrices m) override;
        std::string getType() override { return "Wire"; }
        void render(SDL_Renderer *renderer,
                    const std::map<int, SDL_Point> &nodePositions) const override;
        SDL_Rect getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const override;
        Component *clone() const override { return new Wire(*this); }

        template <class Archive>
        void serialize(Archive &archive)
        {
            archive(::cereal::base_class<Component>(this));
        }
    };

} // namespace nutspice

#endif // NUTSPICE_PASSIVES_H
