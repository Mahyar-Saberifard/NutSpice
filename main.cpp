#include <iostream>
#include <map>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <stdexcept>
#include <SDL2/SDL.h>
#include <windows.h>
#include <direct.h>
#include <SDL2/SDL2_gfx.h>
#include <SDL2/SDL_ttf.h>
#include <unordered_set>
#include <memory>
#include <cereal/types/unordered_map.hpp>
#include <cereal/archives/xml.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/archives/binary.hpp>
#include <fstream>

using namespace std;

namespace cereal {
    template <class Archive>
    void serialize(Archive& archive, std::pair<int, int>& pair) {
        archive(pair.first, pair.second);
    }

    template <class Archive>
    void serialize(Archive& archive, SDL_Point& point) {
        archive(point.x, point.y);
    }
}

#ifdef _WIN32
#define GETCWD _getcwd
#define CHDIR _chdir
#else
#include <unistd.h>
#define GETCWD getcwd
#define CHDIR chdir
#define MAX_PATH PATH_MAX
#endif

struct PlotSignal {
    vector<double> time;
    vector<double> values;
    string name;
    SDL_Color color;
};
struct Cursor {
    int x;
    int y;
    double time;
    double value;
    bool active;
    SDL_Color color;
    bool dragging;
};


vector<Cursor> cursors;
int activeCursorIndex = -1;

int selectedSignalIndex = -1;
bool showGrid = true;
bool showLegend = true;
double timeZoom = 1.0;
double valueZoom = 1.0;
double timePan = 0.0;
double valuePan = 0.0;
bool showCursor = false;
int cursorX = -1;
int cursorY = -1;
double cursorTime = 0.0;
double cursorValue = 0.0;
bool cursorDragging = false;
bool running = true;
SDL_Event event;
string inputText = "";
bool textInputActive = false;
bool FileDialog = false;
bool SaveAsDialog = false;

vector<string> listTxtFiles(const string& directory) {
    vector<string> files;
#ifdef _WIN32
    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile((directory + "\\*.txt").c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                files.push_back(findData.cFileName);
            }
        } while (FindNextFile(hFind, &findData) != 0);
        FindClose(hFind);
    }
#else
    DIR* dir;
    struct dirent* ent;
    if ((dir = opendir(directory.c_str())) {
        while ((ent = readdir(dir))) {
            string filename = ent->d_name;
            if (filename.length() > 4 && filename.substr(filename.length() - 4) == ".txt") {
                files.push_back(filename);
            }
        }
        closedir(dir);
    }
#endif
    return files;
}

void changeToPreviousDirectory() {
    char currentDir[MAX_PATH];
    if (GETCWD(currentDir, sizeof(currentDir)) == NULL) {
        cerr << "Error getting current directory" << endl;
        return;
    }

    if (CHDIR("..") != 0) {
        cerr << "Error changing to parent directory" << endl;
        return;
    }

    char newDir[MAX_PATH];
    GETCWD(newDir, sizeof(newDir));
    cout << "Changed to directory: " << newDir << endl;
}

vector<double> voltages;
vector<double> currents;
vector<double> Vtimes;
vector<double> Itimes;
bool hasDynamic = false;
bool showVoltage = true;
bool showCurrent = false;
bool FileMenu = false;
bool EditMenu = false;
bool ComponentLibrary = false;
bool AnalysisSettings = false;
bool showShortcutHelp = false;

double tStep = 0.01;
double tStop = 1;
map<int, SDL_Point> nodePositions;

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;

const SDL_Color WHITE = {255, 255, 255, 255};
const SDL_Color BLACK = {0, 0, 0, 255};
const SDL_Color RED = {220, 0, 0, 255};
const SDL_Color GREEN = {0, 200, 0, 255};
const SDL_Color BLUE = {0, 0, 220, 255};
const SDL_Color GRAY = {200, 200, 200, 255};

TTF_Font* font = nullptr;

struct Button {
    SDL_Rect rect;
    string text;
    SDL_Color color;
    bool isActive;
};

struct TextBox {
    SDL_Rect rect;
    string text;
    bool isActive;
};

SDL_Rect circuitArea = {50, 50, 800, 600};
SDL_Rect plotArea = {850, 50, 380, 600};
SDL_Rect fileMenuRect = {10, 45, 140, 250};
SDL_Rect editMenuRect = {100, 45, 140, 250};

TextBox valueBox = {{10, SCREEN_HEIGHT - 60, 100, 40}, "Value", false};
TextBox nameBox = {{300, 210, 180, 30}, "Name", false};

bool darkMode = false;

struct ThemeColors {
    SDL_Color background;
    SDL_Color text;
    SDL_Color circuitBg;
    SDL_Color plotBg;
    SDL_Color button;
    SDL_Color buttonText;
    SDL_Color toolbar;
};

ThemeColors lightTheme = {
        {240, 240, 240, 255},
        {0, 0, 0, 255},
        {255, 255, 255, 255},
        {255, 255, 255, 255},
        {200, 200, 200, 255},
        {0, 0, 0, 255},
        {180, 180, 180, 255}
};

ThemeColors darkTheme = {
        {40, 40, 40, 255},
        {220, 220, 220, 255},
        {60, 60, 60, 255},
        {60, 60, 60, 255},
        {80, 80, 80, 255},
        {220, 220, 220, 255},
        {50, 50, 50, 255}
};

ThemeColors currentTheme = lightTheme;

bool showPassives = false;
bool showSources = false;
bool showSemis = false;
bool showDependents = false;

Button darkModeBtn = {460, 5, 100, 30, "Dark Mode", currentTheme.button};
Button passiveBtn = {870, 50, 120, 40, "Passives", currentTheme.button};
Button sourcesBtn = {870, 100, 120, 40, "Sources", currentTheme.button};
Button semiBtn = {870, 150, 120, 40, "Semiconductors", currentTheme.button};
Button depBtn = {870, 200, 120, 40, "Dependent", currentTheme.button};

Button resBtn = {1000, 100, 120, 40, "Resistor", currentTheme.button};
Button capBtn = {1000, 150, 120, 40, "Capacitor", currentTheme.button};
Button indBtn = {1000, 200, 120, 40, "Inductor", currentTheme.button};
Button diodeBtn = {1000, 250, 120, 40, "Diode", currentTheme.button};
Button vSrcBtn = {1000, 100, 120, 40, "V Source", currentTheme.button};
Button iSrcBtn = {1000, 150, 120, 40, "I Source", currentTheme.button};
Button gndBtn = {1000, 200, 120, 40, "Ground", currentTheme.button};
Button vcvsBtn = {1000, 250, 120, 40, "VCVS", currentTheme.button};
Button vccsBtn = {1000, 300, 120, 40, "VCCS", currentTheme.button};
Button ccvsBtn = {1000, 350, 120, 40, "CCVS", currentTheme.button};
Button cccsBtn = {1000, 400, 120, 40, "CCCS", currentTheme.button};


enum ComponentType {
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
    CCVS_SOURCE,
};

enum PlacementMode {
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
    PLACE_CCVS
};

void drawWire(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, SDL_Color color) {
    if (x1 != x2 || y1 != y2) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
    }
}

PlacementMode currentPlacementMode = PLACE_NONE;
SDL_Point placementStartPoint = {0, 0};
bool isPlacingComponent = false;
bool isPlacingWire = false;
vector<SDL_Point> wirePoints;

unordered_map<string, int> nodeMap;
unordered_map<int, string> reverseNodeMap;
int nextNodeNumber = 1;

int getOrCreateNode(const string& nodeName) {
    if (nodeName == "GND" || nodeName == "0") return 0;

    if (nodeMap.find(nodeName) == nodeMap.end()) {
        int newNodeNum = nextNodeNumber++;
        nodeMap[nodeName] = newNodeNum;
        reverseNodeMap[newNodeNum] = nodeName;
        nodePositions[newNodeNum] = {100 + (newNodeNum * 100), 100};
        return newNodeNum;
    }
    return nodeMap[nodeName];
}

string getNodeName(int nodeNumber) {
    if (nodeNumber == 0) return "GND";
    return reverseNodeMap[nodeNumber];
}

double parseSpiceValue(const string& valStr) {
    if (valStr.empty()) return 0.0;

    bool isNumber = true;
    bool hasDot = false;
    bool hasE = false;
    bool hasSign = false;

    for (size_t i = 0; i < valStr.size(); i++) {
        char c = valStr[i];
        if (c == '.' && !hasDot) {
            hasDot = true;
        } else if ((c == 'e' || c == 'E') && !hasE) {
            hasE = true;
            hasDot = false;
            hasSign = false;
        } else if ((c == '+' || c == '-') && (i == 0 || hasE)) {
            hasSign = true;
        } else if (!isdigit(c)) {
            isNumber = false;
            break;
        }
    }

    if (isNumber) {
        return stod(valStr);
    }

    size_t suffixPos = 0;
    while (suffixPos < valStr.size() &&
           (isdigit(valStr[suffixPos]) || valStr[suffixPos] == '.' ||
            valStr[suffixPos] == '-' || valStr[suffixPos] == '+' ||
            valStr[suffixPos] == 'e' || valStr[suffixPos] == 'E')) {
        suffixPos++;
    }

    if (suffixPos == 0) {
        return 0.0;
    }

    double num = stod(valStr.substr(0, suffixPos));
    string suffix = valStr.substr(suffixPos);

    transform(suffix.begin(), suffix.end(), suffix.begin(), ::tolower);

    if (suffix == "t") return num * 1e12;
    if (suffix == "g") return num * 1e9;
    if (suffix == "meg") return num * 1e6;
    if (suffix == "k") return num * 1e3;
    if (suffix == "m") return num * 1e-3;
    if (suffix == "u") return num * 1e-6;
    if (suffix == "n") return num * 1e-9;
    if (suffix == "p") return num * 1e-12;
    if (suffix == "f") return num * 1e-15;

    return num;
}

class Component {

protected:
    int posX, posY;
public:
    ComponentType type;
    string name;
    string nodeName1;
    string nodeName2;
    int node1, node2;
    double value;

    Component(ComponentType t, const string& n, int n1, int n2, double val)
            : type(t), name(n), node1(n1), node2(n2), value(val) {
        nodeName1 = getNodeName(n1);
        nodeName2 = getNodeName(n2);
    }

    virtual ~Component() {}

    virtual void stamp(vector<vector<double>>& G,
                       vector<vector<double>>& B,
                       vector<vector<double>>& C,
                       vector<vector<double>>& D,
                       vector<double>& J,
                       vector<double>& E,
                       int& nextVariable) = 0;

    virtual void update(double dt, const vector<double>& nodeVoltages) {}
    virtual double getCurrent(const vector<double>& nodeVoltages) const { return 0.0; }

    virtual string getInfo() const {
        string typeStr;
        switch(type) {
            case RESISTOR: typeStr = "Resistor"; break;
            case CAPACITOR: typeStr = "Capacitor"; break;
            case INDUCTOR: typeStr = "Inductor"; break;
            case VOLTAGE_SOURCE: typeStr = "Voltage Source"; break;
            case CURRENT_SOURCE: typeStr = "Current Source"; break;
            case DIODE: typeStr = "Diode"; break;
            case GROUND: typeStr = "Ground"; break;
            case SIN_VOLTAGE_SOURCE: typeStr = "Sinusoidal Voltage Source"; break;
            case PULSE_VOLTAGE_SOURCE: typeStr = "Pulse Voltage Source"; break;
            case VCVS_SOURCE: typeStr = "VCVS Source"; break;
            case VCCS_SOURCE: typeStr = "VCCS Source"; break;
            case CCVS_SOURCE: typeStr = "CCVS Source"; break;
            case CCCS_SOURCE: typeStr = "CCCS Source"; break;
        }
        return typeStr + " " + name + " " + getNodeName(node1) + " " + getNodeName(node2) + " " + to_string(value);
    }    virtual void setPosition(int x, int y) {
        posX = x;
        posY = y;
    }
    virtual pair<int,int> getPosition() {
        return {posX, posY};
    }

    virtual string getType() = 0;

    virtual void render(SDL_Renderer* renderer, const map<int, SDL_Point>& nodePositions) const = 0;
    virtual SDL_Rect getBoundingBox(const map<int, SDL_Point>& nodePositions) const = 0;
    virtual Component* clone() const = 0;

};

void renderText(const string& text, int x, int y,  SDL_Color color = currentTheme.text) {
    SDL_Surface* surface = TTF_RenderText_Solid(font, text.c_str(), color);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, nullptr, &rect);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void handleCursorInteraction(SDL_Event& event, const SDL_Rect& plotArea,
                             const vector<double>& times, const vector<double>& values,
                             double minTime, double maxTime, double minValue, double maxValue) {
    int x, y;
    SDL_GetMouseState(&x, &y);

    if (event.type == SDL_MOUSEBUTTONDOWN) {
        if (event.button.button == SDL_BUTTON_LEFT) {
            if (x >= plotArea.x && x <= plotArea.x + plotArea.w &&
                y >= plotArea.y && y <= plotArea.y + plotArea.h) {

                // Find if clicking near existing cursor
                activeCursorIndex = -1;
                for (size_t i = 0; i < cursors.size(); i++) {
                    if (abs(x - cursors[i].x) < 10 && abs(y - cursors[i].y) < 10) {
                        activeCursorIndex = i;
                        cursors[i].dragging = true;
                        break;
                    }
                }

                // If not near existing cursor, create new one with right click
            }
        }
        else if (event.button.button == SDL_BUTTON_RIGHT) {
            if (x >= plotArea.x && x <= plotArea.x + plotArea.w &&
                y >= plotArea.y && y <= plotArea.y + plotArea.h) {

                // Create new cursor on right click
                Cursor newCursor;
                newCursor.x = x;
                newCursor.y = y;
                newCursor.dragging = true;
                newCursor.active = true;

                // Calculate cursor values
                double timeRange = maxTime - minTime;
                double valueRange = maxValue - minValue;
                newCursor.time = minTime + (newCursor.x - plotArea.x) * timeRange / plotArea.w;
                newCursor.value = maxValue - (newCursor.y - plotArea.y) * valueRange / plotArea.h;

                // Assign different colors to cursors
                static const vector<SDL_Color> cursorColors = {
                        {204, 153, 0, 128},    // Yellow
                        {0, 153, 204, 128},    // Blue
                        {204, 0, 153, 128},    // Magenta
                        {0, 204, 153, 128},    // Teal
                        {153, 0, 204, 128}     // Purple
                };
                newCursor.color = cursorColors[cursors.size() % cursorColors.size()];

                cursors.push_back(newCursor);
                activeCursorIndex = cursors.size() - 1;
            }
        }
    }
    else if (event.type == SDL_MOUSEBUTTONUP) {
        if (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT) {
            for (auto& cursor : cursors) {
                cursor.dragging = false;
            }
            activeCursorIndex = -1;
        }
    }
    else if (event.type == SDL_MOUSEMOTION) {
        if (activeCursorIndex != -1 && cursors[activeCursorIndex].dragging) {
            cursors[activeCursorIndex].x = x;
            cursors[activeCursorIndex].y = y;

            // Constrain cursor to plot area
            if (cursors[activeCursorIndex].x < plotArea.x) cursors[activeCursorIndex].x = plotArea.x;
            if (cursors[activeCursorIndex].x > plotArea.x + plotArea.w) cursors[activeCursorIndex].x = plotArea.x + plotArea.w;
            if (cursors[activeCursorIndex].y < plotArea.y) cursors[activeCursorIndex].y = plotArea.y;
            if (cursors[activeCursorIndex].y > plotArea.y + plotArea.h) cursors[activeCursorIndex].y = plotArea.y + plotArea.h;

            // Calculate cursor values
            double timeRange = maxTime - minTime;
            double valueRange = maxValue - minValue;
            cursors[activeCursorIndex].time = minTime + (cursors[activeCursorIndex].x - plotArea.x) * timeRange / plotArea.w;
            cursors[activeCursorIndex].value = maxValue - (cursors[activeCursorIndex].y - plotArea.y) * valueRange / plotArea.h;
        }
    }
    else if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_k) {
            // Toggle all cursors
            bool anyActive = false;
            for (auto& cursor : cursors) {
                if (cursor.active) anyActive = true;
            }
            for (auto& cursor : cursors) {
                cursor.active = !anyActive;
            }
        }
        else if (event.key.keysym.sym == SDLK_ESCAPE) {
            // Clear all cursors
            for (auto& cursor : cursors) {
                cursor.active = false;
                cursor.dragging = false;
            }
            activeCursorIndex = -1;
        }
        else if (event.key.keysym.sym == SDLK_DELETE && activeCursorIndex != -1) {
            // Delete active cursor
            if (cursors.size() > 1) {
                cursors.erase(cursors.begin() + activeCursorIndex);
                activeCursorIndex = -1;
            } else {
                cursors[0].active = false;
            }
        }
    }
}
class Circuit {
public:
    vector<Component*> components;
    int maxNode;
    static double currentTimeStep;
    static double currentTime;
    vector<pair<int, int>> wireConnections;
    map<int, SDL_Point> nodePositions;
    unordered_map<string, int> nodeMap;
    unordered_map<int, string> reverseNodeMap;
    int nextNodeNumber = 1;

    Circuit(const Circuit& other) {
        for (const auto& comp : other.components) {
            components.push_back(comp->clone());
        }

        nodeMap = other.nodeMap;
        reverseNodeMap = other.reverseNodeMap;
        nextNodeNumber = other.nextNodeNumber;
        maxNode = other.maxNode;

        wireConnections = other.wireConnections;

        nodePositions = other.nodePositions;
    }

    Circuit() : maxNode(0){}

    Circuit* clone() const {
        return new Circuit(*this);
    }

    Circuit& operator=(const Circuit& other) {
        if (this != &other) {
            for (auto comp : components) {
                delete comp;
            }
            components.clear();

            for (const auto& comp : other.components) {
                components.push_back(comp->clone());
            }

            nodeMap = other.nodeMap;
            reverseNodeMap = other.reverseNodeMap;
            nextNodeNumber = other.nextNodeNumber;
            maxNode = other.maxNode;
            wireConnections = other.wireConnections;
            nodePositions = other.nodePositions;
        }
        return *this;
    }

    ~Circuit() {
        for (auto comp : components) {
            delete comp;
        }
    }

    void updateNodePositions() {
        int x = 100;
        int y = 100;
        int groundY = 500;

        nodePositions.clear();

        nodePositions[0] = {100, groundY};

        for (int i = 1; i < nextNodeNumber; i++) {
            nodePositions[i] = {x, y};
            x += 100;
            if (x > 700) {
                x = 100;
                y += 100;
            }
        }
    }

    int getOrCreateNode(const string& name) {
        if (name == "GND" || name == "0") return 0;

        auto it = nodeMap.find(name);
        if (it != nodeMap.end()) {
            return it->second;
        }

        int nodeNum = nextNodeNumber++;
        nodeMap[name] = nodeNum;
        reverseNodeMap[nodeNum] = name;
        return nodeNum;
    }

    string getNodeName(int nodeNum) const {
        if (nodeNum == 0) return "GND";
        auto it = reverseNodeMap.find(nodeNum);
        if (it != reverseNodeMap.end()) {
            return it->second;
        }
        return "UNKNOWN";
    }

    void updateNodeConnections() {
        unordered_set<int> connectedNodes;
        connectedNodes.insert(0);

        for (const auto& comp : components) {
            connectedNodes.insert(comp->node1);
            connectedNodes.insert(comp->node2);
        }

        unordered_map<string, int> newNodeMap;
        unordered_map<int, string> newReverseNodeMap;
        int newNextNode = 1;

        for (const auto& node : nodeMap) {
            if (connectedNodes.count(node.second)) {
                newNodeMap[node.first] = (node.second == 0) ? 0 : newNextNode++;
                newReverseNodeMap[newNodeMap[node.first]] = node.first;
            }
        }

        for (auto& comp : components) {
            comp->node1 = newNodeMap[reverseNodeMap[comp->node1]];
            comp->node2 = newNodeMap[reverseNodeMap[comp->node2]];
        }

        nodeMap = newNodeMap;
        reverseNodeMap = newReverseNodeMap;
        nextNodeNumber = newNextNode;
    }

    const unordered_map<string, int>& getNodeMap() const {
        return nodeMap;
    }

    const unordered_map<int, string>& getReverseNodeMap() const {
        return reverseNodeMap;
    }

    bool hasComponent(const string& name) const {
        for (const auto& comp : components) {
            if (comp->name == name) {
                return true;
            }
        }
        return false;
    }

    bool hasGround() const {
        for (const auto& comp : components) {
            if (comp->type == GROUND) {
                return true;
            }
        }
        return false;
    }

    const vector<Component*>& getComponents() const {
        return components;
    }

    vector<string> listComponents(ComponentType filterType = static_cast<ComponentType>(-1)) const {
        vector<string> result;
        for (const auto& comp : components) {
            if (filterType == static_cast<ComponentType>(-1) || comp->type == filterType) {
                result.push_back(comp->getInfo());
            }
        }
        return result;
    }

    void addComponent(Component* comp) {
        if (hasComponent(comp->name)) {
            throw runtime_error("Error: Component " + comp->name + " already exists in the circuit");
        }

        comp->node1 = getOrCreateNode(comp->nodeName1);
        comp->node2 = getOrCreateNode(comp->nodeName2);

        if ((comp->type == RESISTOR || comp->type == CAPACITOR || comp->type == INDUCTOR) &&
            comp->value <= 0) {
            string typeStr;
            if (comp->type == RESISTOR) typeStr = "Resistor";
            else if (comp->type == CAPACITOR) typeStr = "Capacitor";
            else typeStr = "Inductor";
            throw runtime_error("Error: " + typeStr + " value must be positive");
        }

        components.push_back(comp);
        if (comp->node1 > maxNode) maxNode = comp->node1;
        if (comp->node2 > maxNode) maxNode = comp->node2;

        if (comp->type == CAPACITOR || comp->type == INDUCTOR) {
            hasDynamic = true;
        }
    }

