#ifndef NUTSPICE_SOURCES_H
#define NUTSPICE_SOURCES_H

#include "Component.h"
#include <string>

namespace nutspice
{

    class VoltageSource : public Component
    {
    public:
        VoltageSource(const std::string &n, int n1, int n2, double val)
            : Component(VOLTAGE_SOURCE, n, n1, n2, val) {}

        void stamp(StampMatrices m) override;
        std::string getType() override { return "VoltageSource"; }
        void render(SDL_Renderer *renderer,
                    const std::map<int, SDL_Point> &nodePositions) const override;
        SDL_Rect getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const override;
        Component *clone() const override { return new VoltageSource(*this); }

        template <class Archive>
        void serialize(Archive &archive)
        {
            archive(::cereal::base_class<Component>(this));
        }
    };

    class SinVoltageSource : public Component
    {
    public:
        double amplitude;
        double frequency;
        double phase;
        double offset;

        SinVoltageSource(const std::string &n, int n1, int n2,
                         double amp, double freq, double ph = 0.0, double off = 0.0)
            : Component(SIN_VOLTAGE_SOURCE, n, n1, n2, 0.0),
              amplitude(amp), frequency(freq), phase(ph), offset(off) {}

        void stamp(StampMatrices m) override;
        std::string getInfo() const override;
        std::string getType() override { return "SinVoltageSource"; }
        void render(SDL_Renderer *renderer,
                    const std::map<int, SDL_Point> &nodePositions) const override;
        SDL_Rect getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const override;
        Component *clone() const override { return new SinVoltageSource(*this); }

        template <class Archive>
        void serialize(Archive &archive)
        {
            archive(::cereal::base_class<Component>(this),
                    amplitude, frequency, phase, offset);
        }
    };

    class PulseVoltageSource : public Component
    {
    public:
        double v1, v2, td, tr, tf, pw, per;

        PulseVoltageSource(const std::string &n, int n1, int n2,
                           double v1, double v2, double td, double tr,
                           double tf, double pw, double per)
            : Component(PULSE_VOLTAGE_SOURCE, n, n1, n2, 0.0),
              v1(v1), v2(v2), td(td), tr(tr), tf(tf), pw(pw), per(per) {}

        void stamp(StampMatrices m) override;
        std::string getInfo() const override;
        std::string getType() override { return "PulseVoltageSource"; }
        void render(SDL_Renderer *renderer,
                    const std::map<int, SDL_Point> &nodePositions) const override;
        SDL_Rect getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const override;
        Component *clone() const override { return new PulseVoltageSource(*this); }

        template <class Archive>
        void serialize(Archive &archive)
        {
            archive(::cereal::base_class<Component>(this),
                    v1, v2, td, tr, tf, pw, per);
        }
    };

    class CurrentSource : public Component
    {
    public:
        CurrentSource(const std::string &n, int n1, int n2, double val)
            : Component(CURRENT_SOURCE, n, n1, n2, val) {}

        void stamp(StampMatrices m) override;
        std::string getType() override { return "CurrentSource"; }
        void render(SDL_Renderer *renderer,
                    const std::map<int, SDL_Point> &nodePositions) const override;
        SDL_Rect getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const override;
        Component *clone() const override { return new CurrentSource(*this); }

        template <class Archive>
        void serialize(Archive &archive)
        {
            archive(::cereal::base_class<Component>(this));
        }
    };

    class SinCurrentSource : public Component
    {
    public:
        double amplitude;
        double frequency;
        double phase;
        double offset;

        SinCurrentSource(const std::string &n, int n1, int n2,
                         double amp, double freq, double ph = 0.0, double off = 0.0)
            : Component(SIN_CURRENT_SOURCE, n, n1, n2, 0.0),
              amplitude(amp), frequency(freq), phase(ph), offset(off) {}

        void stamp(StampMatrices m) override;
        std::string getInfo() const override;
        std::string getType() override { return "SinCurrentSource"; }
        void render(SDL_Renderer *renderer,
                    const std::map<int, SDL_Point> &nodePositions) const override;
        SDL_Rect getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const override;
        Component *clone() const override { return new SinCurrentSource(*this); }

        template <class Archive>
        void serialize(Archive &archive)
        {
            archive(::cereal::base_class<Component>(this),
                    amplitude, frequency, phase, offset);
        }
    };

    class PulseCurrentSource : public Component
    {
    public:
        double i1, i2, td, tr, tf, pw, per;

        PulseCurrentSource(const std::string &n, int n1, int n2,
                           double i1, double i2, double td, double tr,
                           double tf, double pw, double per)
            : Component(PULSE_CURRENT_SOURCE, n, n1, n2, 0.0),
              i1(i1), i2(i2), td(td), tr(tr), tf(tf), pw(pw), per(per) {}

        void stamp(StampMatrices m) override;
        std::string getInfo() const override;
        std::string getType() override { return "PulseCurrentSource"; }
        void render(SDL_Renderer *renderer,
                    const std::map<int, SDL_Point> &nodePositions) const override;
        SDL_Rect getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const override;
        Component *clone() const override { return new PulseCurrentSource(*this); }

        template <class Archive>
        void serialize(Archive &archive)
        {
            archive(::cereal::base_class<Component>(this),
                    i1, i2, td, tr, tf, pw, per);
        }
    };

} // namespace nutspice

#endif // NUTSPICE_SOURCES_H
