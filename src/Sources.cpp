#include "Sources.h"
#include "Theme.h"
#include "UI.h"
#include "Circuit.h"

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

        SDL_Point posOf(const std::map<int, SDL_Point> &nodePositions, int node)
        {
            auto it = nodePositions.find(node);
            if (it == nodePositions.end())
                return {0, 0};
            return it->second;
        }

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

        // Renders the standard circle-with-symbol motif used by V and I sources.
        void renderCircleSource(SDL_Renderer *renderer,
                                const std::map<int, SDL_Point> &nodePositions,
                                int node1, int node2,
                                const std::string &name,
                                const std::string &glyphLines)
        {
            SDL_Point p1 = posOf(nodePositions, node1);
            SDL_Point p2 = posOf(nodePositions, node2);

            const int radius = 12;
            int centerX = (p1.x + p2.x) / 2;
            int centerY = (p1.y + p2.y) / 2;

            SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g,
                                   currentTheme.text.b, 255);
            SDL_RenderDrawLine(renderer, p1.x, p1.y, centerX - radius, centerY);
            SDL_RenderDrawLine(renderer, centerX + radius, centerY, p2.x, p2.y);

            for (int angle = 0; angle < 360; angle += 10)
            {
                float rad = angle * float(M_PI) / 180.0f;
                int x = int(centerX + radius * std::cos(rad));
                int y = int(centerY + radius * std::sin(rad));
                SDL_RenderDrawPoint(renderer, x, y);
            }

            // Draw the simple glyph lines (e.g. "+ / -" for V, arrow for I).
            // `glyphLines` is a small DSL: "L:x1,y1,x2,y2|L:x1,y1,x2,y2|..."
            std::string s = glyphLines;
            size_t pos = 0;
            while (pos < s.size())
            {
                if (s.substr(pos, 2) == "L:")
                {
                    pos += 2;
                    int coords[4] = {0, 0, 0, 0};
                    for (int i = 0; i < 4; i++)
                    {
                        size_t comma = s.find_first_of(",|", pos);
                        coords[i] = std::stoi(s.substr(pos, comma - pos));
                        pos = (s[comma] == ',') ? comma + 1 : comma;
                    }
                    SDL_RenderDrawLine(renderer, centerX + coords[0], centerY + coords[1],
                                       centerX + coords[2], centerY + coords[3]);
                }
                else if (s.substr(pos, 1) == "|")
                {
                    pos++;
                }
                else
                {
                    pos++;
                }
            }

            renderText(name, centerX + radius + 5, centerY - 10);
        }

    } // namespace

    // =============================================================================
    //  VoltageSource
    // =============================================================================
    void VoltageSource::stamp(StampMatrices m)
    {
        int vsIndex = m.nextVariable++;
        if (node1 != 0)
            m.B[node1 - 1][vsIndex] = 1;
        if (node2 != 0)
            m.B[node2 - 1][vsIndex] = -1;
        if (node1 != 0)
            m.C[vsIndex][node1 - 1] = 1;
        if (node2 != 0)
            m.C[vsIndex][node2 - 1] = -1;
        m.E[vsIndex] = value;
    }

    void VoltageSource::render(SDL_Renderer *renderer,
                               const std::map<int, SDL_Point> &nodePositions) const
    {
        renderCircleSource(renderer, nodePositions, node1, node2, name,
                           "L:-5,0,5,0|L:0,-5,0,5");
    }

    SDL_Rect VoltageSource::getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const
    {
        return twoTerminalBBox(nodePositions, node1, node2);
    }

    // =============================================================================
    //  SinVoltageSource
    // =============================================================================
    void SinVoltageSource::stamp(StampMatrices m)
    {
        int vsIndex = m.nextVariable++;
        if (node1 != 0)
            m.B[node1 - 1][vsIndex] = 1;
        if (node2 != 0)
            m.B[node2 - 1][vsIndex] = -1;
        if (node1 != 0)
            m.C[vsIndex][node1 - 1] = 1;
        if (node2 != 0)
            m.C[vsIndex][node2 - 1] = -1;

        double t = Circuit::currentTime;
        double radians = phase * M_PI / 180.0;
        m.E[vsIndex] = offset + amplitude * std::sin(2.0 * M_PI * frequency * t + radians);
    }

    std::string SinVoltageSource::getInfo() const
    {
        return "Sinusoidal Voltage Source " + name + " " +
               std::to_string(node1) + " " + std::to_string(node2) +
               " DC=" + std::to_string(offset) + " AMP=" + std::to_string(amplitude) +
               " FREQ=" + std::to_string(frequency) + " PHASE=" + std::to_string(phase);
    }

    void SinVoltageSource::render(SDL_Renderer *renderer,
                                  const std::map<int, SDL_Point> &nodePositions) const
    {
        SDL_Point p1 = posOf(nodePositions, node1);
        SDL_Point p2 = posOf(nodePositions, node2);

        const int segments = 16;
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
            float offset = amplitude * std::sin(t * float(M_PI) * 2);
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

    SDL_Rect SinVoltageSource::getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const
    {
        return twoTerminalBBox(nodePositions, node1, node2);
    }

    // =============================================================================
    //  PulseVoltageSource
    // =============================================================================
    void PulseVoltageSource::stamp(StampMatrices m)
    {
        int vsIndex = m.nextVariable++;
        if (node1 != 0)
            m.B[node1 - 1][vsIndex] = 1;
        if (node2 != 0)
            m.B[node2 - 1][vsIndex] = -1;
        if (node1 != 0)
            m.C[vsIndex][node1 - 1] = 1;
        if (node2 != 0)
            m.C[vsIndex][node2 - 1] = -1;

        double t = Circuit::currentTime;
        double cycleTime = std::fmod(t - td, per);
        if (t < td)
        {
            m.E[vsIndex] = v1;
        }
        else if (cycleTime < tr)
        {
            m.E[vsIndex] = v1 + (v2 - v1) * (cycleTime / tr);
        }
        else if (cycleTime < tr + pw)
        {
            m.E[vsIndex] = v2;
        }
        else if (cycleTime < tr + pw + tf)
        {
            m.E[vsIndex] = v2 + (v1 - v2) * (cycleTime - tr - pw) / tf;
        }
        else
        {
            m.E[vsIndex] = v1;
        }
    }

    std::string PulseVoltageSource::getInfo() const
    {
        return "Pulse Voltage Source " + name + " " +
               std::to_string(node1) + " " + std::to_string(node2) +
               " V1=" + std::to_string(v1) + " V2=" + std::to_string(v2) +
               " TD=" + std::to_string(td) + " TR=" + std::to_string(tr) +
               " TF=" + std::to_string(tf) + " PW=" + std::to_string(pw) +
               " PER=" + std::to_string(per);
    }

    void PulseVoltageSource::render(SDL_Renderer *renderer,
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
            float offset = (i > 1 && i < 4) ? amplitude : -amplitude;
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

    SDL_Rect PulseVoltageSource::getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const
    {
        return twoTerminalBBox(nodePositions, node1, node2);
    }

    // =============================================================================
    //  CurrentSource
    // =============================================================================
    void CurrentSource::stamp(StampMatrices m)
    {
        if (node1 != 0)
            m.J[node1 - 1] -= value; // current leaves node1
        if (node2 != 0)
            m.J[node2 - 1] += value; // current enters node2
    }

    void CurrentSource::render(SDL_Renderer *renderer,
                               const std::map<int, SDL_Point> &nodePositions) const
    {
        // Arrow pointing up: vertical bar + diagonal arrowheads.
        renderCircleSource(renderer, nodePositions, node1, node2, name,
                           "L:0,-8,0,8|L:0,8,-5,3|L:0,8,5,3");
    }

    SDL_Rect CurrentSource::getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const
    {
        return twoTerminalBBox(nodePositions, node1, node2);
    }

    // =============================================================================
    //  SinCurrentSource  — BUG FIXED (sign on J[node1-1] was +, should be -)
    // =============================================================================
    void SinCurrentSource::stamp(StampMatrices m)
    {
        double t = Circuit::currentTime;
        double radians = phase * M_PI / 180.0;
        double i = offset + amplitude * std::sin(2.0 * M_PI * frequency * t + radians);
        if (node1 != 0)
            m.J[node1 - 1] -= i; // FIXED: was +=
        if (node2 != 0)
            m.J[node2 - 1] += i; // FIXED: was +=
    }

    std::string SinCurrentSource::getInfo() const
    {
        return "Sinusoidal Current Source " + name + " " +
               std::to_string(node1) + " " + std::to_string(node2) +
               " DC=" + std::to_string(offset) + " AMP=" + std::to_string(amplitude) +
               " FREQ=" + std::to_string(frequency) + " PHASE=" + std::to_string(phase);
    }

    void SinCurrentSource::render(SDL_Renderer *renderer,
                                  const std::map<int, SDL_Point> &nodePositions) const
    {
        // Re-use the sine waveform visual.
        SinVoltageSource visualProxy(name, node1, node2, amplitude, frequency, phase, offset);
        visualProxy.render(renderer, nodePositions);
    }

    SDL_Rect SinCurrentSource::getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const
    {
        return twoTerminalBBox(nodePositions, node1, node2);
    }

    // =============================================================================
    //  PulseCurrentSource  — BUG FIXED (same sign bug as SinCurrentSource)
    // =============================================================================
    void PulseCurrentSource::stamp(StampMatrices m)
    {
        double t = Circuit::currentTime;
        double cycleTime = std::fmod(t - td, per);
        double i = i1;
        if (t < td)
        {
            i = i1;
        }
        else if (cycleTime < tr)
        {
            i = i1 + (i2 - i1) * (cycleTime / tr);
        }
        else if (cycleTime < tr + pw)
        {
            i = i2;
        }
        else if (cycleTime < tr + pw + tf)
        {
            i = i2 + (i1 - i2) * (cycleTime - tr - pw) / tf;
        }

        if (node1 != 0)
            m.J[node1 - 1] -= i; // FIXED: was +=
        if (node2 != 0)
            m.J[node2 - 1] += i; // FIXED: was +=
    }

    std::string PulseCurrentSource::getInfo() const
    {
        return "Pulse Current Source " + name + " " +
               std::to_string(node1) + " " + std::to_string(node2) +
               " I1=" + std::to_string(i1) + " I2=" + std::to_string(i2) +
               " TD=" + std::to_string(td) + " TR=" + std::to_string(tr) +
               " TF=" + std::to_string(tf) + " PW=" + std::to_string(pw) +
               " PER=" + std::to_string(per);
    }

    void PulseCurrentSource::render(SDL_Renderer *renderer,
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
            float offset = (i > 1 && i < 4) ? amplitude : -amplitude;
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

    SDL_Rect PulseCurrentSource::getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const
    {
        return twoTerminalBBox(nodePositions, node1, node2);
    }

} // namespace nutspice