    bool deleteComponent(const string& name) {
        for (auto it = components.begin(); it != components.end(); ++it) {
            if ((*it)->name == name) {
                delete *it;
                components.erase(it);

                updateNodeConnections();
                return true;
            }
        }
        return false;
    }

    void calculateNodePositions();

    void addWire(int node1, int node2) {
        wireConnections.emplace_back(node1, node2);
    }

    const vector<pair<int, int>>& getWires() const {
        return wireConnections;
    }

    bool renameNode(const string& oldName, const string& newName) {
        if (oldName == "GND" || oldName == "0" || newName == "GND" || newName == "0") {
            return false;
        }

        if (nodeMap.find(newName) != nodeMap.end()) {
            return false;
        }

        auto it = nodeMap.find(oldName);
        if (it == nodeMap.end()) {
            return false;
        }

        int nodeNum = it->second;
        nodeMap.erase(it);
        nodeMap[newName] = nodeNum;
        reverseNodeMap[nodeNum] = newName;
        return true;
    }

    bool renameComponent(const string& oldName, const string& newName) {
        if (oldName == newName) {
            return false;
        }

        if (hasComponent(newName)) {
            return false;
        }

        for (auto& comp : components) {
            if (comp->name == oldName) {
                comp->name = newName;
                return true;
            }
        }

        return false;
    }

    vector<string> listNodes() const {
        vector<string> nodes;
        nodes.push_back("GND (0)");
        for (int i = 1; i < nextNodeNumber; i++) {
            nodes.push_back(reverseNodeMap.at(i) + " (" + to_string(i) + ")");
        }
        return nodes;
    }

    static vector<double> solveSystem(vector<vector<double>>& A, vector<double>& b) {
        int n = A.size();

        for (int i = 0; i < n; i++) {
            int maxRow = i;
            for (int k = i + 1; k < n; k++) {
                if (abs(A[k][i]) > abs(A[maxRow][i])) {
                    maxRow = k;
                }
            }

            if (maxRow != i) {
                swap(A[i], A[maxRow]);
                swap(b[i], b[maxRow]);
            }

            if (abs(A[i][i]) < 1e-12) {
                throw runtime_error("Matrix is singular or nearly singular");
            }

            for (int k = i + 1; k < n; k++) {
                double factor = A[k][i] / A[i][i];
                for (int j = i; j < n; j++) {
                    A[k][j] -= factor * A[i][j];
                }
                b[k] -= factor * b[i];
            }
        }

        vector<double> x(n);
        for (int i = n - 1; i >= 0; i--) {
            x[i] = b[i];
            for (int j = i + 1; j < n; j++) {
                x[i] -= A[i][j] * x[j];
            }
            x[i] /= A[i][i];
        }

        return x;
    }

    void analyzeDC() {
        if (!hasGround()) {
            throw runtime_error("Error: No ground node detected in the circuit");
        }

        int numNodes = maxNode;
        int numVSources = 0;
        int numInductors = 0;

        for (auto comp : components) {
            if (comp->type == VOLTAGE_SOURCE || comp->type == SIN_VOLTAGE_SOURCE ||
                comp->type == PULSE_VOLTAGE_SOURCE) numVSources++;
            if (comp->type == INDUCTOR) numInductors++;
        }

        int numVars = numNodes + numVSources + numInductors;
        if (numVars == 0) {
            cout << "No variables to solve for in DC analysis" << endl;
            return;
        }

        vector<vector<double>> G(numNodes, vector<double>(numNodes, 0.0));
        vector<vector<double>> B(numNodes, vector<double>(numVSources + numInductors, 0.0));
        vector<vector<double>> C(numVSources + numInductors, vector<double>(numNodes, 0.0));
        vector<vector<double>> D(numVSources + numInductors, vector<double>(numVSources + numInductors, 0.0));
        vector<double> J(numNodes, 0.0);
        vector<double> E(numVSources + numInductors, 0.0);

        for (int i = 0; i < numNodes; i++) {
            G[i][i] = 1e-12;
        }

        int vsCount = 0;
        for (auto comp : components) {
            if (comp->type == INDUCTOR) {
                double conductance = 1.0 / 1e-12;
                if (comp->node1 != 0) {
                    G[comp->node1-1][comp->node1-1] += conductance;
                    if (comp->node2 != 0) {
                        G[comp->node1-1][comp->node2-1] -= conductance;
                        G[comp->node2-1][comp->node1-1] -= conductance;
                    }
                }
                if (comp->node2 != 0) {
                    G[comp->node2-1][comp->node2-1] += conductance;
                }
            } else {
                comp->stamp(G, B, C, D, J, E, vsCount);
            }
        }

        vector<vector<double>> A(numVars, vector<double>(numVars, 0.0));
        vector<double> b(numVars, 0.0);

        for (int i = 0; i < numNodes; i++) {
            for (int j = 0; j < numNodes; j++) A[i][j] = G[i][j];
            for (int j = 0; j < numVSources + numInductors; j++) A[i][numNodes + j] = B[i][j];
            b[i] = J[i];
        }

        for (int i = 0; i < numVSources + numInductors; i++) {
            for (int j = 0; j < numNodes; j++) A[numNodes + i][j] = C[i][j];
            for (int j = 0; j < numVSources + numInductors; j++) A[numNodes + i][numNodes + j] = D[i][j];
            b[numNodes + i] = E[i];
        }

        try {
            vector<double> x = solveSystem(A, b);

            cout << "\nDC Analysis Results:\n";
            cout << "-------------------\n";
            cout << "Node Voltages:\n";
            cout << "  Node GND: 0.000000 V\n";
            for (int i = 0; i < numNodes; i++) {
                cout << "  Node " << getNodeName(i+1) << ": " << fixed << setprecision(6) << x[i] << " V\n";
            }

            if (numVSources > 0) {
                cout << "\nCurrents through Voltage Sources:\n";
                for (int i = 0; i < numVSources; i++) {
                    cout << "  Source " << (i+1) << ": " << fixed << setprecision(6) << x[numNodes + i] << " A\n";
                }
            }

            cout << "\nCurrents through Resistors:\n";
            for (auto comp : components) {
                if (comp->type == RESISTOR) {
                    double current = comp->getCurrent(x);
                    cout << "  " << comp->name << ": " << fixed << setprecision(6) << current << " A\n";
                }
            }

        } catch (const runtime_error& e) {
            cerr << "Error in DC analysis: " << e.what() << endl;
            cerr << "Matrix A:\n";
            for (int i = 0; i < numVars; i++) {
                for (int j = 0; j < numVars; j++) {
                    cerr << A[i][j] << " ";
                }
                cerr << " | " << b[i] << endl;
            }
        }
    }

    void analyzeTransient(double tStep, double tStop) {
        currentTimeStep = tStep;
        double t = 0.0;

        voltages.clear();
        currents.clear();
        Vtimes.clear();
        Itimes.clear();

        if (!hasGround()) {
            throw runtime_error("Error: No ground node detected in the circuit");
        }

        if (maxNode < 1) {
            throw runtime_error("Error: Circuit must have at least one non-ground node");
        }

        int numNodes = maxNode;
        int numVSources = 0;
        int numInductors = 0;

        for (auto comp : components) {
            if (comp->type == VOLTAGE_SOURCE || comp->type == SIN_VOLTAGE_SOURCE ||
                comp->type == PULSE_VOLTAGE_SOURCE) numVSources++;
            if (comp->type == INDUCTOR) numInductors++;
        }

        int numVars = numNodes + numVSources + numInductors;

        while (t <= tStop) {
            vector<vector<double>> G(numNodes, vector<double>(numNodes, 0.0));
            vector<vector<double>> B(numNodes, vector<double>(numVSources + numInductors, 0.0));
            vector<vector<double>> C(numVSources + numInductors, vector<double>(numNodes, 0.0));
            vector<vector<double>> D(numVSources + numInductors, vector<double>(numVSources + numInductors, 0.0));
            vector<double> J(numNodes, 0.0);
            vector<double> E(numVSources + numInductors, 0.0);

            for (int i = 0; i < numNodes; i++) {
                G[i][i] = 1e-12;
            }

            int vsCount = 0;
            currentTime = t;
            for (auto comp : components) {
                comp->stamp(G, B, C, D, J, E, vsCount);
            }

            vector<double> x;
            try {
                vector<vector<double>> A(numVars, vector<double>(numVars, 0.0));
                vector<double> b(numVars, 0.0);

                for (int i = 0; i < numNodes; i++) {
                    for (int j = 0; j < numNodes; j++) A[i][j] = G[i][j];
                    for (int j = 0; j < numVSources + numInductors; j++) A[i][numNodes + j] = B[i][j];
                    b[i] = J[i];
                }
                for (int i = 0; i < numVSources + numInductors; i++) {
                    for (int j = 0; j < numNodes; j++) A[numNodes + i][j] = C[i][j];
                    for (int j = 0; j < numVSources + numInductors; j++) A[numNodes + i][numNodes + j] = D[i][j];
                    b[numNodes + i] = E[i];
                }

                x = solveSystem(A, b);

                for (int i = 0; i < numNodes; i++) {
                    Vtimes.push_back(t);
                    voltages.push_back(x[i]);
                }

                for (auto comp : components) {
                    if (comp->type == RESISTOR || comp->type == CAPACITOR ||
                        comp->type == INDUCTOR || comp->type == DIODE) {
                        Itimes.push_back(t);
                        currents.push_back(comp->getCurrent(x));
                        }
                }

            } catch (const runtime_error& e) {
                cerr << "Error at t=" << t << ": " << e.what() << endl;
                break;
            }

            for (auto comp : components) {
                comp->update(tStep, x);
            }

            t += tStep;
        }
    }

    void drawLTspiceStylePlot(SDL_Renderer* renderer, const SDL_Rect& area) {
        if (voltages.empty() && currents.empty()) return;

        SDL_SetRenderDrawColor(renderer, currentTheme.plotBg.r, currentTheme.plotBg.g, currentTheme.plotBg.b, 255);
        SDL_RenderFillRect(renderer, &area);
        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 255);
        SDL_RenderDrawRect(renderer, &area);

        double minTime = numeric_limits<double>::max();
        double maxTime = numeric_limits<double>::lowest();
        double minValue = numeric_limits<double>::max();
        double maxValue = numeric_limits<double>::lowest();

        for (int i = 0; i < voltages.size(); i++) {
            if (!Vtimes.empty()) {
                minTime = min(minTime, Vtimes.front());
                maxTime = max(maxTime, Vtimes.back());

                if (!voltages.empty()) {
                    auto Iminmax = minmax_element(currents.begin(), currents.end());

                    if (Iminmax.first != currents.end()) minValue = min(minValue, *Iminmax.first);
                    if (Iminmax.second != currents.end()) maxValue = max(maxValue, *Iminmax.second);

                    auto Vminmax = minmax_element(voltages.begin(), voltages.end());

                    if (Vminmax.first != voltages.end()) minValue = min(minValue, *Vminmax.first);
                    if (Vminmax.second != voltages.end()) maxValue = max(maxValue, *Vminmax.second);

                }
            }
        }

        if (minTime == numeric_limits<double>::max()) minTime = 0;
        if (maxTime == numeric_limits<double>::lowest()) maxTime = 1;
        if (minValue == numeric_limits<double>::max()) minValue = 0;
        if (maxValue == numeric_limits<double>::lowest()) maxValue = 1;

        double valueRange = maxValue - minValue;
        if (valueRange < 1e-12) valueRange = 1;
        minValue -= valueRange * 0.1;
        maxValue += valueRange * 0.1;

        double timeRange = maxTime - minTime;
        if (timeRange <= 0) timeRange = 1;

        timeRange /= timeZoom;
        valueRange = (maxValue - minValue) / valueZoom;
        double centerTime = (minTime + maxTime) / 2 + timePan;
        double centerValue = (minValue + maxValue) / 2 + valuePan;
        minTime = centerTime - timeRange/2;
        maxTime = centerTime + timeRange/2;
        minValue = centerValue - valueRange/2;
        maxValue = centerValue + valueRange/2;

        if (showGrid) {
            SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 50);

            double timeStep = pow(10, floor(log10(timeRange)));
            if (timeStep <= 0) timeStep = 1;

            for (double t = ceil(minTime/timeStep)*timeStep; t <= maxTime; t += timeStep) {
                int x = area.x + static_cast<int>((t - minTime) / timeRange * area.w);
                SDL_RenderDrawLine(renderer, x, area.y, x, area.y + area.h);
                renderText(to_string(t).substr(0, 6), x + 2, area.y + area.h - 20, currentTheme.text);
            }

            double valueStep = pow(10, floor(log10(valueRange)));
            if (valueStep <= 0) valueStep = 1;

            for (double v = ceil(minValue/valueStep)*valueStep; v <= maxValue; v += valueStep) {
                int y = area.y + area.h - static_cast<int>((v - minValue) / valueRange * area.h);
                SDL_RenderDrawLine(renderer, area.x, y, area.x + area.w, y);
                renderText(to_string(v).substr(0, 6), area.x + 2, y - 20, currentTheme.text);
            }
        }

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 255);
        SDL_RenderDrawLine(renderer, area.x, area.y + area.h, area.x + area.w, area.y + area.h);
        SDL_RenderDrawLine(renderer, area.x, area.y, area.x, area.y + area.h);

        SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255);

        for (size_t i = 1; i <= Vtimes.size(); i++) {
            int x1 = area.x + static_cast<int>((Vtimes[i-1] - minTime) / timeRange * area.w);
            int y1 = area.y + area.h - static_cast<int>((voltages[i-1] - minValue) / valueRange * area.h);

            for (int x = -3; x <= 3; x++) {
                for (int y = -3; y <= 3; y++) {
                    if (x*x + y*y <= 3 * 3) {
                        SDL_RenderDrawPoint(renderer, x1 + x, y1 + y);
                    }
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);

        for (size_t i = 1; i <= Itimes.size(); i++) {
            int x1 = area.x + static_cast<int>((Itimes[i-1] - minTime) / timeRange * area.w);
            int y1 = area.y + area.h - static_cast<int>((currents[i-1] - minValue) / valueRange * area.h);

            for (int x = -3; x <= 3; x++) {
                for (int y = -3; y <= 3; y++) {
                    if (x*x + y*y <= 3 * 3) {
                        SDL_RenderDrawPoint(renderer, x1 + x, y1 + y);
                    }
                }
            }
        }

        if (showLegend) {
            int legendX = area.x + 10;
            int legendY = area.y + 10;

            for (size_t i = 0; i < components.size(); i++) {
                const auto& sig = components[i];

                SDL_Rect colorRect = {legendX, legendY, 15, 15};
                SDL_SetRenderDrawColor(renderer, 120, 0, 0, 255);
                SDL_RenderFillRect(renderer, &colorRect);

                renderText(sig->name, legendX + 20, legendY, BLACK);

                legendY += 20;
            }
        }




            // Add cursor drawing logic
        // In your drawLTspiceStylePlot function, replace the single cursor drawing code:
        for (const auto& cursor : cursors) {
            if (cursor.active && cursor.x >= area.x && cursor.x <= area.x + area.w &&
                cursor.y >= area.y && cursor.y <= area.y + area.h) {

                // Draw vertical cursor line
                SDL_SetRenderDrawColor(renderer, cursor.color.r, cursor.color.g, cursor.color.b, cursor.color.a);
                SDL_RenderDrawLine(renderer, cursor.x, area.y, cursor.x, area.y + area.h);

                // Draw horizontal cursor line
                SDL_RenderDrawLine(renderer, area.x, cursor.y, area.x + area.w, cursor.y);

                // Draw cursor information box
                SDL_Rect infoBox = {cursor.x + 10, cursor.y - 80, 180, 70};
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
                SDL_RenderFillRect(renderer, &infoBox);
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderDrawRect(renderer, &infoBox);

                string timeStr = "Time: " + to_string(cursor.time).substr(0, 8) + "s";
                string valueStr = "Value: " + to_string(cursor.value).substr(0, 8);

                renderText(timeStr, infoBox.x + 5, infoBox.y + 5, WHITE);
                renderText(valueStr, infoBox.x + 5, infoBox.y + 25, WHITE);

                // Draw cursor position indicators on axes
                SDL_SetRenderDrawColor(renderer, cursor.color.r, cursor.color.g, cursor.color.b, 255);

                // Time axis indicator
                SDL_Rect timeIndicator = {cursor.x, area.y + area.h - 5, 1, 10};
                SDL_RenderFillRect(renderer, &timeIndicator);

                // Add this after drawing all cursors to show differences

                // Value axis indicator
                SDL_Rect valueIndicator = {area.x - 10, cursor.y, 10, 1};
                SDL_RenderFillRect(renderer, &valueIndicator);
            }
        }
        if (cursors.size() >= 2 && cursors[0].active && cursors[1].active) {
            double timeDelta = abs(cursors[1].time - cursors[0].time);
            double valueDelta = abs(cursors[1].value - cursors[0].value);

            SDL_Rect deltaBox = {area.x + 10, area.y + 10, 200, 60};
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
            SDL_RenderFillRect(renderer, &deltaBox);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer, &deltaBox);

            string deltaTimeStr = "ΔTime: " + to_string(timeDelta).substr(0, 8) + "s";
            string deltaValueStr = "ΔValue: " + to_string(valueDelta).substr(0, 8);

            renderText(deltaTimeStr, deltaBox.x + 5, deltaBox.y + 5, WHITE);
            renderText(deltaValueStr, deltaBox.x + 5, deltaBox.y + 25, WHITE);
        }
    }


    void printTransientResults(const vector<double>& times, const vector<double>& voltages,
                               const vector<double>& currents, int numNodes) {
        cout << "\nTransient Analysis Results:\n";
        cout << "--------------------------\n";

        cout << "Time (s)\t";
        cout << "Node GND (V)\t";
        for (int i = 0; i < numNodes; i++) {
            cout << "Node " << getNodeName(i+1) << " (V)\t";
        }

        cout << "Component Currents (A)\n";

        size_t numTimeSteps = times.size() / numNodes;
        size_t currentIndex = 0;

        for (size_t step = 0; step < numTimeSteps; step++) {
            cout << fixed << setprecision(6) << times[step * numNodes] << "\t";

            cout << "0.000000\t";
            for (int node = 0; node < numNodes; node++) {
                cout << voltages[step * numNodes + node] << "\t";
            }

            for (size_t compIdx = 0; compIdx < components.size(); compIdx++) {
                auto comp = components[compIdx];
                if (comp->type == RESISTOR || comp->type == CAPACITOR ||
                    comp->type == INDUCTOR || comp->type == DIODE) {
                    if (currentIndex < currents.size()) {
                        cout << comp->name << ": " << currents[currentIndex] << "\t";
                        currentIndex++;
                    }
                }
            }
            cout << "\n";
        }
    }

    void analyzeDCSweep(const string& sourceName, double start, double stop, double step) {
        if (!hasGround()) {
            throw runtime_error("Error: No ground node in the circuit");
        }

        Component* targetSource = nullptr;
        for (auto comp : components) {
            if ((comp->type == VOLTAGE_SOURCE || comp->type == CURRENT_SOURCE) && comp->name == sourceName) {
                targetSource = comp;
                break;
            }
        }

        if (!targetSource) {
            throw runtime_error("Error: Source " + sourceName + " not found or not sweepable");
        }

        vector<double> sweepPoints;
        vector<vector<double>> results;

        double value = start;
        while ((step > 0 && value <= stop) || (step < 0 && value >= stop)) {
            targetSource->value = value;
            sweepPoints.push_back(value);

            int numNodes = maxNode;
            int numVSources = 0;
            int numInductors = 0;

            for (auto comp : components) {
                if (comp->type == VOLTAGE_SOURCE || comp->type == SIN_VOLTAGE_SOURCE ||
                    comp->type == PULSE_VOLTAGE_SOURCE) numVSources++;
                if (comp->type == INDUCTOR) numInductors++;
            }

            int numVars = numNodes + numVSources + numInductors;
            if (numVars == 0) {
                throw runtime_error("Error: No variables to solve in sweep analysis");
            }

            vector<vector<double>> G(numNodes, vector<double>(numNodes, 0.0));
            vector<vector<double>> B(numNodes, vector<double>(numVSources + numInductors, 0.0));
            vector<vector<double>> C(numVSources + numInductors, vector<double>(numNodes, 0.0));
            vector<vector<double>> D(numVSources + numInductors, vector<double>(numVSources + numInductors, 0.0));
            vector<double> J(numNodes, 0.0);
            vector<double> E(numVSources + numInductors, 0.0);

            for (int i = 0; i < numNodes; i++) {
                G[i][i] = 1e-12;
            }

            int vsCount = 0;
            for (auto comp : components) {
                if (comp->type == INDUCTOR) {
                    double conductance = 1.0 / 1e-12;
                    if (comp->node1 != 0) {
                        G[comp->node1 - 1][comp->node1 - 1] += conductance;
                        if (comp->node2 != 0) {
                            G[comp->node1 - 1][comp->node2 - 1] -= conductance;
                            G[comp->node2 - 1][comp->node1 - 1] -= conductance;
                        }
                    }
                    if (comp->node2 != 0) {
                        G[comp->node2 - 1][comp->node2 - 1] += conductance;
                    }
                } else {
                    comp->stamp(G, B, C, D, J, E, vsCount);
                }
            }

            vector<vector<double>> A(numVars, vector<double>(numVars, 0.0));
            vector<double> b(numVars, 0.0);

            for (int i = 0; i < numNodes; i++) {
                for (int j = 0; j < numNodes; j++) A[i][j] = G[i][j];
                for (int j = 0; j < numVSources + numInductors; j++) A[i][numNodes + j] = B[i][j];
                b[i] = J[i];
            }

            for (int i = 0; i < numVSources + numInductors; i++) {
                for (int j = 0; j < numNodes; j++) A[numNodes + i][j] = C[i][j];
                for (int j = 0; j < numVSources + numInductors; j++) A[numNodes + i][numNodes + j] = D[i][j];
                b[numNodes + i] = E[i];
            }

            try {
                vector<double> x = solveSystem(A, b);
                results.push_back(x);
            } catch (const runtime_error& e) {
                cerr << "Sweep value " << value << ": " << e.what() << endl;
                results.push_back(vector<double>(numVars, NAN));
            }

            value += step;
        }

        cout << "\nDC Sweep Analysis Results for " << sourceName << ":\n";
        cout << "-------------------------------------\n";
        cout << setw(15) << "Source (V/A)";
        for (int i = 0; i < maxNode; ++i) {
            cout << setw(15) << "V(" + getNodeName(i + 1) + ")";
        }
        cout << "\n";

        for (size_t i = 0; i < sweepPoints.size(); ++i) {
            cout << setw(15) << fixed << setprecision(6) << sweepPoints[i];
            for (int j = 0; j < maxNode; ++j) {
                double v = results[i][j];
                if (isnan(v)) {
                    cout << setw(15) << "NaN";
                } else {
                    cout << setw(15) << fixed << setprecision(6) << v;
                }
            }
            cout << "\n";
        }
    }

    template <class Archive>
    void save(Archive& archive) const;

    template <class Archive>
    void load (Archive& archive);

    template <class Archive>
    void serialize(Archive& archive) {
        archive(
            components,
            maxNode,
            wireConnections,
            nodePositions,
            nodeMap,
            reverseNodeMap,
            nextNodeNumber
        );
    }

    void saveToFile(const string& filename) const;

    bool loadFromFile(const string& filename);
};

