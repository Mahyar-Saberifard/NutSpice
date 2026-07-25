#ifndef NUTSPICE_DEPENDENT_SOURCES_H
#define NUTSPICE_DEPENDENT_SOURCES_H

#include "Component.h"
#include <string>
#include <unordered_map>

namespace nutspice
{

    class VCVS : public Component
    {
    public:
        int ctrlNode1;
        int ctrlNode2;

        VCVS(const std::string &n, int n1, int n2, int cn1, int cn2, double gain)
            : Component(VCVS_SOURCE, n, n1, n2, gain), ctrlNode1(cn1), ctrlNode2(cn2) {}

        void stamp(StampMatrices m) override;
        std::string getType() override { return "VCVS"; }
        void render(SDL_Renderer *renderer,
                    const std::map<int, SDL_Point> &nodePositions) const override;
        SDL_Rect getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const override;
        Component *clone() const override { return new VCVS(*this); }

        template <class Archive>
        void serialize(Archive &archive)
        {
            archive(::cereal::base_class<Component>(this), ctrlNode1, ctrlNode2);
        }
    };

    class VCCS : public Component
    {
    public:
        int ctrlNode1;
        int ctrlNode2;

        VCCS(const std::string &n, int n1, int n2, int cn1, int cn2, double gm)
            : Component(VCCS_SOURCE, n, n1, n2, gm), ctrlNode1(cn1), ctrlNode2(cn2) {}

        void stamp(StampMatrices m) override;
        std::string getType() override { return "VCCS"; }
        void render(SDL_Renderer *renderer,
                    const std::map<int, SDL_Point> &nodePositions) const override;
        SDL_Rect getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const override;
        Component *clone() const override { return new VCCS(*this); }

        template <class Archive>
        void serialize(Archive &archive)
        {
            archive(::cereal::base_class<Component>(this), ctrlNode1, ctrlNode2);
        }
    };

    class CCVS : public Component
    {
    public:
        std::string controllingVoltageSourceName;
        int controllingSourceIndex;

        CCVS(const std::string &n, int n1, int n2, const std::string &ctrlName, double gain)
            : Component(CCVS_SOURCE, n, n1, n2, gain),
              controllingVoltageSourceName(ctrlName),
              controllingSourceIndex(-1) {}

        // Called by Circuit after every voltage source has been assigned an index.
        void resolveIndices(const std::unordered_map<std::string, int> &voltageSourceIndexMap);

        void stamp(StampMatrices m) override;
        std::string getType() override { return "CCVS"; }
        void render(SDL_Renderer *renderer,
                    const std::map<int, SDL_Point> &nodePositions) const override;
        SDL_Rect getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const override;
        Component *clone() const override { return new CCVS(*this); }

        template <class Archive>
        void serialize(Archive &archive)
        {
            archive(::cereal::base_class<Component>(this),
                    controllingVoltageSourceName, controllingSourceIndex);
        }
    };

    class CCCS : public Component
    {
    public:
        std::string controllingVoltageSourceName;
        int controllingSourceIndex;

        CCCS(const std::string &n, int n1, int n2, const std::string &ctrlName, double gain)
            : Component(CCCS_SOURCE, n, n1, n2, gain),
              controllingVoltageSourceName(ctrlName),
              controllingSourceIndex(-1) {}

        void resolveIndices(const std::unordered_map<std::string, int> &voltageSourceIndexMap);

        void stamp(StampMatrices m) override;
        std::string getType() override { return "CCCS"; }
        void render(SDL_Renderer *renderer,
                    const std::map<int, SDL_Point> &nodePositions) const override;
        SDL_Rect getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const override;
        Component *clone() const override { return new CCCS(*this); }

        template <class Archive>
        void serialize(Archive &archive)
        {
            archive(::cereal::base_class<Component>(this),
                    controllingVoltageSourceName, controllingSourceIndex);
        }
    };

} // namespace nutspice

#endif // NUTSPICE_DEPENDENT_SOURCES_H
