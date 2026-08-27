#include "Passives.h"
#include "Theme.h"
#include "UI.h"
#include "Circuit.h" // for Circuit::currentTimeStep / currentTime

#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace nutspice
{

    namespace
    {

        // Convenience: get a node position with a sensible fallback.
        SDL_Point posOf(const std::map<int, SDL_Point> &nodePositions, int node)
        {
            auto it = nodePositions.find(node);
            if (it == nodePositions.end())
                return {0, 0};
            return it->second;
        }

        // Common bounding-box computation for two-terminal components.
        SDL_Rect twoTerminalBBox(const std::map<int, SDL_Point> &nodePositions,
                                 int node1, int node2, int pad = 15)
        {
            SDL_Point p1 = posOf(nodePositions, node1);
            SDL_Point p2 = posOf(nodePositions, node2);
            SDL_Rect r;
            r.x = std::min(p1.x, p2.x) - pad;
            r.y = std::min(p1.y, p2.y) - pad;
            r.w = std::abs(p1.x - p2.x) + pad * 2;
            r.h = std::abs(p1.y - p2.y) + pad * 2;
            return r;
        }

    } // namespace

    // =============================================================================
    //  Resistor
    // =============================================================================
    void Resistor::stamp(StampMatrices m)
    {
        if (value == 0.0)
            return;
        double conductance = 1.0 / value;

        if (node1 != 0)
        {
            m.G[node1 - 1][node1 - 1] += conductance;
            if (node2 != 0)
            {
                m.G[node1 - 1][node2 - 1] -= conductance;
                m.G[node2 - 1][node1 - 1] -= conductance;
            }
        }
        if (node2 != 0)
        {
            m.G[node2 - 1][node2 - 1] += conductance;
        }
    }

    double Resistor::getCurrent(const std::vector<double> &nodeVoltages) const
    {
        if (value == 0.0)
            return 0.0;
        double v1 = (node1 == 0) ? 0.0 : nodeVoltages[node1 - 1];
        double v2 = (node2 == 0) ? 0.0 : nodeVoltages[node2 - 1];
        return (v1 - v2) / value;
    }

    void Resistor::render(SDL_Renderer *renderer,
                          const std::map<int, SDL_Point> &nodePositions) const
    {
        SDL_Point p1 = posOf(nodePositions, node1);
        SDL_Point p2 = posOf(nodePositions, node2);

        const int segments = 5;
        const int amplitude = 10;

        float dx = float(p2.x - p1.x);
        float dy = float(p2.y - p1.y);
        float length = std::sqrt(dx * dx + dy * dy);
        if (length == 0.0f)
            return;
        dx /= length;
        dy /= length;
        float px = -dy, py = dx;

        SDL_Point points[segments + 1];
        for (int i = 0; i <= segments; i++)
        {
            float t = float(i) / segments;
            float x = p1.x + t * (p2.x - p1.x);
            float y = p1.y + t * (p2.y - p1.y);
            float offset = (i % 2) ? amplitude : -amplitude;
            points[i].x = int(x + px * offset);
            points[i].y = int(y + py * offset);
        }

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g,
                               currentTheme.text.b, 255);
        SDL_RenderDrawLines(renderer, points, segments + 1);

        int midX = (p1.x + p2.x) / 2 + int(px * amplitude * 2);
        int midY = (p1.y + p2.y) / 2 + int(py * amplitude * 2);
        renderText(name, midX, midY);
    }

    SDL_Rect Resistor::getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const
    {
        return twoTerminalBBox(nodePositions, node1, node2);
    }

    // =============================================================================
    //  Capacitor (companion model: backward Euler)
    // =============================================================================
    void Capacitor::stamp(StampMatrices m)
    {
        double dt = Circuit::currentTimeStep;
        double geq = (dt > 0.0) ? value / dt : 1e-12;

        if (node1 != 0)
        {
            m.G[node1 - 1][node1 - 1] += geq;
            if (node2 != 0)
            {
                m.G[node1 - 1][node2 - 1] -= geq;
                m.G[node2 - 1][node1 - 1] -= geq;
            }
        }
        if (node2 != 0)
        {
            m.G[node2 - 1][node2 - 1] += geq;
        }

        double ieq = -geq * previousVoltage;
        if (node1 != 0)
            m.J[node1 - 1] -= ieq;
        if (node2 != 0)
            m.J[node2 - 1] += ieq;
    }

    void Capacitor::update(double dt, const std::vector<double> &nodeVoltages)
    {
        double v1 = (node1 == 0) ? 0.0 : nodeVoltages[node1 - 1];
        double v2 = (node2 == 0) ? 0.0 : nodeVoltages[node2 - 1];
        double voltage = v1 - v2;
        if (dt > 0.0)
            current = value * (voltage - previousVoltage) / dt;
        previousVoltage = voltage;
    }

    double Capacitor::getCurrent(const std::vector<double> &) const { return current; }

    void Capacitor::render(SDL_Renderer *renderer,
                           const std::map<int, SDL_Point> &nodePositions) const
    {
        SDL_Point p1 = posOf(nodePositions, node1);
        SDL_Point p2 = posOf(nodePositions, node2);

        const int gap = 12;
        float dx = float(p2.x - p1.x);
        float dy = float(p2.y - p1.y);
        float length = std::sqrt(dx * dx + dy * dy);
        if (length == 0.0f)
            return;
        dx /= length;
        dy /= length;
        float px = -dy, py = dx;

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g,
                               currentTheme.text.b, 255);
        SDL_RenderDrawLine(renderer, p1.x, p1.y,
                           int(p1.x + dx * (length / 2 - gap)),
                           int(p1.y + dy * (length / 2 - gap)));
        SDL_RenderDrawLine(renderer,
                           int(p1.x + dx * (length / 2 + gap)),
                           int(p1.y + dy * (length / 2 + gap)),
                           p2.x, p2.y);

        SDL_Point plate1[2] = {
            {int(p1.x + dx * (length / 2 - gap) + px * gap),
             int(p1.y + dy * (length / 2 - gap) + py * gap)},
            {int(p1.x + dx * (length / 2 - gap) - px * gap),
             int(p1.y + dy * (length / 2 - gap) - py * gap)}};
        SDL_Point plate2[2] = {
            {int(p1.x + dx * (length / 2 + gap) + px * gap),
             int(p1.y + dy * (length / 2 + gap) + py * gap)},
            {int(p1.x + dx * (length / 2 + gap) - px * gap),
             int(p1.y + dy * (length / 2 + gap) - py * gap)}};
        SDL_RenderDrawLines(renderer, plate1, 2);
        SDL_RenderDrawLines(renderer, plate2, 2);

        int midX = (p1.x + p2.x) / 2 + int(px * gap * 2);
        int midY = (p1.y + p2.y) / 2 + int(py * gap * 2);
        renderText(name, midX, midY);
    }

    SDL_Rect Capacitor::getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const
    {
        return twoTerminalBBox(nodePositions, node1, node2);
    }

    // =============================================================================
    //  Inductor (companion model: backward Euler, treated as internal voltage source)
    // =============================================================================
    void Inductor::stamp(StampMatrices m)
    {
        int index = m.nextVariable++;
        double dt = Circuit::currentTimeStep;
        double L = value;
        double Leq = (dt > 0.0) ? L / dt : 1e12;
        double Ieq = current;

        if (node1 != 0)
        {
            m.B[node1 - 1][index] = 1;
            m.C[index][node1 - 1] = 1;
        }
        if (node2 != 0)
        {
            m.B[node2 - 1][index] = -1;
            m.C[index][node2 - 1] = -1;
        }
        m.D[index][index] = -Leq;
        m.E[index] = -Leq * Ieq;
    }

    void Inductor::update(double dt, const std::vector<double> &nodeVoltages)
    {
        double v1 = (node1 == 0) ? 0.0 : nodeVoltages[node1 - 1];
        double v2 = (node2 == 0) ? 0.0 : nodeVoltages[node2 - 1];
        double voltageAcross = v1 - v2;
        if (value > 0.0 && dt > 0.0)
            current += voltageAcross * dt / value;
        previousCurrent = current;
    }

    double Inductor::getCurrent(const std::vector<double> &) const { return current; }

    void Inductor::render(SDL_Renderer *renderer,
                          const std::map<int, SDL_Point> &nodePositions) const
    {
        SDL_Point p1 = posOf(nodePositions, node1);
        SDL_Point p2 = posOf(nodePositions, node2);

        const int loops = 3;
        const int radius = 8;

        float dx = float(p2.x - p1.x);
        float dy = float(p2.y - p1.y);
        float length = std::sqrt(dx * dx + dy * dy);
        if (length == 0.0f)
            return;
        dx /= length;
        dy /= length;
        float px = -dy, py = dx;

        float segmentLength = length / (loops * 2);
        SDL_Point prevPoint = {p1.x, p1.y};

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g,
                               currentTheme.text.b, 255);
        SDL_RenderDrawLine(renderer, p1.x, p1.y,
                           int(p1.x + dx * segmentLength),
                           int(p1.y + dy * segmentLength));

        for (int i = 0; i < loops; i++)
        {
            float centerX = p1.x + dx * (i * 2 + 1) * segmentLength;
            float centerY = p1.y + dy * (i * 2 + 1) * segmentLength;
            for (int angle = -90; angle <= 90; angle += 5)
            {
                float rad = angle * float(M_PI) / 180.0f;
                int x = int(centerX + px * radius * std::cos(rad));
                int y = int(centerY + py * radius * std::sin(rad));
                if (angle == -90)
                {
                    prevPoint = {x, y};
                }
                else
                {
                    SDL_RenderDrawLine(renderer, prevPoint.x, prevPoint.y, x, y);
                    prevPoint = {x, y};
                }
            }
        }
        SDL_RenderDrawLine(renderer, prevPoint.x, prevPoint.y, p2.x, p2.y);

        int midX = (p1.x + p2.x) / 2 + int(px * radius * 2);
        int midY = (p1.y + p2.y) / 2 + int(py * radius * 2);
        renderText(name, midX, midY);
    }

    SDL_Rect Inductor::getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const
    {
        return twoTerminalBBox(nodePositions, node1, node2);
    }

    // =============================================================================
    //  Diode (piecewise-linear: ~open below Vd, ~short above Vd)
    // =============================================================================
    namespace
    {
        class DiodeModel
        { // internal helper, not exposed
        public:
            static constexpr double Is = 1e-14;
            static constexpr double Vt = 0.026;
            static constexpr double n = 1.0;
        };
    }

    void Diode::stamp(StampMatrices m)
    {
        // Kept identical to the original behaviour; only the matrix handle changes.
        double effectiveResistance = (value > 0.7) ? 1e-6 : 1e12;
        double conductance = 1.0 / effectiveResistance;

        if (node1 != 0)
        {
            m.G[node1 - 1][node1 - 1] += conductance;
            if (node2 != 0)
            {
                m.G[node1 - 1][node2 - 1] -= conductance;
                m.G[node2 - 1][node1 - 1] -= conductance;
            }
        }
        if (node2 != 0)
        {
            m.G[node2 - 1][node2 - 1] += conductance;
        }
    }

    void Diode::update(double /*dt*/, const std::vector<double> &nodeVoltages)
    {
        double v1 = (node1 == 0) ? 0.0 : nodeVoltages[node1 - 1];
        double v2 = (node2 == 0) ? 0.0 : nodeVoltages[node2 - 1];
        double lastVoltage = v1 - v2;
        value = lastVoltage; // store the latest forward voltage in `value`
        // current is recomputed on demand in getCurrent()
    }

    double Diode::getCurrent(const std::vector<double> &) const
    {
        return (value > 0.7) ? value / 1e-6 : value / 1e12;
    }

    void Diode::render(SDL_Renderer *renderer,
                       const std::map<int, SDL_Point> &nodePositions) const
    {
        SDL_Point p1 = posOf(nodePositions, node1);
        SDL_Point p2 = posOf(nodePositions, node2);

        const int triangleSize = 15;
        float dx = float(p2.x - p1.x);
        float dy = float(p2.y - p1.y);
        float length = std::sqrt(dx * dx + dy * dy);
        if (length == 0.0f)
            return;
        dx /= length;
        dy /= length;
        float px = -dy, py = dx;

        int midX = (p1.x + p2.x) / 2;
        int midY = (p1.y + p2.y) / 2;

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g,
                               currentTheme.text.b, 255);
        SDL_RenderDrawLine(renderer, p1.x, p1.y,
                           int(midX - dx * triangleSize),
                           int(midY - dy * triangleSize));
        SDL_RenderDrawLine(renderer,
                           int(midX + dx * triangleSize),
                           int(midY + dy * triangleSize),
                           p2.x, p2.y);

        SDL_Point triangle[4];
        triangle[0] = {int(midX - dx * triangleSize), int(midY - dy * triangleSize)};
        triangle[1] = {int(midX + px * triangleSize), int(midY + py * triangleSize)};
        triangle[2] = {int(midX - px * triangleSize), int(midY - py * triangleSize)};
        triangle[3] = triangle[0];
        SDL_RenderDrawLines(renderer, triangle, 4);

        SDL_RenderDrawLine(renderer,
                           int(midX + dx * triangleSize / 2 + px * triangleSize / 2),
                           int(midY + dy * triangleSize / 2 + py * triangleSize / 2),
                           int(midX + dx * triangleSize / 2 - px * triangleSize / 2),
                           int(midY + dy * triangleSize / 2 - py * triangleSize / 2));

        renderText(name, int(midX + px * triangleSize * 2),
                   int(midY + py * triangleSize * 2));
    }

    SDL_Rect Diode::getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const
    {
        return twoTerminalBBox(nodePositions, node1, node2);
    }

    // =============================================================================
    //  Ground
    // =============================================================================
    void Ground::render(SDL_Renderer *renderer,
                        const std::map<int, SDL_Point> &nodePositions) const
    {
        SDL_Point p = posOf(nodePositions, node1);
        const int size = 15;

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g,
                               currentTheme.text.b, 255);
        SDL_RenderDrawLine(renderer, p.x, p.y, p.x, p.y + size);
        SDL_RenderDrawLine(renderer, p.x - size, p.y + size, p.x + size, p.y + size);
        SDL_RenderDrawLine(renderer, p.x - size / 2, p.y + size * 1.5f, p.x + size / 2, p.y + size * 1.5f);
        SDL_RenderDrawLine(renderer, p.x - size / 4, p.y + size * 2, p.x + size / 4, p.y + size * 2);

        renderText(name, p.x + size + 5, p.y + size);
    }

    SDL_Rect Ground::getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const
    {
        // Ground has only node1 (node2 is always 0).  The original code called
        // nodePositions.at(node2) which would either return the GND position or
        // throw — never correct.  Use a square around node1 instead.
        SDL_Point p = posOf(nodePositions, node1);
        return {p.x - 25, p.y - 5, 50, 50};
    }

    // =============================================================================
    //  Wire (modeled as a 1e-6 ohm "resistor" with type = RESISTOR)
    // =============================================================================
    void Wire::stamp(StampMatrices m)
    {
        double conductance = 1.0 / value; // value == 1e-6
        if (node1 != 0)
        {
            m.G[node1 - 1][node1 - 1] += conductance;
            if (node2 != 0)
            {
                m.G[node1 - 1][node2 - 1] -= conductance;
                m.G[node2 - 1][node1 - 1] -= conductance;
            }
        }
        if (node2 != 0)
        {
            m.G[node2 - 1][node2 - 1] += conductance;
        }
    }

    void Wire::render(SDL_Renderer *renderer,
                      const std::map<int, SDL_Point> &nodePositions) const
    {
        SDL_Point p1 = posOf(nodePositions, node1);
        SDL_Point p2 = posOf(nodePositions, node2);

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g,
                               currentTheme.text.b, 255);
        SDL_RenderDrawLine(renderer, p1.x, p1.y, p2.x, p2.y);

        int midX = (p1.x + p2.x) / 2;
        int midY = (p1.y + p2.y) / 2;
        SDL_Rect wireIndicator = {midX - 2, midY - 2, 4, 4};
        SDL_RenderFillRect(renderer, &wireIndicator);
    }

    SDL_Rect Wire::getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const
    {
        return twoTerminalBBox(nodePositions, node1, node2, 5);
    }

} // namespace nutspice