void Circuit::calculateNodePositions(){
    nodePositions.clear();

    nodePositions[0] = {circuitArea.x + circuitArea.w/2, circuitArea.y + circuitArea.h - 50};

    int x = circuitArea.x + 100;
    int y = circuitArea.y + 100;
    int nodesPerRow = 5;

    for (int i = 1; i < nextNodeNumber; i++) {
        nodePositions[i] = {x, y};
        x += 150;
        if (i % nodesPerRow == 0) {
            x = circuitArea.x + 100;
            y += 100;
        }
    }
}
double Circuit::currentTimeStep = 0.0;
double Circuit::currentTime = 0.0;
Component* selectedComponent = nullptr;

bool saveCircuitToFile(const Circuit& circuit, const string& filename) {
    string actualFilename = filename;
    if (actualFilename.find(".txt") == string::npos) {
        actualFilename += ".txt";
    }

    ofstream file(actualFilename);
    if (!file.is_open()) {
        cerr << "Error: Could not open file '" << actualFilename << "' for writing." << endl;
        return false;
    }

    auto component_infos = circuit.listComponents();
    for (const auto& info : component_infos) {
        file << info << endl;
    }

    file.close();

    ifstream check(actualFilename);
    if (!check.is_open()) {
        cerr << "Error: File verification failed - '" << actualFilename << "' not found after saving." << endl;
        return false;
    }
    check.close();

    char absolute_path[MAX_PATH];
#ifdef _WIN32
    if (_fullpath(absolute_path, actualFilename.c_str(), MAX_PATH) != NULL) {
        cout << "SUCCESS: Circuit saved to " << absolute_path << endl;
    } else {
        cout << "SUCCESS: Circuit saved to " << actualFilename << endl;
    }
#else
    if (realpath(actualFilename.c_str(), absolute_path) != NULL) {
        cout << "SUCCESS: Circuit saved to " << absolute_path << endl;
    } else {
        cout << "SUCCESS: Circuit saved to " << actualFilename << endl;
    }
#endif

    return true;
}

void renderButton(const Button& button) {
    SDL_SetRenderDrawColor(renderer, button.color.r, button.color.g, button.color.b, 255);
    SDL_RenderFillRect(renderer, &button.rect);
    SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 255);
    SDL_RenderDrawRect(renderer, &button.rect);
    renderText(button.text, button.rect.x + 5, button.rect.y + 5, currentTheme.buttonText);
}

void renderTextBox(const TextBox& box) {
    SDL_SetRenderDrawColor(renderer, currentTheme.circuitBg.r, currentTheme.circuitBg.g, currentTheme.circuitBg.b, 255);
    SDL_RenderFillRect(renderer, &box.rect);
    SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 255);
    SDL_RenderDrawRect(renderer, &box.rect);

    string displayText = box.text.empty() ? box.rect.w < 100 ? "..." : box.rect.w < 150 ? "Enter..." : "Enter value..." : box.text;
    renderText(displayText, box.rect.x + 5, box.rect.y + 5, currentTheme.text);
}

SDL_Point getNodePosition(int node) {
    auto it = nodePositions.find(node);
    if (it != nodePositions.end()) {
        return it->second;
    }
    return {0, 0};
}

class Resistor : public Component {
public:


    Resistor(const string& n, int n1, int n2, double val) : Component(RESISTOR, n, n1, n2, val) {}

    void stamp(vector<vector<double>>& G,
               vector<vector<double>>&,
               vector<vector<double>>&,
               vector<vector<double>>&,
               vector<double>&,
               vector<double>&,
               int&) override {
        double conductance = 1.0 / value;

        if (node1 != 0) {
            G[node1-1][node1-1] += conductance;
            if (node2 != 0) {
                G[node1-1][node2-1] -= conductance;
                G[node2-1][node1-1] -= conductance;
            }
        }
        if (node2 != 0) {
            G[node2-1][node2-1] += conductance;
        }
    }

    double getCurrent(const vector<double>& nodeVoltages) const override {
        if (value == 0) return 0.0;
        double v1 = (node1 == 0) ? 0 : nodeVoltages[node1-1];
        double v2 = (node2 == 0) ? 0 : nodeVoltages[node2-1];
        return (v1 - v2) / value;
    }
    string getType() override { return "Resistor"; }

    void render(SDL_Renderer* renderer, const map<int, SDL_Point>& nodePositions) const override {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);

        const int segments = 5;
        const int amplitude = 10;

        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        float length = sqrt(dx*dx + dy*dy);

        dx /= length;
        dy /= length;
        float px = -dy;
        float py = dx;

        SDL_Point points[segments + 1];
        for (int i = 0; i <= segments; i++) {
            float t = (float)i / segments;
            float x = p1.x + t * (p2.x - p1.x);
            float y = p1.y + t * (p2.y - p1.y);

            float offset = (i % 2) ? amplitude : -amplitude;
            points[i].x = x + px * offset;
            points[i].y = y + py * offset;
        }

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 255);
        SDL_RenderDrawLines(renderer, points, segments + 1);

        int midX = (p1.x + p2.x)/2 + px * amplitude*2;
        int midY = (p1.y + p2.y)/2 + py * amplitude*2;
        renderText(name, midX, midY, currentTheme.text);
    }

    SDL_Rect getBoundingBox(const map<int, SDL_Point>& nodePositions) const {
        SDL_Point p1 = getNodePosition(node1);
        SDL_Point p2 = getNodePosition(node2);

        SDL_Rect rect;
        rect.x = min(p1.x, p2.x) - 15;
        rect.y = min(p1.y, p2.y) - 15;
        rect.w = abs(p1.x - p2.x) + 30;
        rect.h = abs(p1.y - p2.y) + 30;
        return rect;
    }

    Component* clone() const override {
        return new Resistor(*this);
    }

    template <class Archive>
    void serialize(Archive& archive) {
        archive(
            type, name, nodeName1, nodeName2, node1, node2, value
        );
    }
};

class Capacitor : public Component {
    double prevVoltage;
    double current;
public:
    Capacitor(const string& n, int n1, int n2, double val)
            : Component(CAPACITOR, n, n1, n2, val), prevVoltage(0.0), current(0.0) {}

    void stamp(vector<vector<double>>& G,
               vector<vector<double>>&,
               vector<vector<double>>&,
               vector<vector<double>>&,
               vector<double>& J,
               vector<double>&,
               int&) override {
        double geq = value / Circuit::currentTimeStep;

        if (node1 != 0) {
            G[node1-1][node1-1] += geq;
            if (node2 != 0) {
                G[node1-1][node2-1] -= geq;
                G[node2-1][node1-1] -= geq;
            }
        }
        if (node2 != 0) {
            G[node2-1][node2-1] += geq;
        }

        double ieq = -geq * prevVoltage;
        if (node1 != 0) J[node1-1] -= ieq;
        if (node2 != 0) J[node2-1] += ieq;
    }

    void update(double dt, const vector<double>& nodeVoltages) override {
        double v1 = (node1 == 0) ? 0 : nodeVoltages[node1-1];
        double v2 = (node2 == 0) ? 0 : nodeVoltages[node2-1];
        double voltage = v1 - v2;
        current = value * (voltage - prevVoltage) / dt;
        prevVoltage = voltage;
    }

    double getCurrent(const vector<double>& nodeVoltages) const override {
        return current;
    }
    string getType() override { return "Capacitor"; }

    void render(SDL_Renderer* renderer, const map<int, SDL_Point>& nodePositions) const override {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);

        const int gap = 12;

        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        float length = sqrt(dx*dx + dy*dy);
        dx /= length;
        dy /= length;
        float px = -dy;
        float py = dx;

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 255);
        SDL_RenderDrawLine(renderer, p1.x, p1.y, p1.x + dx*(length/2 - gap), p1.y + dy*(length/2 - gap));
        SDL_RenderDrawLine(renderer, p1.x + dx*(length/2 + gap), p1.y + dy*(length/2 + gap), p2.x, p2.y);

        SDL_Point plate1[2] = {
                {int(p1.x + dx*(length/2 - gap) + px*gap), int(p1.y + dy*(length/2 - gap) + py*gap)},
                {int(p1.x + dx*(length/2 - gap) - px*gap), int(p1.y + dy*(length/2 - gap) - py*gap)}
        };
        SDL_Point plate2[2] = {
                {int(p1.x + dx*(length/2 + gap) + px*gap), int(p1.y + dy*(length/2 + gap) + py*gap)},
                {int(p1.x + dx*(length/2 + gap) - px*gap), int(p1.y + dy*(length/2 + gap) - py*gap)}
        };

        SDL_RenderDrawLines(renderer, plate1, 2);
        SDL_RenderDrawLines(renderer, plate2, 2);

        int midX = (p1.x + p2.x)/2 + px * gap*2;
        int midY = (p1.y + p2.y)/2 + py * gap*2;
        renderText(name, midX, midY, currentTheme.text);
    }

    SDL_Rect getBoundingBox(const map<int, SDL_Point>& nodePositions) const {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);

        SDL_Rect rect;
        rect.x = min(p1.x, p2.x) - 15;
        rect.y = min(p1.y, p2.y) - 15;
        rect.w = abs(p1.x - p2.x) + 30;
        rect.h = abs(p1.y - p2.y) + 30;

        return rect;
    }

    Component* clone() const override {
        return new Capacitor(*this);
    }

    template <class Archive>
    void serialize(Archive& archive) {
        archive(
            type, name, nodeName1, nodeName2, node1, node2, value
        );
    }
};

class Inductor : public Component {
    double current;
    int index;

public:
    Inductor(const string& n, int n1, int n2, double val)
            : Component(INDUCTOR, n, n1, n2, val), current(0.0), index(-1) {}

    void stamp(vector<vector<double>>& G,
               vector<vector<double>>& B,
               vector<vector<double>>& C,
               vector<vector<double>>& D,
               vector<double>& J,
               vector<double>& E,
               int& nextVariable) override {
        index = nextVariable++;

        double dt = Circuit::currentTimeStep;
        double L = value;

        double Leq = L / dt;
        double Ieq = current;

        if (node1 != 0) {
            B[node1 - 1][index] = 1;
            C[index][node1 - 1] = 1;
        }
        if (node2 != 0) {
            B[node2 - 1][index] = -1;
            C[index][node2 - 1] = -1;
        }

        D[index][index] = -Leq;
        E[index] = -Leq * Ieq;
    }

    void update(double dt, const vector<double>& nodeVoltages) override {
        double v1 = (node1 == 0) ? 0 : nodeVoltages[node1 - 1];
        double v2 = (node2 == 0) ? 0 : nodeVoltages[node2 - 1];
        double voltageAcross = v1 - v2;
        current += voltageAcross * dt / value;
    }

    double getCurrent(const vector<double>& nodeVoltages) const override {
        return current;
    }

    string getType() override { return "Inductor"; }

    void render(SDL_Renderer* renderer, const map<int, SDL_Point>& nodePositions) const override {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);

        const int loops = 3;
        const int radius = 8;

        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        float length = sqrt(dx*dx + dy*dy);
        dx /= length;
        dy /= length;
        float px = -dy;
        float py = dx;

        float segmentLength = length / (loops * 2);
        SDL_Point prevPoint = {p1.x, p1.y};

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 255);

        SDL_RenderDrawLine(renderer, p1.x, p1.y,
                           p1.x + dx*segmentLength, p1.y + dy*segmentLength);

        for (int i = 0; i < loops; i++) {
            float centerX = p1.x + dx * (i * 2 + 1) * segmentLength;
            float centerY = p1.y + dy * (i * 2 + 1) * segmentLength;

            for (int angle = -90; angle <= 90; angle += 5) {
                float rad = angle * M_PI / 180.0f;
                int x = centerX + px * radius * cos(rad);
                int y = centerY + py * radius * sin(rad);

                if (angle == -90) {
                    prevPoint = {x, y};
                } else {
                    SDL_RenderDrawLine(renderer, prevPoint.x, prevPoint.y, x, y);
                    prevPoint = {x, y};
                }
            }
        }

        SDL_RenderDrawLine(renderer, prevPoint.x, prevPoint.y, p2.x, p2.y);

        int midX = (p1.x + p2.x)/2 + px * radius*2;
        int midY = (p1.y + p2.y)/2 + py * radius*2;
        renderText(name, midX, midY, currentTheme.text);
    }

    SDL_Rect getBoundingBox(const map<int, SDL_Point>& nodePositions) const {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);

        SDL_Rect rect;
        rect.x = min(p1.x, p2.x) - 15;
        rect.y = min(p1.y, p2.y) - 15;
        rect.w = abs(p1.x - p2.x) + 30;
        rect.h = abs(p1.y - p2.y) + 30;

        return rect;
    }

    Component* clone() const override {
        return new Inductor(*this);
    }

    template <class Archive>
    void serialize(Archive& archive) {
        archive(
            type, name, nodeName1, nodeName2, node1, node2, value
        );
    }
};

class Diode : public Component {
    const double Is;
    const double Vt;
    const double n;
    double lastVoltage;
    double current;

public:
    Diode(const string& name, int node1, int node2)
            : Component(DIODE, name, node1, node2, 0.0),
              Is(1e-14), Vt(0.026), n(1.0),
              lastVoltage(0.7), current(0.0) {}

    void stamp(vector<vector<double>>& G,
               vector<vector<double>>& /*C*/,
               vector<vector<double>>& /*B*/,
               vector<vector<double>>& /*D*/,
               vector<double>& J,
               vector<double>& /*x*/,
               int& /*matrixSize*/) override {

        double vd = lastVoltage;

        double expArg = min(vd / (n * Vt), 40.0);
        double expVd = exp(expArg);

        double g = (Is / (n * Vt)) * expVd;

        double Ieq = Is * (expVd - 1) - g * vd;

        if (node1 != 0) {
            if (node2 != 0) {
                G[node1 - 1][node1 - 1] += g;
                G[node1 - 1][node2 - 1] -= g;
                G[node2 - 1][node1 - 1] -= g;
                G[node2 - 1][node2 - 1] += g;

                J[node1 - 1] -= Ieq;
                J[node2 - 1] += Ieq;
            } else {
                G[node1 - 1][node1 - 1] += g;
                J[node1 - 1] -= Ieq;
            }
        } else if (node2 != 0) {
            G[node2 - 1][node2 - 1] += g;
            J[node2 - 1] += Ieq;
        }
    }

    void update(double dt, const vector<double>& nodeVoltages) override {
        double v1 = (node1 == 0) ? 0.0 : nodeVoltages[node1 - 1];
        double v2 = (node2 == 0) ? 0.0 : nodeVoltages[node2 - 1];
        lastVoltage = v1 - v2;

        double expArg = min(lastVoltage / (n * Vt), 40.0);
        current = Is * (exp(expArg) - 1);
    }

    double getCurrent(const vector<double>& /*nodeVoltages*/) const override {
        return current;
    }

    string getInfo() const override {
        return "Diode " + name + " " + getNodeName(node1) + " " + getNodeName(node2);
    }

    string getType() override { return "Diode"; }

    void render(SDL_Renderer* renderer, const map<int, SDL_Point>& nodePositions) const override {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);

        const int triangleSize = 15;

        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        float length = sqrt(dx*dx + dy*dy);
        dx /= length;
        dy /= length;
        float px = -dy;
        float py = dx;

        int midX = (p1.x + p2.x) / 2;
        int midY = (p1.y + p2.y) / 2;

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 255);
        SDL_RenderDrawLine(renderer, p1.x, p1.y, midX - dx * triangleSize, midY - dy * triangleSize);
        SDL_RenderDrawLine(renderer, midX + dx * triangleSize, midY + dy * triangleSize, p2.x, p2.y);

        SDL_Point triangle[4];
        triangle[0] = {int(midX - dx * triangleSize), int(midY - dy * triangleSize)};
        triangle[1] = {int(midX + px * triangleSize), int(midY + py * triangleSize)};
        triangle[2] = {int(midX - px * triangleSize), int(midY - py * triangleSize)};
        triangle[3] = triangle[0];

        SDL_RenderDrawLines(renderer, triangle, 4);

        SDL_RenderDrawLine(renderer,
                           midX + dx * triangleSize/2 + px * triangleSize/2,
                           midY + dy * triangleSize/2 + py * triangleSize/2,
                           midX + dx * triangleSize/2 - px * triangleSize/2,
                           midY + dy * triangleSize/2 - py * triangleSize/2);

        renderText(name, midX + px * triangleSize*2, midY + py * triangleSize*2, currentTheme.text);
    }

    SDL_Rect getBoundingBox(const map<int, SDL_Point>& nodePositions) const {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);

        SDL_Rect rect;
        rect.x = min(p1.x, p2.x) - 15;
        rect.y = min(p1.y, p2.y) - 15;
        rect.w = abs(p1.x - p2.x) + 30;
        rect.h = abs(p1.y - p2.y) + 30;

        return rect;
    }

    Component* clone() const override {
        return new Diode(*this);
    }

    template <class Archive>
    void serialize(Archive& archive) {
        archive(
            type, name, nodeName1, nodeName2, node1, node2
        );
    }
};

class Ground : public Component {
public:
    Ground(const string& n, int node) : Component(GROUND, n, node, 0, 0.0) {}

    void stamp(vector<vector<double>>&, vector<vector<double>>&, vector<vector<double>>&, vector<vector<double>>&,
               vector<double>&, vector<double>&, int&) override {}

    string getType() override { return "Ground"; }

    void render(SDL_Renderer* renderer, const map<int, SDL_Point>& nodePositions) const override {
        SDL_Point p = nodePositions.at(node1);
        const int size = 15;

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 255);

        SDL_RenderDrawLine(renderer, p.x, p.y, p.x, p.y + size);

        SDL_RenderDrawLine(renderer, p.x - size, p.y + size, p.x + size, p.y + size);
        SDL_RenderDrawLine(renderer, p.x - size/2, p.y + size*1.5, p.x + size/2, p.y + size*1.5);
        SDL_RenderDrawLine(renderer, p.x - size/4, p.y + size*2, p.x + size/4, p.y + size*2);

        renderText(name, p.x + size + 5, p.y + size, currentTheme.text);
    }

    SDL_Rect getBoundingBox(const map<int, SDL_Point>& nodePositions) const {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);

        SDL_Rect rect;
        rect.x = min(p1.x, p2.x) - 15;
        rect.y = min(p1.y, p2.y) - 15;
        rect.w = abs(p1.x - p2.x) + 30;
        rect.h = abs(p1.y - p2.y) + 30;

        return rect;
    }

    Component* clone() const override {
        return new Ground(*this);
    }

    template <class Archive>
    void serialize(Archive& archive) {
        archive(
            type, name, nodeName1, nodeName2, node1, node2
        );
    }
};

class VoltageSource : public Component {
public:
    VoltageSource(const string& n, int n1, int n2, double val) : Component(VOLTAGE_SOURCE, n, n1, n2, val) {}

    void stamp(vector<vector<double>>& G,
               vector<vector<double>>& B,
               vector<vector<double>>& C,
               vector<vector<double>>& D,
               vector<double>& J,
               vector<double>& E,
               int& nextVariable) override {
        int vsIndex = nextVariable++;

        if (node1 != 0) B[node1-1][vsIndex] = 1;
        if (node2 != 0) B[node2-1][vsIndex] = -1;

        if (node1 != 0) C[vsIndex][node1-1] = 1;
        if (node2 != 0) C[vsIndex][node2-1] = -1;

        E[vsIndex] = value;
    }
    string getType() override { return "VoltageSource"; }

    void render(SDL_Renderer* renderer, const map<int, SDL_Point>& nodePositions) const override {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);

