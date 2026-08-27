#include "DependentSources.h"
#include "Theme.h"
#include "UI.h"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

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

        void drawDiamond(SDL_Renderer *renderer, int centerX, int centerY, int size)
        {
            SDL_Point diamond[5] = {
                {centerX, centerY - size},
                {centerX + size, centerY},
                {centerX, centerY + size},
                {centerX - size, centerY},
                {centerX, centerY - size}};
            SDL_RenderDrawLines(renderer, diamond, 5);
        }

        // Draws a dashed line between (x1,y1) and (x2,y2) — used for the
        // "sensing" wire on VCVS / VCCS.
        void drawDashedLine(SDL_Renderer *renderer, int x1, int y1, int x2, int y2, int dashLength = 5)
        {
            float dx = float(x2 - x1);
            float dy = float(y2 - y1);
            float distance = std::sqrt(dx * dx + dy * dy);
            if (distance == 0.0f)
                return;
            dx /= distance;
            dy /= distance;

            for (float i = 0; i < distance; i += dashLength * 2)
            {
                float startX = x1 + dx * i;
                float startY = y1 + dy * i;
                float endX = x1 + dx * (i + dashLength);
                float endY = y1 + dy * (i + dashLength);
                if (endX - x1 > dx * distance)
                    endX = x1 + dx * distance;
                if (endY - y1 > dy * distance)
                    endY = y1 + dy * distance;
                SDL_RenderDrawLine(renderer, int(startX), int(startY), int(endX), int(endY));
            }
        }

        // Draws the small "+" marker on the +ve sense node.
        void drawPlusMarker(SDL_Renderer *renderer, int x, int y)
        {
            SDL_RenderDrawLine(renderer, x - 5, y, x + 5, y);
            SDL_RenderDrawLine(renderer, x, y - 5, x, y + 5);
        }

        // Draws the small "-" marker on the -ve sense node.
        void drawMinusMarker(SDL_Renderer *renderer, int x, int y)
        {
            SDL_RenderDrawLine(renderer, x - 5, y, x + 5, y);
        }

        // Draws an arrow head — used for CCVS / CCCS control-sense pointer.
        void drawArrowHead(SDL_Renderer *renderer, int tipX, int tipY, int back, int spread)
        {
            SDL_RenderDrawLine(renderer, tipX, tipY, tipX - back, tipY - spread);
            SDL_RenderDrawLine(renderer, tipX, tipY, tipX - back, tipY + spread);
        }

    } // namespace

    // =============================================================================
    //  VCVS — V_out = gain * (V_ctrlNode1 - V_ctrlNode2)
    // =============================================================================
    void VCVS::stamp(StampMatrices m)
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

        if (ctrlNode1 != 0)
            m.D[vsIndex][ctrlNode1 - 1] -= value;
        if (ctrlNode2 != 0)
            m.D[vsIndex][ctrlNode2 - 1] += value;
    }

    void VCVS::render(SDL_Renderer *renderer,
                      const std::map<int, SDL_Point> &nodePositions) const
    {
        SDL_Point p1 = posOf(nodePositions, node1);
        SDL_Point p2 = posOf(nodePositions, node2);
        SDL_Point cp1 = posOf(nodePositions, ctrlNode1);
        SDL_Point cp2 = posOf(nodePositions, ctrlNode2);

        const int size = 15;
        int centerX = (p1.x + p2.x) / 2;
        int centerY = (p1.y + p2.y) / 2;

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g,
                               currentTheme.text.b, 255);
        drawDiamond(renderer, centerX, centerY, size);

        SDL_RenderDrawLine(renderer, p1.x, p1.y, centerX, centerY - size);
        SDL_RenderDrawLine(renderer, p2.x, p2.y, centerX, centerY + size);

        int midCtrlX = (cp1.x + cp2.x) / 2;
        int midCtrlY = (cp1.y + cp2.y) / 2;
        drawDashedLine(renderer, midCtrlX, midCtrlY, centerX, centerY);

        drawPlusMarker(renderer, cp1.x, cp1.y);
        drawMinusMarker(renderer, cp2.x, cp2.y);

        renderText(name + " (G=" + std::to_string(value).substr(0, 4) + ")",
                   centerX + size + 5, centerY - 10);
    }

    SDL_Rect VCVS::getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const
    {
        SDL_Point p1 = posOf(nodePositions, node1);
        SDL_Point p2 = posOf(nodePositions, node2);
        SDL_Point cp1 = posOf(nodePositions, ctrlNode1);
        SDL_Point cp2 = posOf(nodePositions, ctrlNode2);
        int minX = std::min({p1.x, p2.x, cp1.x, cp2.x});
        int minY = std::min({p1.y, p2.y, cp1.y, cp2.y});
        int maxX = std::max({p1.x, p2.x, cp1.x, cp2.x});
        int maxY = std::max({p1.y, p2.y, cp1.y, cp2.y});
        return {minX - 20, minY - 20, maxX - minX + 40, maxY - minY + 40};
    }

    // =============================================================================
    //  VCCS — I_out = gm * (V_ctrlNode1 - V_ctrlNode2)
    // =============================================================================
    void VCCS::stamp(StampMatrices m)
    {
        if (node1 != 0 && ctrlNode1 != 0)
            m.G[node1 - 1][ctrlNode1 - 1] += value;
        if (node1 != 0 && ctrlNode2 != 0)
            m.G[node1 - 1][ctrlNode2 - 1] -= value;
        if (node2 != 0 && ctrlNode1 != 0)
            m.G[node2 - 1][ctrlNode1 - 1] -= value;
        if (node2 != 0 && ctrlNode2 != 0)
            m.G[node2 - 1][ctrlNode2 - 1] += value;
    }

    void VCCS::render(SDL_Renderer *renderer,
                      const std::map<int, SDL_Point> &nodePositions) const
    {
        SDL_Point p1 = posOf(nodePositions, node1);
        SDL_Point p2 = posOf(nodePositions, node2);
        SDL_Point cp1 = posOf(nodePositions, ctrlNode1);
        SDL_Point cp2 = posOf(nodePositions, ctrlNode2);

        const int size = 15;
        int centerX = (p1.x + p2.x) / 2;
        int centerY = (p1.y + p2.y) / 2;

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g,
                               currentTheme.text.b, 255);
        drawDiamond(renderer, centerX, centerY, size);

        // Arrow inside diamond to indicate "current source".
        SDL_RenderDrawLine(renderer, centerX, centerY - size / 2,
                           centerX, centerY + size / 2);
        SDL_RenderDrawLine(renderer, centerX, centerY + size / 2,
                           centerX - size / 3, centerY + size / 4);
        SDL_RenderDrawLine(renderer, centerX, centerY + size / 2,
                           centerX + size / 3, centerY + size / 4);

        SDL_RenderDrawLine(renderer, p1.x, p1.y, centerX - size, centerY);
        SDL_RenderDrawLine(renderer, p2.x, p2.y, centerX + size, centerY);

        int midCtrlX = (cp1.x + cp2.x) / 2;
        int midCtrlY = (cp1.y + cp2.y) / 2;
        drawDashedLine(renderer, midCtrlX, midCtrlY, centerX, centerY);

        drawPlusMarker(renderer, cp1.x, cp1.y);
        drawMinusMarker(renderer, cp2.x, cp2.y);

        renderText(name + " (gm=" + std::to_string(value).substr(0, 4) + ")",
                   centerX + size + 5, centerY - 10);
    }

    SDL_Rect VCCS::getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const
    {
        SDL_Point p1 = posOf(nodePositions, node1);
        SDL_Point p2 = posOf(nodePositions, node2);
        SDL_Point cp1 = posOf(nodePositions, ctrlNode1);
        SDL_Point cp2 = posOf(nodePositions, ctrlNode2);
        int minX = std::min({p1.x, p2.x, cp1.x, cp2.x});
        int minY = std::min({p1.y, p2.y, cp1.y, cp2.y});
        int maxX = std::max({p1.x, p2.x, cp1.x, cp2.x});
        int maxY = std::max({p1.y, p2.y, cp1.y, cp2.y});
        return {minX - 20, minY - 20, maxX - minX + 40, maxY - minY + 40};
    }

    // =============================================================================
    //  CCVS — V_out = gain * I_ctrl   (I_ctrl = through-current of another VSource)
    // =============================================================================
    void CCVS::resolveIndices(const std::unordered_map<std::string, int> &voltageSourceIndexMap)
    {
        auto it = voltageSourceIndexMap.find(controllingVoltageSourceName);
        if (it == voltageSourceIndexMap.end())
        {
            throw std::runtime_error("CCVS '" + name + "': controlling voltage source '" +
                                     controllingVoltageSourceName + "' not found");
        }
        controllingSourceIndex = it->second;
    }

    void CCVS::stamp(StampMatrices m)
    {
        if (controllingSourceIndex < 0)
        {
            throw std::runtime_error("CCVS '" + name + "': resolveIndices() not called");
        }
        int vsIndex = m.nextVariable++;

        if (node1 != 0)
            m.B[node1 - 1][vsIndex] = 1;
        if (node2 != 0)
            m.B[node2 - 1][vsIndex] = -1;
        if (node1 != 0)
            m.C[vsIndex][node1 - 1] = 1;
        if (node2 != 0)
            m.C[vsIndex][node2 - 1] = -1;

        m.D[vsIndex][controllingSourceIndex] -= value;
    }

    void CCVS::render(SDL_Renderer *renderer,
                      const std::map<int, SDL_Point> &nodePositions) const
    {
        SDL_Point p1 = posOf(nodePositions, node1);
        SDL_Point p2 = posOf(nodePositions, node2);

        const int size = 15;
        int centerX = (p1.x + p2.x) / 2;
        int centerY = (p1.y + p2.y) / 2;

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g,
                               currentTheme.text.b, 255);
        drawDiamond(renderer, centerX, centerY, size);
        SDL_RenderDrawLine(renderer, p1.x, p1.y, centerX, centerY - size);
        SDL_RenderDrawLine(renderer, p2.x, p2.y, centerX, centerY + size);

        SDL_Point controlPoint = {centerX + size * 2, centerY};
        SDL_RenderDrawLine(renderer, centerX + size, centerY, controlPoint.x, controlPoint.y);
        drawArrowHead(renderer, controlPoint.x, controlPoint.y, 5, 5);

        renderText(name + " (H=" + std::to_string(value).substr(0, 4) + ")",
                   centerX + size + 5, centerY - 10);
        renderText("ctrl: " + controllingVoltageSourceName,
                   centerX - 50, centerY + size + 15);
    }

    SDL_Rect CCVS::getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const
    {
        SDL_Point p1 = posOf(nodePositions, node1);
        SDL_Point p2 = posOf(nodePositions, node2);
        int minX = std::min(p1.x, p2.x) - 30;
        int minY = std::min(p1.y, p2.y) - 30;
        int maxX = std::max(p1.x, p2.x) + 30;
        int maxY = std::max(p1.y, p2.y) + 30;
        return {minX, minY, maxX - minX, maxY - minY};
    }

    // =============================================================================
    //  CCCS — I_out = gain * I_ctrl   (I_ctrl = through-current of another VSource)
    //  FIXED: stamp on B matrix, NOT on D.
    // =============================================================================
    void CCCS::resolveIndices(const std::unordered_map<std::string, int> &voltageSourceIndexMap)
    {
        auto it = voltageSourceIndexMap.find(controllingVoltageSourceName);
        if (it == voltageSourceIndexMap.end())
        {
            throw std::runtime_error("CCCS '" + name + "': controlling voltage source '" +
                                     controllingVoltageSourceName + "' not found");
        }
        controllingSourceIndex = it->second;
    }

    void CCCS::stamp(StampMatrices m)
    {
        if (controllingSourceIndex < 0)
        {
            throw std::runtime_error("CCCS '" + name + "': resolveIndices() not called");
        }

        // The output current = gain * x[numNodes + controllingSourceIndex].
        // In MNA, the contribution of an extra-variable to the node-1 equation is
        // B[node1-1][k] * x_k.  Setting B[node1-1][k] = +gain means the current
        // ENTERING node1 from this CCCS is +gain * I_ctrl — and the J-side
        // convention is "currents leaving the node are negative on the LHS", so
        // a current entering node1 corresponds to B[node1-1][k] = +gain.
        //
        // The original code wrote to D[node1-1] which is wrong because D is
        // indexed by SOURCE variables, not by nodes.
        if (node1 != 0)
            m.B[node1 - 1][controllingSourceIndex] += value;
        if (node2 != 0)
            m.B[node2 - 1][controllingSourceIndex] -= value;
    }

    void CCCS::render(SDL_Renderer *renderer,
                      const std::map<int, SDL_Point> &nodePositions) const
    {
        SDL_Point p1 = posOf(nodePositions, node1);
        SDL_Point p2 = posOf(nodePositions, node2);

        const int size = 15;
        int centerX = (p1.x + p2.x) / 2;
        int centerY = (p1.y + p2.y) / 2;

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g,
                               currentTheme.text.b, 255);
        drawDiamond(renderer, centerX, centerY, size);

        // Arrow inside diamond.
        SDL_RenderDrawLine(renderer, centerX, centerY - size / 2,
                           centerX, centerY + size / 2);
        SDL_RenderDrawLine(renderer, centerX, centerY + size / 2,
                           centerX - size / 3, centerY + size / 4);
        SDL_RenderDrawLine(renderer, centerX, centerY + size / 2,
                           centerX + size / 3, centerY + size / 4);

        SDL_RenderDrawLine(renderer, p1.x, p1.y, centerX - size, centerY);
        SDL_RenderDrawLine(renderer, p2.x, p2.y, centerX + size, centerY);

        SDL_Point controlPoint = {centerX + size * 2, centerY};
        SDL_RenderDrawLine(renderer, centerX + size, centerY, controlPoint.x, controlPoint.y);
        drawArrowHead(renderer, controlPoint.x, controlPoint.y, 5, 5);

        renderText(name + " (F=" + std::to_string(value).substr(0, 4) + ")",
                   centerX + size + 5, centerY - 10);
        renderText("ctrl: " + controllingVoltageSourceName,
                   centerX - 50, centerY + size + 15);
    }

    SDL_Rect CCCS::getBoundingBox(const std::map<int, SDL_Point> &nodePositions) const
    {
        SDL_Point p1 = posOf(nodePositions, node1);
        SDL_Point p2 = posOf(nodePositions, node2);
        int minX = std::min(p1.x, p2.x) - 30;
        int minY = std::min(p1.y, p2.y) - 30;
        int maxX = std::max(p1.x, p2.x) + 30;
        int maxY = std::max(p1.y, p2.y) + 30;
        return {minX, minY, maxX - minX, maxY - minY};
    }

} // namespace nutspice