        const int radius = 12;
        int centerX = (p1.x + p2.x) / 2;
        int centerY = (p1.y + p2.y) / 2;

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 255);
        SDL_RenderDrawLine(renderer, p1.x, p1.y, centerX - radius, centerY);
        SDL_RenderDrawLine(renderer, centerX + radius, centerY, p2.x, p2.y);

        for (int angle = 0; angle < 360; angle += 10) {
            float rad = angle * M_PI / 180.0f;
            int x = centerX + radius * cos(rad);
            int y = centerY + radius * sin(rad);
            SDL_RenderDrawPoint(renderer, x, y);
        }

        SDL_RenderDrawLine(renderer, centerX-5, centerY, centerX+5, centerY);
        SDL_RenderDrawLine(renderer, centerX, centerY-5, centerX, centerY+5);

        renderText(name, centerX + radius + 5, centerY - 10, currentTheme.text);
    }

    SDL_Rect getBoundingBox(const map<int, SDL_Point>& nodePositions) const {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);

        SDL_Rect rect;
        rect.x = min(p1.x, p2.x) - 15;
        rect.y = min(p1.y, p2.y) - 15;
        rect.w = abs(p1.x - p2.x) + 30;
        rect.h = abs(p1.y - p2.y) + 30;

        return rect;
    }

    Component* clone() const override {
        return new VoltageSource(*this);
    }

    template <class Archive>
    void serialize(Archive& archive) {
        archive(
            type, name, nodeName1, nodeName2, node1, node2, value
        );
    }
};

class SinVoltageSource : public Component {
    double amplitude;
    double frequency;
    double phase;
    double offset;
public:
    SinVoltageSource(const string& n, int n1, int n2, double amp, double freq, double ph = 0.0, double off = 0.0)
            : Component(SIN_VOLTAGE_SOURCE, n, n1, n2, 0.0),
              amplitude(amp), frequency(freq), phase(ph), offset(off) {}

    void stamp(vector<vector<double>>& G,
               vector<vector<double>>& B,
               vector<vector<double>>& C,
               vector<vector<double>>& D,
               vector<double>& J,
               vector<double>& E,
               int& nextVariable) override {
        int vsIndex = nextVariable++;

        if (node1 != 0) B[node1-1][vsIndex] = 1;
        if (node2 != 0) B[node2-1][vsIndex] = -1;

        if (node1 != 0) C[vsIndex][node1-1] = 1;
        if (node2 != 0) C[vsIndex][node2-1] = -1;

        double t = Circuit::currentTime;
        double radians = phase * M_PI / 180.0;
        E[vsIndex] = offset + amplitude * sin(2 * M_PI * frequency * t + radians);
    }

    string getInfo() const override {
        return "Sinusoidal Voltage Source " + name + " " + getNodeName(node1) + " " + getNodeName(node2) +
               " DC=" + to_string(offset) + " AMP=" + to_string(amplitude) +
               " FREQ=" + to_string(frequency) + " PHASE=" + to_string(phase);
    }

    string getType() override { return "SinVoltageSource"; }

    void render(SDL_Renderer* renderer, const map<int, SDL_Point>& nodePositions) const {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);

        const int segments = 5;
        const int amplitude = 10;

        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        float length = sqrt(dx*dx + dy*dy);

        dx /= length;
        dy /= length;
        float px = -dy;
        float py = dx;

        SDL_Point points[segments + 1];
        for (int i = 0; i <= segments; i++) {
            float t = (float)i / segments;
            float x = p1.x + t * (p2.x - p1.x);
            float y = p1.y + t * (p2.y - p1.y);

            float offset = (i % 2) ? amplitude : -amplitude;
            points[i].x = x + px * offset;
            points[i].y = y + py * offset;
        }

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 255);
        SDL_RenderDrawLines(renderer, points, segments + 1);

        int midX = (p1.x + p2.x)/2 + px * amplitude*2;
        int midY = (p1.y + p2.y)/2 + py * amplitude*2;
        renderText(name, midX, midY, currentTheme.text);
    }

    SDL_Rect getBoundingBox(const map<int, SDL_Point>& nodePositions) const {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);

        SDL_Rect rect;
        rect.x = min(p1.x, p2.x) - 15;
        rect.y = min(p1.y, p2.y) - 15;
        rect.w = abs(p1.x - p2.x) + 30;
        rect.h = abs(p1.y - p2.y) + 30;

        return rect;
    }

    Component* clone() const override {
        return new SinVoltageSource(*this);
    }

    template <class Archive>
    void serialize(Archive& archive) {
        archive(
            type, name, nodeName1, nodeName2, node1, node2, value, amplitude, frequency, phase, offset
        );
    }
};

class PulseVoltageSource : public Component {
    double v1, v2, td, tr, tf, pw, per;
public:
    PulseVoltageSource(const string& n, int n1, int n2, double v1, double v2,
                       double td, double tr, double tf, double pw, double per)
            : Component(PULSE_VOLTAGE_SOURCE, n, n1, n2, 0.0),
              v1(v1), v2(v2), td(td), tr(tr), tf(tf), pw(pw), per(per) {}

    void stamp(vector<vector<double>>& G,
               vector<vector<double>>& B,
               vector<vector<double>>& C,
               vector<vector<double>>& D,
               vector<double>& J,
               vector<double>& E,
               int& nextVariable) override {
        int vsIndex = nextVariable++;

        if (node1 != 0) B[node1-1][vsIndex] = 1;
        if (node2 != 0) B[node2-1][vsIndex] = -1;

        if (node1 != 0) C[vsIndex][node1-1] = 1;
        if (node2 != 0) C[vsIndex][node2-1] = -1;

        double t = Circuit::currentTime;
        double cycleTime = fmod(t - td, per);

        if (t < td) {
            E[vsIndex] = v1;
        } else if (cycleTime < tr) {
            E[vsIndex] = v1 + (v2 - v1) * (cycleTime / tr);
        } else if (cycleTime < tr + pw) {
            E[vsIndex] = v2;
        } else if (cycleTime < tr + pw + tf) {
            E[vsIndex] = v2 + (v1 - v2) * (cycleTime - tr - pw) / tf;
        } else {
            E[vsIndex] = v1;
        }
    }

    string getInfo() const override {
        return "Pulse Voltage Source " + name + " " + getNodeName(node1) + " " + getNodeName(node2) +
               " V1=" + to_string(v1) + " V2=" + to_string(v2) + " TD=" + to_string(td) +
               " TR=" + to_string(tr) + " TF=" + to_string(tf) + " PW=" + to_string(pw) +
               " PER=" + to_string(per);
    }

    string getType() override { return "PulseVoltageSource"; }

    void render(SDL_Renderer* renderer, const map<int, SDL_Point>& nodePositions) const {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);

        const int segments = 5;
        const int amplitude = 10;

        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        float length = sqrt(dx*dx + dy*dy);

        dx /= length;
        dy /= length;
        float px = -dy;
        float py = dx;

        SDL_Point points[segments + 1];
        for (int i = 0; i <= segments; i++) {
            float t = (float)i / segments;
            float x = p1.x + t * (p2.x - p1.x);
            float y = p1.y + t * (p2.y - p1.y);

            float offset = (i % 2) ? amplitude : -amplitude;
            points[i].x = x + px * offset;
            points[i].y = y + py * offset;
        }

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 255);
        SDL_RenderDrawLines(renderer, points, segments + 1);

        int midX = (p1.x + p2.x)/2 + px * amplitude*2;
        int midY = (p1.y + p2.y)/2 + py * amplitude*2;
        renderText(name, midX, midY, currentTheme.text);
    }

    SDL_Rect getBoundingBox(const map<int, SDL_Point>& nodePositions) const {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);

        SDL_Rect rect;
        rect.x = min(p1.x, p2.x) - 15;
        rect.y = min(p1.y, p2.y) - 15;
        rect.w = abs(p1.x - p2.x) + 30;
        rect.h = abs(p1.y - p2.y) + 30;

        return rect;
    }

    Component* clone() const override {
        return new PulseVoltageSource(*this);
    }

    template <class Archive>
    void serialize(Archive& archive) {
        archive(
            type, name, nodeName1, nodeName2, node1, node2, v1, v2, td, tf, tr, pw, per
        );
    }
};

class CurrentSource : public Component {
public:
    CurrentSource(const string& n, int n1, int n2, double val) : Component(CURRENT_SOURCE, n, n1, n2, val) {}

    void stamp(vector<vector<double>>& G,
               vector<vector<double>>& B,
               vector<vector<double>>& C,
               vector<vector<double>>& D,
               vector<double>& J,
               vector<double>& E,
               int& nextVariable) override {
        if (node1 != 0) J[node1-1] -= value;
        if (node2 != 0) J[node2-1] += value;
    }

    string getType() override { return "CurrentSource"; }

    void render(SDL_Renderer* renderer, const map<int, SDL_Point>& nodePositions) const override {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);

        const int radius = 12;
        int centerX = (p1.x + p2.x) / 2;
        int centerY = (p1.y + p2.y) / 2;

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 255);
        SDL_RenderDrawLine(renderer, p1.x, p1.y, centerX - radius, centerY);
        SDL_RenderDrawLine(renderer, centerX + radius, centerY, p2.x, p2.y);

        for (int angle = 0; angle < 360; angle += 10) {
            float rad = angle * M_PI / 180.0f;
            int x = centerX + radius * cos(rad);
            int y = centerY + radius * sin(rad);
            SDL_RenderDrawPoint(renderer, x, y);
        }

        SDL_RenderDrawLine(renderer, centerX, centerY-8, centerX, centerY+8);
        SDL_RenderDrawLine(renderer, centerX, centerY+8, centerX-5, centerY+3);
        SDL_RenderDrawLine(renderer, centerX, centerY+8, centerX+5, centerY+3);

        renderText(name, centerX + radius + 5, centerY - 10, currentTheme.text);
    }

    SDL_Rect getBoundingBox(const map<int, SDL_Point>& nodePositions) const {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);

        SDL_Rect rect;
        rect.x = min(p1.x, p2.x) - 15;
        rect.y = min(p1.y, p2.y) - 15;
        rect.w = abs(p1.x - p2.x) + 30;
        rect.h = abs(p1.y - p2.y) + 30;

        return rect;
    }

    Component* clone() const override {
        return new CurrentSource(*this);
    }

    template <class Archive>
    void serialize(Archive& archive) {
        archive(
            type, name, nodeName1, nodeName2, node1, node2, value
        );
    }
};

class SinCurrentSource : public Component {
    double amplitude;
    double frequency;
    double phase;
    double offset;
public:
    SinCurrentSource(const string& n, int n1, int n2, double amp, double freq, double ph = 0.0, double off = 0.0)
            : Component(SIN_CURRENT_SOURCE, n, n1, n2, 0.0),
              amplitude(amp), frequency(freq), phase(ph), offset(off) {}

    void stamp(vector<vector<double>>& G,
               vector<vector<double>>& B,
               vector<vector<double>>& C,
               vector<vector<double>>& D,
               vector<double>& J,
               vector<double>& E,
               int& nextVariable) override {
        double t = Circuit::currentTime;
        double radians = phase * M_PI / 180.0;
        double i = offset + amplitude * sin(2 * M_PI * frequency * t + radians);

        if (node1 != 0) J[node1 - 1] += i;
        if (node2 != 0) J[node2 - 1] += i;
    }

    string getInfo() const override {
        return "Sinusoidal Current Source " + name + " " + getNodeName(node1) + " " + getNodeName(node2) +
               " DC=" + to_string(offset) + " AMP=" + to_string(amplitude) +
               " FREQ=" + to_string(frequency) + " PHASE=" + to_string(phase);
    }

    string getType() override { return "SinCurrentSource"; }

    void render(SDL_Renderer* renderer, const map<int, SDL_Point>& nodePositions) const {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);

        const int segments = 5;
        const int amplitude = 10;

        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        float length = sqrt(dx*dx + dy*dy);

        dx /= length;
        dy /= length;
        float px = -dy;
        float py = dx;

        SDL_Point points[segments + 1];
        for (int i = 0; i <= segments; i++) {
            float t = (float)i / segments;
            float x = p1.x + t * (p2.x - p1.x);
            float y = p1.y + t * (p2.y - p1.y);

            float offset = (i % 2) ? amplitude : -amplitude;
            points[i].x = x + px * offset;
            points[i].y = y + py * offset;
        }

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 255);
        SDL_RenderDrawLines(renderer, points, segments + 1);

        int midX = (p1.x + p2.x)/2 + px * amplitude*2;
        int midY = (p1.y + p2.y)/2 + py * amplitude*2;
        renderText(name, midX, midY, currentTheme.text);
    }

    SDL_Rect getBoundingBox(const map<int, SDL_Point>& nodePositions) const {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);

        SDL_Rect rect;
        rect.x = min(p1.x, p2.x) - 15;
        rect.y = min(p1.y, p2.y) - 15;
        rect.w = abs(p1.x - p2.x) + 30;
        rect.h = abs(p1.y - p2.y) + 30;

        return rect;
    }

    Component* clone() const override {
        return new SinCurrentSource(*this);
    }

    template <class Archive>
    void serialize(Archive& archive) {
        archive(
            type, name, nodeName1, nodeName2, node1, node2, value, amplitude, frequency, phase, offset
        );
    }
};

class PulseCurrentSource : public Component {
    double i1, i2, td, tr, tf, pw, per;
public:
    PulseCurrentSource(const string& n, int n1, int n2, double i1, double i2,
                       double td, double tr, double tf, double pw, double per)
            : Component(PULSE_CURRENT_SOURCE, n, n1, n2, 0.0),
              i1(i1), i2(i2), td(td), tr(tr), tf(tf), pw(pw), per(per) {}

    void stamp(vector<vector<double>>& G,
               vector<vector<double>>& B,
               vector<vector<double>>& C,
               vector<vector<double>>& D,
               vector<double>& J,
               vector<double>& E,
               int& nextVariable) override {
        double t = Circuit::currentTime;
        double cycleTime = fmod(t - td, per);
        double i = i1;

        if (t < td) {
            i = i1;
        } else if (cycleTime < tr) {
            i = i1 + (i2 - i1) * (cycleTime / tr);
        } else if (cycleTime < tr + pw) {
            i = i2;
        } else if (cycleTime < tr + pw + tf) {
            i = i2 + (i1 - i2) * (cycleTime - tr - pw) / tf;
        }

        if (node1 != 0) J[node1 - 1] += i;
        if (node2 != 0) J[node2 - 1] += i;
    }

    string getInfo() const override {
        return "Pulse Current Source " + name + " " + getNodeName(node1) + " " + getNodeName(node2) +
               " I1=" + to_string(i1) + " I2=" + to_string(i2) + " TD=" + to_string(td) +
               " TR=" + to_string(tr) + " TF=" + to_string(tf) + " PW=" + to_string(pw) +
               " PER=" + to_string(per);
    }

    string getType() override { return "PulseCurrentSource"; }

    void render(SDL_Renderer* renderer, const map<int, SDL_Point>& nodePositions) const {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);

        const int segments = 5;
        const int amplitude = 10;

        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        float length = sqrt(dx*dx + dy*dy);

        dx /= length;
        dy /= length;
        float px = -dy;
        float py = dx;

        SDL_Point points[segments + 1];
        for (int i = 0; i <= segments; i++) {
            float t = (float)i / segments;
            float x = p1.x + t * (p2.x - p1.x);
            float y = p1.y + t * (p2.y - p1.y);

            float offset = (i % 2) ? amplitude : -amplitude;
            points[i].x = x + px * offset;
            points[i].y = y + py * offset;
        }

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 255);
        SDL_RenderDrawLines(renderer, points, segments + 1);

        int midX = (p1.x + p2.x)/2 + px * amplitude*2;
        int midY = (p1.y + p2.y)/2 + py * amplitude*2;
        renderText(name, midX, midY, currentTheme.text);
    }

    SDL_Rect getBoundingBox(const map<int, SDL_Point>& nodePositions) const {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);

        SDL_Rect rect;
        rect.x = min(p1.x, p2.x) - 15;
        rect.y = min(p1.y, p2.y) - 15;
        rect.w = abs(p1.x - p2.x) + 30;
        rect.h = abs(p1.y - p2.y) + 30;

        return rect;
    }

    Component* clone() const override {
        return new PulseCurrentSource(*this);
    }

    template <class Archive>
    void serialize(Archive& archive) {
        archive(
            type, name, nodeName1, nodeName2, node1, node2, i1, i2, td, tr, tf, pw, per
        );
    }
};

class VCVS : public Component {
    int ctrlNode1, ctrlNode2;
public:
    VCVS(const string& n, int n1, int n2, int cn1, int cn2, double gain)
            : Component(VCVS_SOURCE, n, n1, n2, gain), ctrlNode1(cn1), ctrlNode2(cn2) {}

    void stamp(vector<vector<double>>& G,
               vector<vector<double>>& B,
               vector<vector<double>>& C,
               vector<vector<double>>& D,
               vector<double>& J,
               vector<double>& E,
               int& nextVariable) override {
        int vsIndex = nextVariable++;

        if (node1 != 0) B[node1 - 1][vsIndex] = 1;
        if (node2 != 0) B[node2 - 1][vsIndex] = -1;
        if (node1 != 0) C[vsIndex][node1 - 1] = 1;
        if (node2 != 0) C[vsIndex][node2 - 1] = -1;

        if (ctrlNode1 != 0) D[vsIndex][ctrlNode1 - 1] -= value;
        if (ctrlNode2 != 0) D[vsIndex][ctrlNode2 - 1] += value;
    }

    string getType() override { return "VCVS"; }

    void render(SDL_Renderer* renderer, const map<int, SDL_Point>& nodePositions) const {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);
        SDL_Point cp1 = nodePositions.at(ctrlNode1);
        SDL_Point cp2 = nodePositions.at(ctrlNode2);

        // Draw the main component (diamond shape for dependent source)
        const int size = 15;
        int centerX = (p1.x + p2.x) / 2;
        int centerY = (p1.y + p2.y) / 2;

        // Draw diamond
        SDL_Point diamond[5] = {
                {centerX, centerY - size},
                {centerX + size, centerY},
                {centerX, centerY + size},
                {centerX - size, centerY},
                {centerX, centerY - size}
        };

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 255);
        SDL_RenderDrawLines(renderer, diamond, 5);

        // Draw leads
        SDL_RenderDrawLine(renderer, p1.x, p1.y, centerX, centerY - size);
        SDL_RenderDrawLine(renderer, p2.x, p2.y, centerX, centerY + size);

        // Draw control path (dashed line)
        int midCtrlX = (cp1.x + cp2.x) / 2;
        int midCtrlY = (cp1.y + cp2.y) / 2;

        // Draw dashed line from control nodes to component
        const int dashLength = 5;
        float dx = centerX - midCtrlX;
        float dy = centerY - midCtrlY;
        float distance = sqrt(dx*dx + dy*dy);
        dx /= distance;
        dy /= distance;

        for (float i = 0; i < distance; i += dashLength * 2) {
            float startX = midCtrlX + dx * i;
            float startY = midCtrlY + dy * i;
            float endX = midCtrlX + dx * (i + dashLength);
            float endY = midCtrlY + dy * (i + dashLength);

            if (endX > midCtrlX + dx * distance) endX = midCtrlX + dx * distance;
            if (endY > midCtrlY + dy * distance) endY = midCtrlY + dy * distance;

            SDL_RenderDrawLine(renderer, static_cast<int>(startX), static_cast<int>(startY),
                               static_cast<int>(endX), static_cast<int>(endY));
        }

        // Draw plus and minus signs at control nodes
        SDL_RenderDrawLine(renderer, cp1.x - 5, cp1.y, cp1.x + 5, cp1.y);
        SDL_RenderDrawLine(renderer, cp1.x, cp1.y - 5, cp1.x, cp1.y + 5);

        SDL_RenderDrawLine(renderer, cp2.x - 5, cp2.y, cp2.x + 5, cp2.y);

        // Draw gain value
        //renderText(name + " (G=" + to_string(value).substr(0,4) + ")", centerX + size + 5, centerY - 10, currentTheme.text);
    }

    SDL_Rect getBoundingBox(const map<int, SDL_Point>& nodePositions) const {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);
        SDL_Point cp1 = nodePositions.at(ctrlNode1);
        SDL_Point cp2 = nodePositions.at(ctrlNode2);

        int minX = min({p1.x, p2.x, cp1.x, cp2.x});
        int minY = min({p1.y, p2.y, cp1.y, cp2.y});
        int maxX = max({p1.x, p2.x, cp1.x, cp2.x});
        int maxY = max({p1.y, p2.y, cp1.y, cp2.y});

        return {minX - 20, minY - 20, maxX - minX + 40, maxY - minY + 40};
    }

    Component* clone() const override {
        return new VCVS(*this);
    }

    template <class Archive>
    void serialize(Archive& archive) {
        archive(
            type, name, nodeName1, nodeName2, node1, node2, ctrlNode1, ctrlNode2
        );
    }
};

class CCVS : public Component {
    string controllingVoltageSourceName;
    int controllingSourceIndex;
public:
    CCVS(const string& n, int n1, int n2, const string& ctrlName, double gain)
            : Component(CCVS_SOURCE, n, n1, n2, gain), controllingVoltageSourceName(ctrlName), controllingSourceIndex(-1) {}

    void resolveIndices(const unordered_map<string, int>& voltageSourceIndexMap) {
        controllingSourceIndex = voltageSourceIndexMap.at(controllingVoltageSourceName);
    }

    void stamp(vector<vector<double>>& G,
               vector<vector<double>>& B,
               vector<vector<double>>& C,
               vector<vector<double>>& D,
               vector<double>& J,
               vector<double>& E,
               int& nextVariable) override {
        int vsIndex = nextVariable++;

        if (node1 != 0) B[node1 - 1][vsIndex] = 1;
        if (node2 != 0) B[node2 - 1][vsIndex] = -1;
        if (node1 != 0) C[vsIndex][node1 - 1] = 1;
        if (node2 != 0) C[vsIndex][node2 - 1] = -1;

        D[vsIndex][controllingSourceIndex] = -value;
    }

    string getType() override { return "CCVS"; }

    void render(SDL_Renderer* renderer, const map<int, SDL_Point>& nodePositions) const {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);

        // Draw the main component (diamond shape for dependent source)
        const int size = 15;
        int centerX = (p1.x + p2.x) / 2;
        int centerY = (p1.y + p2.y) / 2;

        // Draw diamond
        SDL_Point diamond[5] = {
                {centerX, centerY - size},
                {centerX + size, centerY},
                {centerX, centerY + size},
                {centerX - size, centerY},
                {centerX, centerY - size}
        };

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 255);
        SDL_RenderDrawLines(renderer, diamond, 5);

        // Draw leads
        SDL_RenderDrawLine(renderer, p1.x, p1.y, centerX, centerY - size);
        SDL_RenderDrawLine(renderer, p2.x, p2.y, centerX, centerY + size);

        // Draw arrow pointing to controlling element
        // Find controlling component (simplified - in real implementation you'd need to look it up)
        SDL_Point controlPoint = {centerX + size*2, centerY};
        SDL_RenderDrawLine(renderer, centerX + size, centerY, controlPoint.x, controlPoint.y);
        SDL_RenderDrawLine(renderer, controlPoint.x, controlPoint.y, controlPoint.x - 5, controlPoint.y - 5);
        SDL_RenderDrawLine(renderer, controlPoint.x, controlPoint.y, controlPoint.x - 5, controlPoint.y + 5);

        // Draw gain value
//        renderText(name + " (R=" + to_string(value).substr(0,4) + ")", centerX + size + 5, centerY - 10, currentTheme.text);
//        renderText("Controls: " + controllingVoltageSourceName, centerX - 50, centerY + size + 15, currentTheme.text);
    }

    SDL_Rect getBoundingBox(const map<int, SDL_Point>& nodePositions) const {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);

        int minX = min(p1.x, p2.x) - 30;
        int minY = min(p1.y, p2.y) - 30;
        int maxX = max(p1.x, p2.x) + 30;
        int maxY = max(p1.y, p2.y) + 30;

        return {minX, minY, maxX - minX, maxY - minY};
    }

    Component* clone() const override {
        return new CCVS(*this);
    }

    template <class Archive>
    void serialize(Archive& archive) {
        archive(
            type, name, nodeName1, nodeName2, node1, node2, controllingVoltageSourceName, controllingSourceIndex
        );
    }
};

class VCCS : public Component {
    int ctrlNode1, ctrlNode2;
public:
    VCCS(const string& n, int n1, int n2, int cn1, int cn2, double gm)
            : Component(VCCS_SOURCE, n, n1, n2, gm), ctrlNode1(cn1), ctrlNode2(cn2) {}

    void stamp(vector<vector<double>>& G,
               vector<vector<double>>& B,
               vector<vector<double>>& C,
               vector<vector<double>>& D,
               vector<double>& J,
               vector<double>& E,
               int& nextVariable) override {
        if (node1 != 0 && ctrlNode1 != 0) G[node1 - 1][ctrlNode1 - 1] += value;
        if (node1 != 0 && ctrlNode2 != 0) G[node1 - 1][ctrlNode2 - 1] -= value;
        if (node2 != 0 && ctrlNode1 != 0) G[node2 - 1][ctrlNode1 - 1] -= value;
        if (node2 != 0 && ctrlNode2 != 0) G[node2 - 1][ctrlNode2 - 1] += value;
    }
    string getType() override { return "VCCS"; }

    void render(SDL_Renderer* renderer, const map<int, SDL_Point>& nodePositions) const {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);
        SDL_Point cp1 = nodePositions.at(ctrlNode1);
        SDL_Point cp2 = nodePositions.at(ctrlNode2);

        // Draw the main component (diamond shape with arrow for current source)
        const int size = 15;
        int centerX = (p1.x + p2.x) / 2;
        int centerY = (p1.y + p2.y) / 2;

        // Draw diamond
        SDL_Point diamond[5] = {
                {centerX, centerY - size},
                {centerX + size, centerY},
                {centerX, centerY + size},
                {centerX - size, centerY},
                {centerX, centerY - size}
        };

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 255);
        SDL_RenderDrawLines(renderer, diamond, 5);

        // Draw arrow inside diamond indicating current direction
        SDL_RenderDrawLine(renderer, centerX, centerY - size/2, centerX, centerY + size/2);
        SDL_RenderDrawLine(renderer, centerX, centerY + size/2, centerX - size/3, centerY + size/4);
        SDL_RenderDrawLine(renderer, centerX, centerY + size/2, centerX + size/3, centerY + size/4);

        // Draw leads
        SDL_RenderDrawLine(renderer, p1.x, p1.y, centerX - size, centerY);
        SDL_RenderDrawLine(renderer, p2.x, p2.y, centerX + size, centerY);

        // Draw control path (dashed line)
        int midCtrlX = (cp1.x + cp2.x) / 2;
        int midCtrlY = (cp1.y + cp2.y) / 2;

        // Draw dashed line from control nodes to component
        const int dashLength = 5;
        float dx = centerX - midCtrlX;
        float dy = centerY - midCtrlY;
        float distance = sqrt(dx*dx + dy*dy);
        dx /= distance;
        dy /= distance;

        for (float i = 0; i < distance; i += dashLength * 2) {
            float startX = midCtrlX + dx * i;
            float startY = midCtrlY + dy * i;
            float endX = midCtrlX + dx * (i + dashLength);
            float endY = midCtrlY + dy * (i + dashLength);

            if (endX > midCtrlX + dx * distance) endX = midCtrlX + dx * distance;
            if (endY > midCtrlY + dy * distance) endY = midCtrlY + dy * distance;

            SDL_RenderDrawLine(renderer, static_cast<int>(startX), static_cast<int>(startY),
                               static_cast<int>(endX), static_cast<int>(endY));
        }

        // Draw plus and minus signs at control nodes
        SDL_RenderDrawLine(renderer, cp1.x - 5, cp1.y, cp1.x + 5, cp1.y);
        SDL_RenderDrawLine(renderer, cp1.x, cp1.y - 5, cp1.x, cp1.y + 5);

        SDL_RenderDrawLine(renderer, cp2.x - 5, cp2.y, cp2.x + 5, cp2.y);

        // Draw transconductance value
        //  renderText(name + " (gm=" + to_string(value).substr(0,4) + ")", centerX + size + 5, centerY - 10, currentTheme.text);
    }

    SDL_Rect getBoundingBox(const map<int, SDL_Point>& nodePositions) const {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);
        SDL_Point cp1 = nodePositions.at(ctrlNode1);
        SDL_Point cp2 = nodePositions.at(ctrlNode2);

        int minX = min({p1.x, p2.x, cp1.x, cp2.x});
        int minY = min({p1.y, p2.y, cp1.y, cp2.y});
        int maxX = max({p1.x, p2.x, cp1.x, cp2.x});
        int maxY = max({p1.y, p2.y, cp1.y, cp2.y});

        return {minX - 20, minY - 20, maxX - minX + 40, maxY - minY + 40};
    }

    Component* clone() const override {
        return new VCCS(*this);
    }

    template <class Archive>
    void serialize(Archive& archive) {
        archive(
            type, name, nodeName1, nodeName2, node1, node2, ctrlNode1, ctrlNode2
        );
    }
};

class CCCS : public Component {
    string controllingVoltageSourceName;
    int controllingSourceIndex;
public:
    CCCS(const string& n, int n1, int n2, const string& ctrlName, double beta)
            : Component(CCCS_SOURCE, n, n1, n2, beta), controllingVoltageSourceName(ctrlName), controllingSourceIndex(-1) {}

    void resolveIndices(const unordered_map<string, int>& voltageSourceIndexMap) {
        controllingSourceIndex = voltageSourceIndexMap.at(controllingVoltageSourceName);
    }

    void stamp(vector<vector<double>>& G,
               vector<vector<double>>& B,
               vector<vector<double>>& C,
               vector<vector<double>>& D,
               vector<double>& J,
               vector<double>& E,
               int& nextVariable) override {
        if (node1 != 0) D[node1 - 1][controllingSourceIndex] += value;
        if (node2 != 0) D[node2 - 1][controllingSourceIndex] -= value;
    }

    string getType() override { return "CCCS"; }

    void render(SDL_Renderer* renderer, const map<int, SDL_Point>& nodePositions) const {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);

        // Draw the main component (diamond shape with arrow for current source)
        const int size = 15;
        int centerX = (p1.x + p2.x) / 2;
        int centerY = (p1.y + p2.y) / 2;

        // Draw diamond
        SDL_Point diamond[5] = {
                {centerX, centerY - size},
                {centerX + size, centerY},
                {centerX, centerY + size},
                {centerX - size, centerY},
                {centerX, centerY - size}
        };

        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 255);
        SDL_RenderDrawLines(renderer, diamond, 5);

        // Draw arrow inside diamond indicating current direction
        SDL_RenderDrawLine(renderer, centerX, centerY - size/2, centerX, centerY + size/2);
        SDL_RenderDrawLine(renderer, centerX, centerY + size/2, centerX - size/3, centerY + size/4);
        SDL_RenderDrawLine(renderer, centerX, centerY + size/2, centerX + size/3, centerY + size/4);

        // Draw leads
        SDL_RenderDrawLine(renderer, p1.x, p1.y, centerX - size, centerY);
        SDL_RenderDrawLine(renderer, p2.x, p2.y, centerX + size, centerY);

        // Draw arrow pointing to controlling element
        // Find controlling component (simplified - in real implementation you'd need to look it up)
        SDL_Point controlPoint = {centerX + size*2, centerY};
        SDL_RenderDrawLine(renderer, centerX + size, centerY, controlPoint.x, controlPoint.y);
        SDL_RenderDrawLine(renderer, controlPoint.x, controlPoint.y, controlPoint.x - 5, controlPoint.y - 5);
        SDL_RenderDrawLine(renderer, controlPoint.x, controlPoint.y, controlPoint.x - 5, controlPoint.y + 5);

        // Draw gain value
//        renderText(name + " (F=" + to_string(value).substr(0,4) + ")", centerX + size + 5, centerY - 10, currentTheme.text);
//        renderText("Controls: " + controllingVoltageSourceName, centerX - 50, centerY + size + 15, currentTheme.text);
    }

    SDL_Rect getBoundingBox(const map<int, SDL_Point>& nodePositions) const {
        SDL_Point p1 = nodePositions.at(node1);
        SDL_Point p2 = nodePositions.at(node2);

        int minX = min(p1.x, p2.x) - 30;
        int minY = min(p1.y, p2.y) - 30;
        int maxX = max(p1.x, p2.x) + 30;
        int maxY = max(p1.y, p2.y) + 30;

        return {minX, minY, maxX - minX, maxY - minY};
    }

    Component* clone() const override {
        return new CCCS(*this);
    }

    template <class Archive>
    void serialize(Archive& archive) {
        archive(
            type, name, nodeName1, nodeName2, node1, node2, controllingVoltageSourceName, controllingSourceIndex
        );
    }
};

void processCircuitFile(const string& filename, Circuit& circuit) {
    ifstream file(filename);
    if (!file.is_open()) {
        string withExtension = filename;
        if (filename.find(".txt") == string::npos) {
            withExtension += ".txt";
            file.open(withExtension);
        }

        if (!file.is_open()) {
            cerr << "Error: Could not open file '" << filename << "'" << endl;
            char cwd[MAX_PATH];
            if (GETCWD(cwd, sizeof(cwd))) {
                cerr << "Current working directory: " << cwd << endl;
            }
            return;
        }
    }

    string line;
    while (getline(file, line)) {
        if (line.empty() || line[0] == '*') continue;

        istringstream iss(line);
        string type, name, node1, node2, valStr;
        iss >> type >> name >> node1 >> node2;

        if (type.empty()) continue;

        try {
            char typeChar = toupper(type[0]);
            if (typeChar == 'G' && type.size() >= 3 &&
                toupper(type[1]) == 'N' && toupper(type[2]) == 'D') {
                circuit.addComponent(new Ground("GND", getOrCreateNode(node1)));
                cout << "Added ground connection to node " << node1 << endl;
                continue;
            }

            int n1 = getOrCreateNode(node1);
            int n2 = getOrCreateNode(node2);

            switch(typeChar) {
                case 'R':
                case 'C':
                case 'L': {
                    string valStr;
                    if (!(iss >> valStr)) {
                        cerr << "Error: Missing value for component " << name << endl;
                        continue;
                    }
                    double value = parseSpiceValue(valStr);
                    if (value <= 0) {
                        cerr << "Error: Value must be positive for " << name << endl;
                        continue;
                    }
                    if (typeChar == 'R') {
                        circuit.addComponent(new Resistor(name, n1, n2, value));
                    } else if (typeChar == 'C') {
                        circuit.addComponent(new Capacitor(name, n1, n2, value));
                    } else {
                        circuit.addComponent(new Inductor(name, n1, n2, value));
                    }
                    break;
                }
                case 'V': {
                    if (type.size() > 1 && toupper(type[1]) == 'S') {
                        string dcStr, ampStr, freqStr, phaseStr = "0";
                        if (!(iss >> dcStr >> ampStr >> freqStr)) {
                            cerr << "Error: Invalid sinusoidal source parameters" << endl;
                            continue;
                        }
                        iss >> phaseStr;
                        double dc = parseSpiceValue(dcStr);
                        double amp = parseSpiceValue(ampStr);
                        double freq = parseSpiceValue(freqStr);
                        double phase = parseSpiceValue(phaseStr);
                        circuit.addComponent(new SinVoltageSource(name, n1, n2, amp, freq, phase, dc));
                    } else if (type.size() > 1 && toupper(type[1]) == 'P') {
                        string v1Str, v2Str, tdStr, trStr, tfStr, pwStr, perStr;
                        if (!(iss >> v1Str >> v2Str >> tdStr >> trStr >> tfStr >> pwStr >> perStr)) {
                            cerr << "Error: Invalid pulse source parameters" << endl;
                            continue;
                        }
                        double v1 = parseSpiceValue(v1Str);
                        double v2 = parseSpiceValue(v2Str);
                        double td = parseSpiceValue(tdStr);
                        double tr = parseSpiceValue(trStr);
                        double tf = parseSpiceValue(tfStr);
                        double pw = parseSpiceValue(pwStr);
                        double per = parseSpiceValue(perStr);
                        circuit.addComponent(new PulseVoltageSource(name, n1, n2, v1, v2, td, tr, tf, pw, per));
                    } else {
                        string valStr;
                        if (!(iss >> valStr)) {
                            cerr << "Error: Missing value for voltage source " << name << endl;
                            continue;
                        }
                        double value = parseSpiceValue(valStr);
                        circuit.addComponent(new VoltageSource(name, n1, n2, value));
                    }
                    break;
                }
                case 'I': {
                    if (type.size() > 1 && toupper(type[1]) == 'S') {
                        string dcStr, ampStr, freqStr, phaseStr = "0";
                        if (!(iss >> dcStr >> ampStr >> freqStr)) {
                            cerr << "Error: Invalid sinusoidal source parameters" << endl;
                            continue;
                        }
                        iss >> phaseStr;
                        double dc = parseSpiceValue(dcStr);
                        double amp = parseSpiceValue(ampStr);
                        double freq = parseSpiceValue(freqStr);
                        double phase = parseSpiceValue(phaseStr);
                        circuit.addComponent(new SinCurrentSource(name, n1, n2, amp, freq, phase, dc));
                    } else if (type.size() > 1 && toupper(type[1]) == 'P') {
                        string i1Str, i2Str, tdStr, trStr, tfStr, pwStr, perStr;
                        if (!(iss >> i1Str >> i2Str >> tdStr >> trStr >> tfStr >> pwStr >> perStr)) {
                            cerr << "Error: Invalid pulse source parameters" << endl;
                            continue;
                        }
                        double i1 = parseSpiceValue(i1Str);
                        double i2 = parseSpiceValue(i2Str);
                        double td = parseSpiceValue(tdStr);
                        double tr = parseSpiceValue(trStr);
                        double tf = parseSpiceValue(tfStr);
                        double pw = parseSpiceValue(pwStr);
                        double per = parseSpiceValue(perStr);
                        circuit.addComponent(new PulseCurrentSource(name, n1, n2, i1, i2, td, tr, tf, pw, per));
                    } else {
                        string valStr;
                        if (!(iss >> valStr)) {
                            cerr << "Error: Missing value for current source " << name << endl;
                            continue;
                        }
                        double value = parseSpiceValue(valStr);
                        circuit.addComponent(new CurrentSource(name, n1, n2, value));
                    }
                    break;
                }
                case 'D': {
                    circuit.addComponent(new Diode(name, n1, n2));
                    break;
                }
                case 'E': {
                    string cn1, cn2, gainStr;
                    if (!(iss >> cn1 >> cn2 >> gainStr)) {
                        cerr << "Error: Invalid VCVS parameters" << endl;
                        continue;
                    }
                    int ctrlNode1 = getOrCreateNode(cn1);
                    int ctrlNode2 = getOrCreateNode(cn2);
                    double gain = parseSpiceValue(gainStr);
                    circuit.addComponent(new VCVS(name, n1, n2, ctrlNode1, ctrlNode2, gain));
                    break;
                }
                case 'G': {
                    string cn1, cn2, gmStr;
                    if (!(iss >> cn1 >> cn2 >> gmStr)) {
                        cerr << "Error: Invalid VCCS parameters" << endl;
                        continue;
                    }
                    int ctrlNode1 = getOrCreateNode(cn1);
                    int ctrlNode2 = getOrCreateNode(cn2);
                    double gm = parseSpiceValue(gmStr);
                    circuit.addComponent(new VCCS(name, n1, n2, ctrlNode1, ctrlNode2, gm));
                    break;
                }
                case 'H': {
                    string vsName, gainStr;
                    if (!(iss >> vsName >> gainStr)) {
                        cerr << "Error: Invalid CCVS parameters" << endl;
                        continue;
                    }
                    double gain = parseSpiceValue(gainStr);
                    circuit.addComponent(new CCVS(name, n1, n2, vsName, gain));
                    break;
                }
                case 'F': {
                    string vsName, gainStr;
                    if (!(iss >> vsName >> gainStr)) {
                        cerr << "Error: Invalid CCCS parameters" << endl;
                        continue;
                    }
                    double gain = parseSpiceValue(gainStr);
                    circuit.addComponent(new CCCS(name, n1, n2, vsName, gain));
                    break;
                }
                default:
                    cerr << "Error: Unknown component type " << type << endl;
            }
            cout << "Added component: " << line << endl;
        } catch (const exception& e) {
            cerr << "Error processing line: " << line << " - " << e.what() << endl;
        }
    }
    file.close();
}

void resetGlobalState() {
    nodeMap.clear();
    reverseNodeMap.clear();
    nextNodeNumber = 1;
    voltages.clear();
    currents.clear();
    Vtimes.clear();
    Itimes.clear();
    hasDynamic = false;
    showVoltage = true;
    showCurrent = false;
}

bool initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << endl;
        return false;
    }

    if (TTF_Init() == -1) {
        cerr << "SDL_ttf could not initialize! TTF_Error: " << TTF_GetError() << endl;
        return false;
    }

    window = SDL_CreateWindow("Circuit Simulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << endl;
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << endl;
        return false;
    }

    font = TTF_OpenFont("C:\\Windows\\Fonts\\consola.ttf", 24);
    if (!font) {
        cerr << "Failed to load font! TTF_Error: " << TTF_GetError() << endl;
        return false;
    }

    return true;
}

enum AppState {
    MAIN_VIEW,
    EDITOR_VIEW,
    TOOLS,
    FILES,
    ADDING_COMPONENT,
    COMPONENT_LIBRARY,
    ANALYSIS_SETTINGS,
    PLOT_VIEW
};

string selectedComponentType = "";
AppState currentState = MAIN_VIEW;
vector<Circuit*> undoStack;
vector<Circuit*> redoStack;
string copiedComponent;

const string CIRCUIT_EXTENSION = ".cir";

string ensureExtension(const string& filename) {
    if (filename.find('.') == string::npos) {
        return filename + CIRCUIT_EXTENSION;
    }
    return filename;
}

vector<string> listCircuitFiles(const string& directory) {
    vector<string> files;
    // Similar to your listTxtFiles but look for .cir files
    WIN32_FIND_DATA findData;
    HANDLE hFind = FindFirstFile((directory + "\\*.cir").c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                files.push_back(findData.cFileName);
            }
        } while (FindNextFile(hFind, &findData) != 0);
        FindClose(hFind);
    }
    return files;
}

template<class Archive>
void Circuit::save(Archive &archive) const {
    // Save component count
    archive(components.size());

    // Save each component with type information
    for (const auto& comp : components) {
        archive(comp->getType());

        // Save component-specific data
        if (auto resistor = dynamic_cast<Resistor*>(comp)) {
            archive(*resistor);
        } else if (auto capacitor = dynamic_cast<Capacitor*>(comp)) {
            archive(*capacitor);
        } else if (auto inductor = dynamic_cast<Inductor*>(comp)) {
            archive(*inductor);
        } else if (auto voltageSource = dynamic_cast<VoltageSource*>(comp)) {
            archive(*voltageSource);
        } else if (auto currentSource = dynamic_cast<CurrentSource*>(comp)) {
            archive(*currentSource);
        } else if (auto diode = dynamic_cast<Diode*>(comp)) {
            archive(*diode);
        } else if (auto ground = dynamic_cast<Ground*>(comp)) {
            archive(*ground);
        } else if (auto sinVoltageSource = dynamic_cast<SinVoltageSource*>(comp)) {
            archive(*sinVoltageSource);
        } else if (auto sinCurrentSource = dynamic_cast<SinCurrentSource*>(comp)) {
            archive(*sinCurrentSource);
        } else if (auto pulseVoltageSource = dynamic_cast<PulseVoltageSource*>(comp)) {
            archive(*pulseVoltageSource);
        } else if (auto pulseCurrentSource = dynamic_cast<PulseCurrentSource*>(comp)) {
            archive(*pulseCurrentSource);
        }
    }

    // Save circuit data
    archive(
        maxNode,
        wireConnections,
        nodePositions,
        nodeMap,
        reverseNodeMap,
        nextNodeNumber
    );
}

template <class Archive>
void Circuit::load(Archive& archive) {
    // Clear existing components
    for (auto comp : components) {
        delete comp;
    }
    components.clear();

    // Load component count
    size_t numComponents;
    archive(numComponents);

    // Load each component
    for (size_t i = 0; i < numComponents; i++) {
        string type;
        archive(type);

        Component* comp = nullptr;
        if (type == "Resistor") {
            Resistor* resistor = new Resistor("", 0, 0, 0);
            archive(*resistor);
            comp = resistor;
        } else if (type == "Capacitor") {
            Capacitor* capacitor = new Capacitor("", 0, 0, 0);
            archive(*capacitor);
            comp = capacitor;
        } else if (type == "Inductor") {
            Inductor* inductor = new Inductor("", 0, 0, 0);
            archive(*inductor);
            comp = inductor;
        } else if (type == "VoltageSource") {
            VoltageSource* vs = new VoltageSource("", 0, 0, 0);
            archive(*vs);
            comp = vs;
        } else if (type == "CurrentSource") {
            CurrentSource* cs = new CurrentSource("", 0, 0, 0);
            archive(*cs);
            comp = cs;
        } else if (type == "Diode") {
            Diode* diode = new Diode("", 0, 0);
            archive(*diode);
            comp = diode;
        } else if (type == "Ground") {
            Ground* ground = new Ground("", 0);
            archive(*ground);
            comp = ground;
        } else if (type == "SinVoltageSource") {
            SinVoltageSource* svs = new SinVoltageSource("", 0, 0, 0, 0, 0, 0);
            archive(*svs);
            comp = svs;
        } else if (type == "SinCurrentSource") {
            SinCurrentSource* css = new SinCurrentSource("", 0, 0, 0, 0, 0, 0);
            archive(*css);
            comp = css;
        } else if (type == "PulseVoltageSource") {
            PulseVoltageSource* pvs = new PulseVoltageSource("", 0, 0, 0, 0, 0, 0, 0, 0, 0);
            archive(*pvs);
            comp = pvs;
        } else if (type == "PulseCurrentSource") {
            PulseCurrentSource* pcs = new PulseCurrentSource("", 0, 0, 0, 0, 0, 0, 0, 0, 0);
            archive(*pcs);
            comp = pcs;
        }

        if (comp) {
            components.push_back(comp);
        }
    }

    // Load circuit data
    archive(
        maxNode,
        wireConnections,
        nodePositions,
        nodeMap,
        reverseNodeMap,
        nextNodeNumber
    );
}

void Circuit::saveToFile(const string& filename) const {
    try {
        std::ofstream ofs(filename, std::ios::binary);
        cereal::BinaryOutputArchive archive(ofs);
        save(archive);
        cout << "Circuit saved successfully to: " << filename << endl;
    } catch (const std::exception& e) {
        cerr << "Error saving circuit: " << e.what() << endl;
    }
}

bool Circuit::loadFromFile(const string& filename) {
    try {
        std::ifstream ifs(filename, std::ios::binary);
        if (!ifs.is_open()) {
            cerr << "Could not open file: " << filename << endl;
            return false;
        }

        cereal::BinaryInputArchive archive(ifs);
        load(archive);
        cout << "Circuit loaded successfully from: " << filename << endl;
        return true;
    } catch (const std::exception& e) {
        cerr << "Error loading circuit: " << e.what() << endl;
        return false;
    }
}

void saveUndoState(Circuit*& currentCircuit) {
    for (auto circuit : redoStack) {
        delete circuit;
    }
    redoStack.clear();

    Circuit* copy = new Circuit(*currentCircuit);
    copy->nodePositions = currentCircuit->nodePositions;

    undoStack.push_back(copy);

    if (undoStack.size() > 20) {
        delete undoStack.front();
        undoStack.erase(undoStack.begin());
    }
}

void undo(Circuit*& currentCircuit) {
    if (!undoStack.empty()) {
        Circuit* redoState = new Circuit(*currentCircuit);
        redoState->nodePositions = currentCircuit->nodePositions;
        redoStack.push_back(redoState);

        delete currentCircuit;
        currentCircuit = new Circuit(*undoStack.back());
        currentCircuit->nodePositions = undoStack.back()->nodePositions;
        undoStack.pop_back();
    }
}

void redo(Circuit*& currentCircuit) {
    if (!redoStack.empty()) {
        Circuit* undoState = new Circuit(*currentCircuit);
        undoState->nodePositions = currentCircuit->nodePositions;
        undoStack.push_back(undoState);

        delete currentCircuit;
        currentCircuit = new Circuit(*redoStack.back());
        currentCircuit->nodePositions = redoStack.back()->nodePositions;
        redoStack.pop_back();
    }
}

void showAnalysisSettings(SDL_Renderer* renderer) {
    SDL_Rect analysisWindow = {250, 150, 360, 300};

    SDL_SetRenderDrawColor(renderer, currentTheme.background.r, currentTheme.background.g, currentTheme.background.b, 255);
    SDL_RenderFillRect(renderer, &analysisWindow);

    renderText("Analysis Settings", analysisWindow.x + 20, analysisWindow.y + 20, currentTheme.text);

    renderText("Time Step (s):", analysisWindow.x + 20, analysisWindow.y + 60, currentTheme.text);
    TextBox stepBox = {analysisWindow.x + 150, analysisWindow.y + 60, 150, 30, to_string(tStep)};
    renderTextBox(stepBox);

    renderText("Stop Time (s):", analysisWindow.x + 20, analysisWindow.y + 110, currentTheme.text);
    TextBox stopBox = {analysisWindow.x + 150, analysisWindow.y + 110, 150, 30, to_string(tStop)};
    renderTextBox(stopBox);

    Button runBtn = {analysisWindow.x + 50, analysisWindow.y + 220, 100, 40, "Run", GREEN};
    Button cancelBtn = {analysisWindow.x + 200, analysisWindow.y + 220, 100, 40, "Cancel", RED};
    renderButton(runBtn);
    renderButton(cancelBtn);
}

void showFileDialog(SDL_Renderer* renderer, const vector<string>& files) {
    SDL_Rect dialog = {200, 150, 400, 400};

    SDL_SetRenderDrawColor(renderer, currentTheme.background.r, currentTheme.background.g, currentTheme.background.b, 255);
    SDL_RenderFillRect(renderer, &dialog);

    renderText("Open Circuit File", dialog.x + 20, dialog.y + 20, currentTheme.text);

    SDL_Rect fileList = {dialog.x + 20, dialog.y + 60, 360, 250};
    SDL_SetRenderDrawColor(renderer, currentTheme.plotBg.r, currentTheme.plotBg.g, currentTheme.plotBg.b, 255);
    SDL_RenderFillRect(renderer, &fileList);

    for (size_t i = 0; i < files.size(); i++) {
        SDL_Rect fileRect = {fileList.x + 10, fileList.y + 10 + (int)i*30, fileList.w - 20, 25};
        SDL_SetRenderDrawColor(renderer, currentTheme.background.r, currentTheme.background.g, currentTheme.background.b, 255);
        SDL_RenderFillRect(renderer, &fileRect);
        renderText(files[i], fileRect.x + 5, fileRect.y + 5, currentTheme.text);
    }

    Button openBtn = {dialog.x + 100, dialog.y + 330, 100, 40, "Open", GREEN};
    Button cancelBtn = {dialog.x + 220, dialog.y + 330, 100, 40, "Cancel", RED};
    renderButton(openBtn);
    renderButton(cancelBtn);
}

void showFileMenu(SDL_Renderer* renderer, SDL_Rect menuRect) {
    SDL_SetRenderDrawColor(renderer, currentTheme.background.r, currentTheme.background.g, currentTheme.background.b, 255);
    SDL_RenderFillRect(renderer, &menuRect);

    Button newBtn = {menuRect.x + 10, menuRect.y + 40, 120, 30, "New Circuit", currentTheme.button};
    Button openBtn = {menuRect.x + 10, menuRect.y + 80, 120, 30, "Open", currentTheme.button};
    Button saveBtn = {menuRect.x + 10, menuRect.y + 120, 120, 30, "Save", currentTheme.button};
    Button saveAsBtn = {menuRect.x + 10, menuRect.y + 160, 120, 30, "Save As", currentTheme.button};
    Button exitBtn = {menuRect.x + 10, menuRect.y + 200, 120, 30, "Exit", RED};

    renderButton(newBtn);
    renderButton(openBtn);
    renderButton(saveBtn);
    renderButton(saveAsBtn);
    renderButton(exitBtn);
}

void showEditMenu(SDL_Renderer* renderer, SDL_Rect menuRect) {
    SDL_SetRenderDrawColor(renderer, currentTheme.background.r, currentTheme.background.g, currentTheme.background.b, 255);
    SDL_RenderFillRect(renderer, &menuRect);

    Button undoBtn = {menuRect.x + 10, menuRect.y + 40, 120, 30, "Undo", currentTheme.button};
    Button redoBtn = {menuRect.x + 10, menuRect.y + 80, 120, 30, "Redo", currentTheme.button};
    Button copyBtn = {menuRect.x + 10, menuRect.y + 120, 120, 30, "Copy", currentTheme.button};
    Button pasteBtn = {menuRect.x + 10, menuRect.y + 160, 120, 30, "Paste", currentTheme.button};
    Button deleteBtn = {menuRect.x + 10, menuRect.y + 200, 120, 30, "Delete", RED};
    Button deleteBtn1 = {menuRect.x + 10, menuRect.y + 200, 120, 30, "Delete", RED};

    renderButton(deleteBtn1);
    renderButton(undoBtn);
    renderButton(redoBtn);
    renderButton(copyBtn);
    renderButton(pasteBtn);
    renderButton(deleteBtn);
}

void showComponentProperties(SDL_Renderer* renderer, Component* component) {
    if (!component) return;

    SDL_Rect propWindow = {600, 100, 350, 400};

    SDL_SetRenderDrawColor(renderer, currentTheme.background.r, currentTheme.background.g, currentTheme.background.b, 255);
    SDL_RenderFillRect(renderer, &propWindow);

    renderText("Component Properties", propWindow.x + 20, propWindow.y + 20, currentTheme.text);

    renderText("Type: " + component->getType(), propWindow.x + 20, propWindow.y + 60, currentTheme.text);
    renderText("Name: " + component->name, propWindow.x + 20, propWindow.y + 90, currentTheme.text);

    renderText("Connections:", propWindow.x + 20, propWindow.y + 120, currentTheme.text);
    renderText("Node 1: " + getNodeName(component->node1), propWindow.x + 40, propWindow.y + 150, currentTheme.text);
    renderText("Node 2: " + getNodeName(component->node2), propWindow.x + 40, propWindow.y + 180, currentTheme.text);

    if (component->type != GROUND && component->type != DIODE) {
        renderText("Value:", propWindow.x + 20, propWindow.y + 210, currentTheme.text);
        TextBox valueBox = {propWindow.x + 100, propWindow.y + 210, 150, 30, to_string(component->value)};
        renderTextBox(valueBox);
    }

    if (component->type == SIN_VOLTAGE_SOURCE || component->type == SIN_CURRENT_SOURCE) {
        SinVoltageSource* src = dynamic_cast<SinVoltageSource*>(component);
        renderText("Amplitude:", propWindow.x + 20, propWindow.y + 250, currentTheme.text);
        TextBox ampBox = {propWindow.x + 120, propWindow.y + 250, 150, 30, to_string(src->value)};
        renderTextBox(ampBox);
    }

    Button saveBtn = {propWindow.x + 50, propWindow.y + 320, 100, 40, "Save", GREEN};
    Button deleteBtn = {propWindow.x + 200, propWindow.y + 320, 100, 40, "Delete", RED};
    renderButton(saveBtn);
    renderButton(deleteBtn);
}

void showSaveAsDialog(SDL_Renderer* renderer, string& currentFilename) {
    SDL_Rect dialog = {250, 200, 300, 200};

    SDL_SetRenderDrawColor(renderer, currentTheme.background.r, currentTheme.background.g, currentTheme.background.b, 255);
    SDL_RenderFillRect(renderer, &dialog);

    renderText("Save Circuit As", dialog.x + 20, dialog.y + 20, currentTheme.text);

    renderText("Filename:", dialog.x + 20, dialog.y + 60, currentTheme.text);
    nameBox = {dialog.x + 100, dialog.y + 60, 180, 30, currentFilename};
    renderTextBox(nameBox);

    Button saveBtn = {dialog.x + 50, dialog.y + 120, 100, 40, "Save", GREEN};
    Button cancelBtn = {dialog.x + 170, dialog.y + 120, 100, 40, "Cancel", RED};
    renderButton(saveBtn);
    renderButton(cancelBtn);
}

void drawToolbar(SDL_Renderer* renderer) {
    SDL_Rect toolbar = {0, 0, SCREEN_WIDTH, 40};
    SDL_SetRenderDrawColor(renderer, currentTheme.toolbar.r, currentTheme.toolbar.g, currentTheme.toolbar.b, 255);
    SDL_RenderFillRect(renderer, &toolbar);

    Button fileBtn = {10, 5, 80, 30, "File", currentTheme.button, false};
    Button editBtn = {100, 5, 80, 30, "Edit", currentTheme.button, false};
    Button viewBtn = {190, 5, 80, 30, "View", currentTheme.button, false};
    Button analyzeBtn = {280, 5, 80, 30, "Analyze", currentTheme.button, false};
    Button toolsBtn = {370, 5, 80, 30, "Tools", currentTheme.button, false};
    Button darkModeBtn = {460, 5, 100, 30, darkMode ? "Light Mode" : "Dark Mode", currentTheme.button};

    renderButton(fileBtn);
    renderButton(editBtn);
    renderButton(viewBtn);
    renderButton(analyzeBtn);
    renderButton(toolsBtn);
    renderButton(darkModeBtn);
}

bool isPointNearLine(int px, int py, int x1, int y1, int x2, int y2, int threshold) {
    float lineLength = sqrtf((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
    if (lineLength == 0) return false;

    float u = ((px - x1) * (x2 - x1) + (py - y1) * (y2 - y1)) / (lineLength * lineLength);
    u = fmaxf(0, fminf(1, u));

    float closestX = x1 + u * (x2 - x1);
    float closestY = y1 + u * (y2 - y1);

    float distance = sqrtf((px - closestX) * (px - closestX) + (py - closestY) * (py - closestY));
    return distance <= threshold;
}

bool isMouseOver(const SDL_Rect& rect, int x, int y) {
    return (x >= rect.x && x <= rect.x + rect.w && y >= rect.y && y <= rect.y + rect.h);
}

void getPerpendicularPoints(int x1, int y1, int x2, int y2, int offset,
                            int& outX1, int& outY1, int& outX2, int& outY2) {
    int dx = x2 - x1;
    int dy = y2 - y1;

    float length = sqrt(dx*dx + dy*dy);
    float nx = -dy/length;
    float ny = dx/length;

    outX1 = x1 + nx * offset;
    outY1 = y1 + ny * offset;
    outX2 = x2 + nx * offset;
    outY2 = y2 + ny * offset;
}

void drawResistorPreview(SDL_Renderer* renderer, int x1, int y1, int x2, int y2) {
    const int segments = 5;
    const int amplitude = 10;

    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = sqrt(dx*dx + dy*dy);

    dx /= length;
    dy /= length;
    float px = -dy;
    float py = dx;

    SDL_Point points[segments + 1];
    for (int i = 0; i <= segments; i++) {
        float t = (float)i / segments;
        float x = x1 + t * (x2 - x1);
        float y = y1 + t * (y2 - y1);

        float offset = (i % 2) ? amplitude : -amplitude;
        points[i].x = x + px * offset;
        points[i].y = y + py * offset;
    }

    SDL_SetRenderDrawColor(renderer, GREEN.r, GREEN.g, GREEN.b, 255);
    SDL_RenderDrawLines(renderer, points, segments + 1);
}

void drawCapacitorPreview(SDL_Renderer* renderer, int x1, int y1, int x2, int y2) {
    const int gap = 12;

    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = sqrt(dx*dx + dy*dy);
    dx /= length;
    dy /= length;
    float px = -dy;
    float py = dx;

    SDL_Point plate1[2], plate2[2];
    plate1[0].x = x1 + dx * (length/2 - gap) + px * gap/2;
    plate1[0].y = y1 + dy * (length/2 - gap) + py * gap/2;
    plate1[1].x = x1 + dx * (length/2 - gap) - px * gap/2;
    plate1[1].y = y1 + dy * (length/2 - gap) - py * gap/2;

    plate2[0].x = x1 + dx * (length/2 + gap) + px * gap/2;
    plate2[0].y = y1 + dy * (length/2 + gap) + py * gap/2;
    plate2[1].x = x1 + dx * (length/2 + gap) - px * gap/2;
    plate2[1].y = y1 + dy * (length/2 + gap) - py * gap/2;

    SDL_SetRenderDrawColor(renderer, GREEN.r, GREEN.g, GREEN.b, 255);
    SDL_RenderDrawLine(renderer, x1, y1, plate1[0].x, plate1[0].y);
    SDL_RenderDrawLine(renderer, plate2[1].x, plate2[1].y, x2, y2);
    SDL_RenderDrawLine(renderer, plate1[0].x, plate1[0].y, plate1[1].x, plate1[1].y);
    SDL_RenderDrawLine(renderer, plate2[0].x, plate2[0].y, plate2[1].x, plate2[1].y);
}

void drawInductorPreview(SDL_Renderer* renderer, int x1, int y1, int x2, int y2) {
    const int loops = 3;
    const int radius = 8;

    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = sqrt(dx*dx + dy*dy);
    dx /= length;
    dy /= length;
    float px = -dy;
    float py = dx;

    float segmentLength = length / (loops * 2);
    SDL_Point prevPoint = {x1, y1};

    for (int i = 0; i < loops; i++) {
        float centerX = x1 + dx * (i * 2 + 1) * segmentLength;
        float centerY = y1 + dy * (i * 2 + 1) * segmentLength;

        for (int angle = -90; angle <= 90; angle += 5) {
            float rad = angle * M_PI / 180.0f;
            int x = centerX + px * radius * cos(rad);
            int y = centerY + py * radius * sin(rad);

            if (angle == -90) {
                prevPoint = {x, y};
            } else {
                SDL_RenderDrawLine(renderer, prevPoint.x, prevPoint.y, x, y);
                prevPoint = {x, y};
            }
        }
    }

    SDL_RenderDrawLine(renderer, prevPoint.x, prevPoint.y, x2, y2);
}

void drawVoltageSourcePreview(SDL_Renderer* renderer, int x1, int y1, int x2, int y2) {
    const int radius = 12;
    int centerX = (x1 + x2) / 2;
    int centerY = (y1 + y2) / 2;

    SDL_SetRenderDrawColor(renderer, GREEN.r, GREEN.g, GREEN.b, 255);
    SDL_RenderDrawLine(renderer, x1, y1, centerX - radius, centerY);
    SDL_RenderDrawLine(renderer, centerX + radius, centerY, x2, y2);

    for (int angle = 0; angle < 360; angle += 10) {
        float rad = angle * M_PI / 180.0f;
        int x = centerX + radius * cos(rad);
        int y = centerY + radius * sin(rad);
        SDL_RenderDrawPoint(renderer, x, y);
    }

    SDL_RenderDrawLine(renderer, centerX-5, centerY, centerX+5, centerY);
    SDL_RenderDrawLine(renderer, centerX, centerY-5, centerX, centerY+5);
}

void drawCurrentSourcePreview(SDL_Renderer* renderer, int x1, int y1, int x2, int y2) {
    const int radius = 12;
    int centerX = (x1 + x2) / 2;
    int centerY = (y1 + y2) / 2;

    SDL_SetRenderDrawColor(renderer, GREEN.r, GREEN.g, GREEN.b, 255);
    SDL_RenderDrawLine(renderer, x1, y1, centerX - radius, centerY);
    SDL_RenderDrawLine(renderer, centerX + radius, centerY, x2, y2);

    for (int angle = 0; angle < 360; angle += 10) {
        float rad = angle * M_PI / 180.0f;
        int x = centerX + radius * cos(rad);
        int y = centerY + radius * sin(rad);
        SDL_RenderDrawPoint(renderer, x, y);
    }

    SDL_RenderDrawLine(renderer, centerX, centerY-8, centerX, centerY+8);
    SDL_RenderDrawLine(renderer, centerX, centerY+8, centerX-5, centerY+3);
    SDL_RenderDrawLine(renderer, centerX, centerY+8, centerX+5, centerY+3);
}

void drawDiodePreview(SDL_Renderer* renderer, int x1, int y1, int x2, int y2) {
    const int triangleSize = 15;

    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = sqrt(dx*dx + dy*dy);
    dx /= length;
    dy /= length;
    float px = -dy;
    float py = dx;

    int midX = (x1 + x2) / 2;
    int midY = (y1 + y2) / 2;

    SDL_SetRenderDrawColor(renderer, GREEN.r, GREEN.g, GREEN.b, 255);
    SDL_RenderDrawLine(renderer, x1, y1, midX - dx * triangleSize, midY - dy * triangleSize);
    SDL_RenderDrawLine(renderer, midX + dx * triangleSize, midY + dy * triangleSize, x2, y2);

    SDL_Point triangle[4];
    triangle[0] = {int(midX - dx * triangleSize), int(midY - dy * triangleSize)};
    triangle[1] = {int(midX + px * triangleSize), int(midY + py * triangleSize)};
    triangle[2] = {int(midX - px * triangleSize), int(midY - py * triangleSize)};
    triangle[3] = triangle[0];

    SDL_RenderDrawLines(renderer, triangle, 4);

    SDL_RenderDrawLine(renderer,
                       midX + dx * triangleSize/2 + px * triangleSize/2,
                       midY + dy * triangleSize/2 + py * triangleSize/2,
                       midX + dx * triangleSize/2 - px * triangleSize/2,
                       midY + dy * triangleSize/2 - py * triangleSize/2);
}

void drawGroundPreview(SDL_Renderer* renderer, int x, int y) {
    const int size = 15;

    SDL_SetRenderDrawColor(renderer, GREEN.r, GREEN.g, GREEN.b, 255);
    SDL_RenderDrawLine(renderer, x, y, x, y + size);

    SDL_RenderDrawLine(renderer, x - size, y + size, x + size, y + size);
    SDL_RenderDrawLine(renderer, x - size/2, y + size*1.5, x + size/2, y + size*1.5);
    SDL_RenderDrawLine(renderer, x - size/4, y + size*2, x + size/4, y + size*2);
}

void drawVCVSPreview(SDL_Renderer* renderer, int x1, int y1, int x2, int y2) {
    const int size = 15;
    int centerX = (x1 + x2) / 2;
    int centerY = (y1 + y2) / 2;

    // Draw diamond
    SDL_Point diamond[5] = {
            {centerX, centerY - size},
            {centerX + size, centerY},
            {centerX, centerY + size},
            {centerX - size, centerY},
            {centerX, centerY - size}
    };

    SDL_SetRenderDrawColor(renderer, GREEN.r, GREEN.g, GREEN.b, 255);
    SDL_RenderDrawLines(renderer, diamond, 5);

    // Draw leads
    SDL_RenderDrawLine(renderer, x1, y1, centerX, centerY - size);
    SDL_RenderDrawLine(renderer, x2, y2, centerX, centerY + size);

    // Draw control path indicator (simplified for preview)
    SDL_RenderDrawLine(renderer, centerX + size, centerY, centerX + size*2, centerY);
}

void drawVCCSPreview(SDL_Renderer* renderer, int x1, int y1, int x2, int y2) {
    const int size = 15;
    int centerX = (x1 + x2) / 2;
    int centerY = (y1 + y2) / 2;

    // Draw diamond
    SDL_Point diamond[5] = {
            {centerX, centerY - size},
            {centerX + size, centerY},
            {centerX, centerY + size},
            {centerX - size, centerY},
            {centerX, centerY - size}
    };

    SDL_SetRenderDrawColor(renderer, GREEN.r, GREEN.g, GREEN.b, 255);
    SDL_RenderDrawLines(renderer, diamond, 5);

    // Draw arrow inside diamond
    SDL_RenderDrawLine(renderer, centerX, centerY - size/2, centerX, centerY + size/2);
    SDL_RenderDrawLine(renderer, centerX, centerY + size/2, centerX - size/3, centerY + size/4);
    SDL_RenderDrawLine(renderer, centerX, centerY + size/2, centerX + size/3, centerY + size/4);

    // Draw leads
    SDL_RenderDrawLine(renderer, x1, y1, centerX - size, centerY);
    SDL_RenderDrawLine(renderer, x2, y2, centerX + size, centerY);

    // Draw control path indicator (simplified for preview)
    SDL_RenderDrawLine(renderer, centerX + size, centerY, centerX + size*2, centerY);
}

void drawCCVSPreview(SDL_Renderer* renderer, int x1, int y1, int x2, int y2) {
    const int size = 15;
    int centerX = (x1 + x2) / 2;
    int centerY = (y1 + y2) / 2;

    // Draw diamond
    SDL_Point diamond[5] = {
            {centerX, centerY - size},
            {centerX + size, centerY},
            {centerX, centerY + size},
            {centerX - size, centerY},
            {centerX, centerY - size}
    };

    SDL_SetRenderDrawColor(renderer, GREEN.r, GREEN.g, GREEN.b, 255);
    SDL_RenderDrawLines(renderer, diamond, 5);

    // Draw leads
    SDL_RenderDrawLine(renderer, x1, y1, centerX, centerY - size);
    SDL_RenderDrawLine(renderer, x2, y2, centerX, centerY + size);

    // Draw control arrow
    SDL_Point controlPoint = {centerX + size*2, centerY};
    SDL_RenderDrawLine(renderer, centerX + size, centerY, controlPoint.x, controlPoint.y);
    SDL_RenderDrawLine(renderer, controlPoint.x, controlPoint.y, controlPoint.x - 5, controlPoint.y - 5);
    SDL_RenderDrawLine(renderer, controlPoint.x, controlPoint.y, controlPoint.x - 5, controlPoint.y + 5);
}

void drawCCCSPreview(SDL_Renderer* renderer, int x1, int y1, int x2, int y2) {
    const int size = 15;
    int centerX = (x1 + x2) / 2;
    int centerY = (y1 + y2) / 2;

    // Draw diamond
    SDL_Point diamond[5] = {
            {centerX, centerY - size},
            {centerX + size, centerY},
            {centerX, centerY + size},
            {centerX - size, centerY},
            {centerX, centerY - size}
    };

    SDL_SetRenderDrawColor(renderer, GREEN.r, GREEN.g, GREEN.b, 255);
    SDL_RenderDrawLines(renderer, diamond, 5);

    // Draw arrow inside diamond
    SDL_RenderDrawLine(renderer, centerX, centerY - size/2, centerX, centerY + size/2);
    SDL_RenderDrawLine(renderer, centerX, centerY + size/2, centerX - size/3, centerY + size/4);
    SDL_RenderDrawLine(renderer, centerX, centerY + size/2, centerX + size/3, centerY + size/4);

    // Draw leads
    SDL_RenderDrawLine(renderer, x1, y1, centerX - size, centerY);
    SDL_RenderDrawLine(renderer, x2, y2, centerX + size, centerY);

    // Draw control arrow
    SDL_Point controlPoint = {centerX + size*2, centerY};
    SDL_RenderDrawLine(renderer, centerX + size, centerY, controlPoint.x, controlPoint.y);
    SDL_RenderDrawLine(renderer, controlPoint.x, controlPoint.y, controlPoint.x - 5, controlPoint.y - 5);
    SDL_RenderDrawLine(renderer, controlPoint.x, controlPoint.y, controlPoint.x - 5, controlPoint.y + 5);
}

int findOrCreateNodeAt(int x, int y) {
    const int NODE_PROXIMITY_THRESHOLD = 15;

    for (const auto& node : nodePositions) {
        int dx = x - node.second.x;
        int dy = y - node.second.y;
        if (dx*dx + dy*dy < NODE_PROXIMITY_THRESHOLD*NODE_PROXIMITY_THRESHOLD) {
            return node.first;
        }
    }

    int newNode = nextNodeNumber++;
    nodePositions[newNode] = {x, y};
    reverseNodeMap[newNode] = to_string(newNode);
    nodeMap[to_string(newNode)] = newNode;
    return newNode;
}

void calculateNodePositions(const Circuit& circuit) {
    nodePositions.clear();

    nodePositions[0] = {circuitArea.x + circuitArea.w/2, circuitArea.y + circuitArea.h - 50};

    int x = circuitArea.x + 100;
    int y = circuitArea.y + 100;
    int nodesPerRow = 5;

    for (int i = 1; i < circuit.nextNodeNumber; i++) {
        nodePositions[i] = {x, y};
        x += 150;
        if (i % nodesPerRow == 0) {
            x = circuitArea.x + 100;
            y += 100;
        }
    }
}

void handleComponentSelection(const Circuit* circuit, int x, int y) {
    selectedComponent = nullptr;

    for (const auto& comp : circuit->getComponents()) {
        SDL_Rect bounds = comp->getBoundingBox(nodePositions);
        if (x >= bounds.x && x <= bounds.x + bounds.w &&
            y >= bounds.y && y <= bounds.y + bounds.h) {
            selectedComponent = comp;
            break;
        }
    }
}

void handleComponentPlacement(Circuit* circuit, int x, int y) {
    static int compCount = 1;

    if (!isPlacingComponent && currentPlacementMode != PLACE_NONE) {
        placementStartPoint = {x, y};
        isPlacingComponent = true;
        return;
    }

    if (isPlacingComponent) {
        if (currentPlacementMode == PLACE_WIRE) {
            int node1 = findOrCreateNodeAt(placementStartPoint.x, placementStartPoint.y);
            int node2 = findOrCreateNodeAt(x, y);

            if (node1 != node2) {
                saveUndoState(circuit);
                circuit->addWire(node1, node2);
                cout << "Created wire between node " << node1 << " and " << node2 << endl;
            }
        }
        else {
            string name;
            Component* newComp = nullptr;
            double value = valueBox.text.empty() ? 0.0 : parseSpiceValue(valueBox.text);

            int node1 = findOrCreateNodeAt(placementStartPoint.x, placementStartPoint.y);
            int node2 = (currentPlacementMode == PLACE_GROUND) ? 0 : findOrCreateNodeAt(x, y);

            if (node1 != node2 || currentPlacementMode == PLACE_GROUND) {
                string node1Name = getNodeName(node1);
                string node2Name = getNodeName(node2);

                switch(currentPlacementMode) {
                    case PLACE_RESISTOR:
                        name = "R" + to_string(compCount++);
                        if (value <= 0) value = 1000.0;
                        newComp = new Resistor(name, node1, node2, value);
                        newComp->nodeName1 = node1Name;
                        newComp->nodeName2 = node2Name;
                        break;
                    case PLACE_CAPACITOR:
                        name = "C" + to_string(compCount++);
                        if (value <= 0) value = 1e-6;
                        newComp = new Capacitor(name, node1, node2, value);
                        newComp->nodeName1 = node1Name;
                        newComp->nodeName2 = node2Name;
                        break;
                    case PLACE_INDUCTOR:
                        name = "L" + to_string(compCount++);
                        if (value <= 0) value = 1e-3;
                        newComp = new Inductor(name, node1, node2, value);
                        newComp->nodeName1 = node1Name;
                        newComp->nodeName2 = node2Name;
                        break;
                    case PLACE_VOLTAGE_SOURCE:
                        name = "V" + to_string(compCount++);
                        if (value <= 0) value = 5.0;
                        newComp = new VoltageSource(name, node1, node2, value);
                        newComp->nodeName1 = node1Name;
                        newComp->nodeName2 = node2Name;
                        break;
                    case PLACE_CURRENT_SOURCE:
                        name = "I" + to_string(compCount++);
                        if (value <= 0) value = 0.1;
                        newComp = new CurrentSource(name, node1, node2, value);
                        newComp->nodeName1 = node1Name;
                        newComp->nodeName2 = node2Name;
                        break;
                    case PLACE_DIODE:
                        name = "D" + to_string(compCount++);
                        newComp = new Diode(name, node1, node2);
                        newComp->nodeName1 = node1Name;
                        newComp->nodeName2 = node2Name;
                        break;
                    case PLACE_GROUND:
                        name = "GND" + to_string(compCount++);
                        newComp = new Ground(name, node1);
                        newComp->nodeName1 = node1Name;
                        newComp->nodeName2 = node2Name;
                        break;
                    case PLACE_VCVS: {
                        string cn1 = "1";
                        string cn2 = "2";
                        double gain = valueBox.text.empty() ? 1.0 : parseSpiceValue(valueBox.text);
                        name = "E" + to_string(compCount++);
                        int ctrlNode1 = getOrCreateNode(cn1);
                        int ctrlNode2 = getOrCreateNode(cn2);
                        newComp = new VCVS(name, node1, node2, ctrlNode1, ctrlNode2, gain);
                        break;
                    }
                    case PLACE_VCCS: {
                        string cn1 = "1";
                        string cn2 = "2";
                        double gm = valueBox.text.empty() ? 0.1 : parseSpiceValue(valueBox.text);
                        name = "G" + to_string(compCount++);
                        int ctrlNode1 = getOrCreateNode(cn1);
                        int ctrlNode2 = getOrCreateNode(cn2);
                        newComp = new VCCS(name, node1, node2, ctrlNode1, ctrlNode2, gm);
                        break;
                    }
                    case PLACE_CCVS: {
                        string vsName = "CCVS"; // Using node1Box for controlling source name
                        double gain = valueBox.text.empty() ? 1.0 : parseSpiceValue(valueBox.text);
                        name = "H" + to_string(compCount++);
                        newComp = new CCVS(name, node1, node2, vsName, gain);
                        break;
                    }
                    case PLACE_CCCS: {
                        string vsName = "CCCS"; // Using node1Box for controlling source name
                        double gain = valueBox.text.empty() ? 1.0 : parseSpiceValue(valueBox.text);
                        name = "F" + to_string(compCount++);
                        newComp = new CCCS(name, node1, node2, vsName, gain);
                        break;
                    }
                    default:
                        break;
                }

                if (newComp) {
                    saveUndoState(circuit);
                    circuit->addComponent(newComp);
                    cout << "Added " << newComp->getType() << " " << name
                         << " between nodes " << node1 << " and " << node2 << endl;
                }
            }
        }

        isPlacingComponent = false;
        currentPlacementMode = PLACE_NONE;
    }
}

void drawCircuit(const Circuit& circuit, SDL_Renderer* renderer) {
    for (const auto& wire : circuit.getWires()) {
        auto p1 = nodePositions.at(wire.first);
        auto p2 = nodePositions.at(wire.second);
        drawWire(renderer, p1.x, p1.y, p2.x, p2.y, currentTheme.text);
    }

    for (const auto& comp : circuit.getComponents()) {
        comp->render(renderer, nodePositions);
    }

    for (const auto& node : nodePositions) {
        SDL_Rect nodeRect = {node.second.x - 5, node.second.y - 5, 10, 10};
        SDL_SetRenderDrawColor(renderer, BLUE.r, BLUE.g, BLUE.b, 255);
        SDL_RenderFillRect(renderer, &nodeRect);

        string label = (node.first == 0) ? "GND" : to_string(node.first);
        renderText(label, node.second.x + 12, node.second.y - 8, currentTheme.text);
    }

    if (isPlacingComponent) {
        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);

        switch(currentPlacementMode) {
            case PLACE_RESISTOR:
                drawResistorPreview(renderer, placementStartPoint.x, placementStartPoint.y, mouseX, mouseY);
                break;
            case PLACE_CAPACITOR:
                drawCapacitorPreview(renderer, placementStartPoint.x, placementStartPoint.y, mouseX, mouseY);
                break;
            case PLACE_INDUCTOR:
                drawInductorPreview(renderer, placementStartPoint.x, placementStartPoint.y, mouseX, mouseY);
                break;
            case PLACE_VOLTAGE_SOURCE:
                drawVoltageSourcePreview(renderer, placementStartPoint.x, placementStartPoint.y, mouseX, mouseY);
                break;
            case PLACE_CURRENT_SOURCE:
                drawCurrentSourcePreview(renderer, placementStartPoint.x, placementStartPoint.y, mouseX, mouseY);
                break;
            case PLACE_DIODE:
                drawDiodePreview(renderer, placementStartPoint.x, placementStartPoint.y, mouseX, mouseY);
                break;
            case PLACE_GROUND:
                drawGroundPreview(renderer, mouseX, mouseY);
                break;
            case PLACE_WIRE:
                drawWire(renderer, placementStartPoint.x, placementStartPoint.y, mouseX, mouseY, GREEN);
                break;
            default:
                break;
        }
    }
}

void handleComponentLibraryClick(int x, int y) {
    if (isMouseOver(passiveBtn.rect, x, y)) {
        showPassives = !showPassives;
        showSources = false;
        showSemis = false;
        showDependents = false;
    }
    else if (isMouseOver(sourcesBtn.rect, x, y)) {
        showPassives = false;
        showSources = !showSources;
        showSemis = false;
        showDependents = false;
    }
    else if (isMouseOver(semiBtn.rect, x, y)) {
        showPassives = false;
        showSources = false;
        showSemis = !showSemis;
        showDependents = false;
    }
    else if (isMouseOver(depBtn.rect, x, y)) {
        showPassives = false;
        showSources = false;
        showSemis = false;
        showDependents = !showDependents;
    }
    else if (showPassives) {
        if (isMouseOver(resBtn.rect, x, y)) {
            currentPlacementMode = PLACE_RESISTOR;
        }
        else if (isMouseOver(capBtn.rect, x, y)) {
            currentPlacementMode = PLACE_CAPACITOR;
        }
        else if (isMouseOver(indBtn.rect, x, y)) {
            currentPlacementMode = PLACE_INDUCTOR;
        }
    }
    else if (showSources) {
        if (isMouseOver(vSrcBtn.rect, x, y)) {
            currentPlacementMode = PLACE_VOLTAGE_SOURCE;
        }
        else if (isMouseOver(iSrcBtn.rect, x, y)) {
            currentPlacementMode = PLACE_CURRENT_SOURCE;
        }
        else if (isMouseOver(gndBtn.rect, x, y)) {
            currentPlacementMode = PLACE_GROUND;
        }
    }
    else if (showSemis && isMouseOver(diodeBtn.rect, x, y)) {
        currentPlacementMode = PLACE_DIODE;
    }
    else if (showDependents) {
        if (isMouseOver(vcvsBtn.rect, x, y)) {
            currentPlacementMode = PLACE_VCVS;
        }
        else if (isMouseOver(vccsBtn.rect, x, y)) {
            currentPlacementMode = PLACE_VCCS;
        }
        else if (isMouseOver(ccvsBtn.rect, x, y)) {
            currentPlacementMode = PLACE_CCVS;
        }
        else if (isMouseOver(cccsBtn.rect, x, y)) {
            currentPlacementMode = PLACE_CCCS;
        }
    }
}

void renderComponentLibrary(SDL_Renderer* renderer) {
    SDL_Rect libWindow = {850, 50, 380, 600};
    SDL_SetRenderDrawColor(renderer,
                           currentTheme.circuitBg.r,
                           currentTheme.circuitBg.g,
                           currentTheme.circuitBg.b,
                           255);
    SDL_RenderFillRect(renderer, &libWindow);
    SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 255);
    SDL_RenderDrawRect(renderer, &libWindow);

    renderText("Component Library", libWindow.x + 20, libWindow.y + 10, currentTheme.text);

    passiveBtn = {870, 50, 120, 40, "Passives", currentTheme.button};
    sourcesBtn = {870, 100, 120, 40, "Sources", currentTheme.button};
    semiBtn = {870, 150, 120, 40, "Semiconductors", currentTheme.button};
    depBtn = {870, 200, 120, 40, "Dependent", currentTheme.button};

    resBtn = {1000, 100, 120, 40, "Resistor", currentTheme.button};
    capBtn = {1000, 150, 120, 40, "Capacitor", currentTheme.button};
    indBtn = {1000, 200, 120, 40, "Inductor", currentTheme.button};
    diodeBtn = {1000, 250, 120, 40, "Diode", currentTheme.button};
    vSrcBtn = {1000, 100, 120, 40, "V Source", currentTheme.button};
    iSrcBtn = {1000, 150, 120, 40, "I Source", currentTheme.button};
    gndBtn = {1000, 200, 120, 40, "Ground", currentTheme.button};
    vcvsBtn = {1000, 250, 120, 40, "VCVS", currentTheme.button};
    vccsBtn = {1000, 300, 120, 40, "VCCS", currentTheme.button};
    ccvsBtn = {1000, 350, 120, 40, "CCVS", currentTheme.button};
    cccsBtn = {1000, 400, 120, 40, "CCCS", currentTheme.button};

    renderButton(passiveBtn);
    renderButton(sourcesBtn);
    renderButton(semiBtn);
    renderButton(depBtn);

    if (showPassives) {
        renderButton(resBtn);
        renderButton(capBtn);
        renderButton(indBtn);
    }

    if (showSources) {
        renderButton(vSrcBtn);
        renderButton(iSrcBtn);
        renderButton(gndBtn);
    }

    if (showSemis) {
        renderButton(diodeBtn);
    }

    if (showDependents) {
        renderButton(vcvsBtn);
        renderButton(vccsBtn);
        renderButton(ccvsBtn);
        renderButton(cccsBtn);
    }

    Button closeBtn = {libWindow.x + libWindow.w - 40, libWindow.y + 10, 30, 30, "X", RED};
    renderButton(closeBtn);
}

void renderStatus(SDL_Renderer* renderer) {
    if (currentPlacementMode != PLACE_NONE) {
        string modeName;
        switch(currentPlacementMode) {
            case PLACE_RESISTOR: modeName = "Resistor (R)"; break;
            case PLACE_CAPACITOR: modeName = "Capacitor (C)"; break;
            case PLACE_INDUCTOR: modeName = "Inductor (L)"; break;
            case PLACE_VOLTAGE_SOURCE: modeName = "Voltage Source (V)"; break;
            case PLACE_CURRENT_SOURCE: modeName = "Current Source (I)"; break;
            case PLACE_DIODE: modeName = "Diode (Shift+D)"; break;
            case PLACE_GROUND: modeName = "Ground (Shift+G)"; break;
            case PLACE_WIRE: modeName = "Wire (W)"; break;
            case PLACE_VCVS: modeName = "VCVS"; break;
            case PLACE_VCCS: modeName = "VCCS"; break;
            case PLACE_CCVS: modeName = "CCVS"; break;
            case PLACE_CCCS: modeName = "CCCS"; break;
            default: modeName = "Unknown"; break;
        }

        string message = "Placing " + modeName + " - Click first connection point";
        if (isPlacingComponent) {
            message = "Placing " + modeName + " - Click second connection point (ESC to cancel)";
        }

        renderText(message, 10, SCREEN_HEIGHT - 30, currentTheme.text);
    }
}

void renderShortcutHelp(SDL_Renderer* renderer) {
    int y = 50;
    renderText("Keyboard Shortcuts:", 10, y, currentTheme.text); y += 30;
    renderText("R - Place Resistor", 20, y, currentTheme.text); y += 25;
    renderText("C - Place Capacitor", 20, y, currentTheme.text); y += 25;
    renderText("L - Place Inductor", 20, y, currentTheme.text); y += 25;
    renderText("V - Place Voltage Source", 20, y, currentTheme.text); y += 25;
    renderText("I - Place Current Source", 20, y, currentTheme.text); y += 25;
    renderText("Shift+D - Place Diode", 20, y, currentTheme.text); y += 25;
    renderText("Shift+G - Place Ground", 20, y, currentTheme.text); y += 25;
    renderText("W - Place Wire", 20, y, currentTheme.text); y += 25;
    renderText("ESC - Cancel placement", 20, y, currentTheme.text); y += 25;
}

void handleSaveButton(Circuit* circuit, string& currentCircuitFile) {
    if (currentCircuitFile.empty()) {
        // Show save dialog
        SaveAsDialog = true;
    } else {
        circuit->saveToFile(ensureExtension(currentCircuitFile));
    }
}

void handleLoadButton(Circuit*& circuit, string& currentCircuitFile, const string& filename) {
    Circuit* newCircuit = new Circuit();
    if (newCircuit->loadFromFile(ensureExtension(filename))) {
        delete circuit;
        circuit = newCircuit;
        currentCircuitFile = filename;
        calculateNodePositions(*circuit);
    } else {
        delete newCircuit;
    }
}

int main(int argc, char* argv[]) {
    cursors.push_back({-1, -1, 0.0, 0.0, false, {204, 153, 0, 128}, false});
    changeToPreviousDirectory();
    renderStatus(renderer);
    double timeZoom = 1.0;
    double valueZoom = 1.0;
    double timePan = 0.0;
    double valuePan = 0.0;

    string currentCircuitFile = "";
    Circuit* circuit = new Circuit();

    if (!initSDL()) {
        return 1;
    }

    TextBox* activeTextBox = nullptr;
    vector<string> txtFiles = listTxtFiles(".");

    nodePositions[0] = {100, 500};
    reverseNodeMap[0] = "GND";
    nextNodeNumber = 1;

    while (running) {
        if (undoStack.empty()) {
            saveUndoState(circuit);
        }

        while (SDL_PollEvent(&event)) {
            if (!Vtimes.empty() && !voltages.empty()) {
                double minTime = *min_element(Vtimes.begin(), Vtimes.end());
                double maxTime = *max_element(Vtimes.begin(), Vtimes.end());
                double minValue = *min_element(voltages.begin(), voltages.end());
                double maxValue = *max_element(voltages.begin(), voltages.end());

                handleCursorInteraction(event, plotArea, Vtimes, voltages,
                                        minTime, maxTime, minValue, maxValue);
            }
            if (event.type == SDL_QUIT) {
                running = false;
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN) {
                int x, y;
                SDL_GetMouseState(&x, &y);

                if (isMouseOver(darkModeBtn.rect, x, y)) {
                    darkMode = !darkMode;
                    currentTheme = darkMode ? darkTheme : lightTheme;
                    darkModeBtn.text = darkMode ? "Light Mode" : "Dark Mode";
                    continue;
                }
                if (showCursor && x >= plotArea.x && x <= plotArea.x + plotArea.w &&
                    y >= plotArea.y && y <= plotArea.y + plotArea.h) {
                    cursorX = x;
                    cursorY = y;
                }

                if (y <= 40) {
                    if (x >= 10 && x <= 90) {
                        FileMenu = !FileMenu;
                        EditMenu = false;
                        ComponentLibrary = false;
                        AnalysisSettings = false;
                        continue;
                    }
                    else if (x >= 100 && x <= 180) {
                        EditMenu = !EditMenu;
                        ComponentLibrary = false;
                        FileMenu = false;
                        AnalysisSettings = false;
                        continue;
                    }
                    else if (x >= 280 && x <= 360) {
                        AnalysisSettings = !AnalysisSettings;
                        FileMenu = false;
                        EditMenu = false;
                        ComponentLibrary = false;
                        continue;
                    }
                    else if (x >= 370 && x <= 450) {
                        ComponentLibrary = !ComponentLibrary;
                        FileMenu = false;
                        EditMenu = false;
                        AnalysisSettings = false;
                        continue;
                    }
                }

                if (x >= circuitArea.x && x <= circuitArea.x + circuitArea.w &&
                    y >= circuitArea.y && y <= circuitArea.y + circuitArea.h) {

                    if (currentPlacementMode != PLACE_NONE) {
                        handleComponentPlacement(circuit, x, y);
                    }
                    else {
                        handleComponentSelection(circuit, x, y);
                    }
                }

                if (ComponentLibrary) {
                    handleComponentLibraryClick(x, y);

                    SDL_Rect libWindow = {850, 50, 380, 600};
                    Button closeBtn = {libWindow.x + libWindow.w - 40, libWindow.y + 10, 30, 30, "X", RED};
                    if (isMouseOver(closeBtn.rect, x, y)) {
                        ComponentLibrary = false;
                    }
                    continue;
                }
                if (x >= plotArea.x && x <= plotArea.x + plotArea.w &&
                    y >= plotArea.y && y <= plotArea.y + plotArea.h) {
                    if (showLegend && x < plotArea.x + 150) {
                        int legendIndex = (y - plotArea.y - 10) / 20;
                        if (legendIndex >= 0 && legendIndex < circuit->components.size()) {
                            selectedSignalIndex = legendIndex;
                        }
                    }
                }

                if (FileMenu) {
                    if (x >= fileMenuRect.x + 10 && x <= fileMenuRect.x + 130) {
                        if (y >= fileMenuRect.y + 40 && y <= fileMenuRect.y + 70) {
                            resetGlobalState();
                            delete circuit;
                            circuit = new Circuit();
                            currentCircuitFile = "";
                            FileMenu = false;
                        }
                        else if (y >= fileMenuRect.y + 80 && y <= fileMenuRect.y + 110) {
                            FileDialog = true;
                            txtFiles = listTxtFiles(".");
                        }
                        else if (y >= fileMenuRect.y + 120 && y <= fileMenuRect.y + 150) {
                            if (!currentCircuitFile.empty()) {
                                saveCircuitToFile(*circuit, currentCircuitFile);
                            }
                            else {
                                SaveAsDialog = true;
                            }
                            FileMenu = false;
                        }
                        else if (y >= fileMenuRect.y + 160 && y <= fileMenuRect.y + 190) {
                            SaveAsDialog = true;
                            FileMenu = false;
                        }
                        else if (y >= fileMenuRect.y + 200 && y <= fileMenuRect.y + 230) {
                            running = false;
                        }
                    }
                    continue;
                }

                if (EditMenu) {
                    if (x >= editMenuRect.x + 10 && x <= editMenuRect.x + 130) {
                        if (y >= editMenuRect.y + 40 && y <= editMenuRect.y + 70) {
                            if (!undoStack.empty()) {
                                undo(circuit);
                            }
                            EditMenu = false;
                        }
                        else if (y >= editMenuRect.y + 80 && y <= editMenuRect.y + 110) {
                            if (!redoStack.empty()) {
                                redo(circuit);
                            }
                            EditMenu = false;
                        }
                        else if (y >= editMenuRect.y + 200 && y <= editMenuRect.y + 230) {
                            if (selectedComponent) {
                                saveUndoState(circuit);
                                circuit->deleteComponent(selectedComponent->name);
                                selectedComponent = nullptr;
                                calculateNodePositions(*circuit);
                            }
                            EditMenu = false;
                        }
                        else if (y >= editMenuRect.y + 120 && y <= editMenuRect.y + 150) {
                            if (selectedComponent) {
                                copiedComponent = selectedComponent->getInfo();
                            }
                            EditMenu = false;
                        }
                        else if (y >= editMenuRect.y + 160 && y <= editMenuRect.y + 190) {
                            if (!copiedComponent.empty()) {
                                istringstream iss(copiedComponent);
                                string type, name, node1, node2, valueStr;
                                iss >> type >> name >> node1 >> node2 >> valueStr;

                                static int pasteCount = 1;
                                name = name + "_copy" + to_string(pasteCount++);

                                Component* newComp = nullptr;
                                if (type == "Resistor") {
                                    newComp = new Resistor(name, getOrCreateNode(node1), getOrCreateNode(node2), stod(valueStr));
                                }
                                if (type == "Capacitor") {
                                    newComp = new Capacitor(name, getOrCreateNode(node1), getOrCreateNode(node2), stod(valueStr));
                                }
                                if (type == "Inductor") {
                                    newComp = new Inductor(name, getOrCreateNode(node1), getOrCreateNode(node2), stod(valueStr));
                                }
                                if (type == "Voltage") {
                                    newComp = new VoltageSource(name, getOrCreateNode(node1), getOrCreateNode(node2), stod(valueStr));
                                }
                                if (type == "Current") {
                                    newComp = new CurrentSource(name, getOrCreateNode(node1), getOrCreateNode(node2), stod(valueStr));
                                }
                                if (type == "Ground") {
                                    newComp = new Ground(name, getOrCreateNode(node1));
                                }

                                if (newComp) {
                                    saveUndoState(circuit);
                                    circuit->addComponent(newComp);
                                    calculateNodePositions(*circuit);
                                }
                            }
                            EditMenu = false;
                        }
                        else if (y >= editMenuRect.y + 200 && y <= editMenuRect.y + 230) {
                            if (selectedComponent) {
                                saveUndoState(circuit);
                                circuit->deleteComponent(selectedComponent->name);
                                selectedComponent = nullptr;
                                calculateNodePositions(*circuit);
                            }
                            EditMenu = false;
                        }
                    }
                    continue;
                }

                if (AnalysisSettings) {
                    SDL_Rect analysisWindow = {250, 150, 350, 300};
                    if (x >= analysisWindow.x + 50 && x <= analysisWindow.x + 150 &&
                        y >= analysisWindow.y + 220 && y <= analysisWindow.y + 260) {
                        try {
                            if (hasDynamic) {
                                circuit->analyzeTransient(tStep,tStop);
                                circuit->printTransientResults(Vtimes, voltages, currents, circuit->listNodes().size() - 1);
                            }
                            else {
                                circuit->analyzeDC();
                            }
                            showVoltage = true;
                            showCurrent = false;
                            AnalysisSettings = false;
                        }
                        catch (const exception& e) {
                            cerr << "Analysis error: " << e.what() << endl;
                        }
                    }
                    else if (x >= analysisWindow.x + 200 && x <= analysisWindow.x + 300 &&
                             y >= analysisWindow.y + 220 && y <= analysisWindow.y + 260) {
                        AnalysisSettings = false;
                    }
                    continue;
                }

                if (FileDialog) {
                    SDL_Rect dialog = {200, 150, 400, 400};
                    if (x >= dialog.x + 100 && x <= dialog.x + 200 &&
                        y >= dialog.y + 330 && y <= dialog.y + 370) {
                        for (size_t i = 0; i < txtFiles.size(); i++) {
                            SDL_Rect fileRect = {dialog.x + 20, dialog.y + 60 + (int)i * 30, 360, 25};
                            if (x >= fileRect.x && x <= fileRect.x + fileRect.w &&
                                y >= fileRect.y && y <= fileRect.y + fileRect.h) {
                                resetGlobalState();
                                delete circuit;
                                circuit = new Circuit();
                                processCircuitFile(txtFiles[i], *circuit);
                                currentCircuitFile = txtFiles[i];
                                calculateNodePositions(*circuit);
                                FileDialog = false;
                                break;
                            }
                        }
                    }
                    else if (x >= dialog.x + 220 && x <= dialog.x + 320 &&
                             y >= dialog.y + 330 && y <= dialog.y + 370) {
                        FileDialog = false;
                    }
                    continue;
                }

                if (SaveAsDialog) {
                    SDL_Rect dialog = {250, 200, 300, 200};
                    if (x >= dialog.x + 50 && x <= dialog.x + 150 &&
                        y >= dialog.y + 120 && y <= dialog.y + 160) {
                        handleSaveButton(circuit, nameBox.text);
                        SaveAsDialog = false;
                    }
                    else if (x >= dialog.x + 170 && x <= dialog.x + 270 &&
                             y >= dialog.y + 120 && y <= dialog.y + 160) {
                        SaveAsDialog = false;
                    }
                    else if (x >= dialog.x + 100 && x <= dialog.x + 280 &&
                             y >= dialog.y + 60 && y <= dialog.y + 90) {
                        textInputActive = true;
                        activeTextBox = &nameBox;
                        inputText = nameBox.text;
                    }
                    continue;
                }

                if (x >= valueBox.rect.x && x <= valueBox.rect.x + valueBox.rect.w &&
                         y >= valueBox.rect.y && y <= valueBox.rect.y + valueBox.rect.h) {
                    textInputActive = true;
                    activeTextBox = &valueBox;
                    inputText = valueBox.text;
                }
                else {
                    textInputActive = false;
                    activeTextBox = nullptr;
                }
            }
            else if (event.type == SDL_KEYDOWN && textInputActive) {
                if (event.key.keysym.sym == SDLK_RETURN) {
                    textInputActive = false;
                    if (activeTextBox) {
                        activeTextBox->text = inputText;
                    }
                    else if (SaveAsDialog) {
                        nameBox.text = inputText;
                    }
                }
                else if (event.key.keysym.sym == SDLK_BACKSPACE && !inputText.empty()) {
                    inputText.pop_back();
                }
                else if (event.key.keysym.sym == SDLK_ESCAPE) {
                    textInputActive = false;
                    inputText = "";
                }




            }
            else if (event.type == SDL_MOUSEWHEEL) {
                if (SDL_GetModState() & KMOD_CTRL) {
                    if (event.wheel.y > 0) valueZoom *= 1.2;
                    else if (event.wheel.y < 0) valueZoom /= 1.2;
                } else {
                    if (event.wheel.y > 0) {
                        timeZoom *= 1.2;
                        valueZoom *= 1.2;
                    } else if (event.wheel.y < 0) {
                        timeZoom /= 1.2;
                        valueZoom /= 1.2;
                    }
                }
                timeZoom = max(0.1, min(timeZoom, 20.0));
                valueZoom = max(0.1, min(valueZoom, 20.0));
            }
            else if (event.type == SDL_MOUSEMOTION) {
                if (event.motion.state & SDL_BUTTON_MMASK) {
                    if (!Vtimes.empty() && !Vtimes.empty()) {
                        double timeRange = (Vtimes.back() - Vtimes.front()) / timeZoom;
                        double minVal = *min_element(voltages.begin(), voltages.end());
                        double maxVal = *max_element(voltages.begin(), voltages.end());
                        double valueRange = (maxVal - minVal) / valueZoom;

                        timePan += event.motion.xrel * timeRange / plotArea.w;
                        valuePan -= event.motion.yrel * valueRange / plotArea.h;
                    }
                }
            }
            else if (event.type == SDL_TEXTINPUT && textInputActive) {
                inputText += event.text.text;
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT) {
                int x, y;
                SDL_GetMouseState(&x, &y);

                handleComponentSelection(circuit, x, y);

                if (selectedComponent) {
                    SDL_Rect menu = {x, y, 100, 40};
                    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
                    SDL_RenderFillRect(renderer, &menu);

                    Button deleteBtn = {x, y, 100, 40, "Delete", RED};
                    renderButton(deleteBtn);
                    SDL_RenderPresent(renderer);

                    bool choiceMade = false;
                    while (!choiceMade) {
                        while (SDL_PollEvent(&event)) {
                            if (event.type == SDL_MOUSEBUTTONDOWN) {
                                if (isMouseOver(deleteBtn.rect, event.button.x, event.button.y)) {
                                    saveUndoState(circuit);
                                    circuit->deleteComponent(selectedComponent->name);
                                    selectedComponent = nullptr;
                                    calculateNodePositions(*circuit);
                                }
                                choiceMade = true;
                            }
                        }
                    }
                }
            }
            else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE && currentPlacementMode != PLACE_NONE) {
                    currentPlacementMode = PLACE_NONE;
                    isPlacingComponent = false;
                }
                else if (event.key.keysym.sym == SDLK_h) {
                    showShortcutHelp = !showShortcutHelp;
                }
                else if (event.key.keysym.sym == SDLK_DELETE) {
                    if (selectedComponent) {
                        saveUndoState(circuit);
                        circuit->deleteComponent(selectedComponent->name);
                        selectedComponent = nullptr;
                        calculateNodePositions(*circuit);
                    }
                }
                else if (event.key.keysym.mod & KMOD_SHIFT) {
                    switch (event.key.keysym.sym) {
                        case SDLK_d:
                            currentPlacementMode = PLACE_DIODE;
                            isPlacingComponent = false;
                            cout << "Diode placement mode activated" << endl;
                            break;
                        case SDLK_g:
                            currentPlacementMode = PLACE_GROUND;
                            isPlacingComponent = false;
                            cout << "Ground placement mode activated" << endl;
                            break;
                    }
                } else {
                    switch (event.key.keysym.sym) {
                        case SDLK_r:
                            currentPlacementMode = PLACE_RESISTOR;
                            isPlacingComponent = false;
                            cout << "Resistor placement mode activated" << endl;
                            break;
                        case SDLK_c:
                            currentPlacementMode = PLACE_CAPACITOR;
                            isPlacingComponent = false;
                            cout << "Capacitor placement mode activated" << endl;
                            break;
                        case SDLK_l:
                            currentPlacementMode = PLACE_INDUCTOR;
                            isPlacingComponent = false;
                            cout << "Inductor placement mode activated" << endl;
                            break;
                        case SDLK_v:
                            currentPlacementMode = PLACE_VOLTAGE_SOURCE;
                            isPlacingComponent = false;
                            cout << "Voltage source placement mode activated" << endl;
                            break;
                        case SDLK_i:
                            currentPlacementMode = PLACE_CURRENT_SOURCE;
                            isPlacingComponent = false;
                            cout << "Current source placement mode activated" << endl;
                            break;
                        case SDLK_w:
                            currentPlacementMode = PLACE_WIRE;
                            isPlacingComponent = false;
                            cout << "Wire placement mode activated" << endl;
                            break;
                    }
                }

            }
        }

        if (textInputActive) {
            if (activeTextBox) {
                activeTextBox->text = inputText;
            }
            else if (SaveAsDialog) {
                nameBox.text = inputText;
            }
        }

        SDL_SetRenderDrawColor(renderer, currentTheme.background.r, currentTheme.background.g, currentTheme.background.b, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, currentTheme.circuitBg.r, currentTheme.circuitBg.g, currentTheme.circuitBg.b, 255);
        SDL_RenderFillRect(renderer, &circuitArea);
        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 255);
        SDL_RenderDrawRect(renderer, &circuitArea);

        SDL_SetRenderDrawColor(renderer, currentTheme.plotBg.r, currentTheme.plotBg.g, currentTheme.plotBg.b, 255);
        SDL_RenderFillRect(renderer, &plotArea);
        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 255);
        SDL_RenderDrawRect(renderer, &plotArea);

        if (circuit) {
            drawCircuit(*circuit, renderer);
        }

        if (!Vtimes.empty()) {
            circuit->drawLTspiceStylePlot(renderer, plotArea);
        }

        drawToolbar(renderer);

        renderTextBox(valueBox);

        if (FileMenu) {
            showFileMenu(renderer, fileMenuRect);
        }

        if (EditMenu) {
            showEditMenu(renderer, editMenuRect);
        }

        if (ComponentLibrary) {
            renderComponentLibrary(renderer);
        }

        if (AnalysisSettings) {
            showAnalysisSettings(renderer);
        }

        if (FileDialog) {
            showFileDialog(renderer, txtFiles);
        }

        if (SaveAsDialog) {
            showSaveAsDialog(renderer, nameBox.text);
        }

        if (selectedComponent) {
            showComponentProperties(renderer, selectedComponent);
        }

        if (showShortcutHelp) {
            SDL_Rect helpBackground = {0, 0, 250, 250};
            SDL_SetRenderDrawColor(renderer, currentTheme.background.r, currentTheme.background.g, currentTheme.background.b, 200);
            SDL_RenderFillRect(renderer, &helpBackground);
            renderShortcutHelp(renderer);
        }

        if (currentPlacementMode != PLACE_NONE) {
            string modeName = "";
            switch(currentPlacementMode) {
                case PLACE_RESISTOR: modeName = "Resistor"; break;
                case PLACE_CAPACITOR: modeName = "Capacitor"; break;
                case PLACE_INDUCTOR: modeName = "Inductor"; break;
                case PLACE_VOLTAGE_SOURCE: modeName = "Voltage Source"; break;
                case PLACE_CURRENT_SOURCE: modeName = "Current Source"; break;
                case PLACE_DIODE: modeName = "Diode"; break;
                case PLACE_GROUND: modeName = "Ground"; break;
                case PLACE_VCCS: modeName = "VCCS"; break;
                case PLACE_VCVS: modeName = "VCVS"; break;
                case PLACE_CCCS: modeName = "CCCS"; break;
                case PLACE_CCVS: modeName = "CCVS"; break;
                default: break;
            }

            string message = "Placing " + modeName + " - Click first connection point";
            if (isPlacingComponent) {
                message = "Placing " + modeName + " - Click second connection point (ESC to cancel)";
            }

            renderText(message, 10, SCREEN_HEIGHT - 30, currentTheme.text);

            SDL_RenderPresent(renderer);
        }

        SDL_RenderPresent(renderer);
    }

    for (auto circuit : undoStack) {
        delete circuit;
    }
    for (auto circuit : redoStack) {
        delete circuit;
    }
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    delete circuit;
    return 0;
}