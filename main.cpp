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

using namespace std;

#ifdef _WIN32
#define GETCWD _getcwd
#define CHDIR _chdir
#else
#include <unistd.h>
#define GETCWD getcwd
#define CHDIR chdir
#define MAX_PATH PATH_MAX
#endif

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

double tStep = 0.001;
double tStop = 0.1;
std::map<int, SDL_Point> nodePositions;

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;

const SDL_Color WHITE = {255, 255, 255, 255};
const SDL_Color BLACK = {0, 0, 0, 255};
const SDL_Color RED = {220, 0, 0, 255};
const SDL_Color GREEN = {0, 200, 0, 255};
const SDL_Color BLUE = {0, 0, 220, 255};
const SDL_Color currentThemebutton = {200, 200, 200, 255};

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

TextBox node1Box = {{10, SCREEN_HEIGHT - 60, 100, 40}, "Node1", false};
TextBox node2Box = {{120, SCREEN_HEIGHT - 60, 100, 40}, "Node2", false};
TextBox valueBox = {{230, SCREEN_HEIGHT - 60, 100, 40}, "Value", false};
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
        {240, 240, 240, 255},  // background
        {0, 0, 0, 255},         // text
        {255, 255, 255, 255},   // circuitBg
        {255, 255, 255, 255},   // plotBg
        {200, 200, 200, 255},   // button
        {0, 0, 0, 255},         // buttonText
        {180, 180, 180, 255}    // toolbar
};

ThemeColors darkTheme = {
        {40, 40, 40, 255},      // background
        {220, 220, 220, 255},   // text
        {60, 60, 60, 255},      // circuitBg
        {60, 60, 60, 255},      // plotBg
        {80, 80, 80, 255},      // button
        {220, 220, 220, 255},   // buttonText
        {50, 50, 50, 255}       // toolbar
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
    PLACE_WIRE
};

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
        nodeMap[nodeName] = nextNodeNumber;
        reverseNodeMap[nextNodeNumber] = nodeName;
        return nextNodeNumber++;
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
    std::string nodeName1;
    std::string nodeName2;
    int node1, node2;
    double value;

    Component(ComponentType t, const string& n, int n1, int n2, double val)
            : type(t), name(n), node1(n1), node2(n2), value(val) {}

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
    virtual std::pair<int,int> getPosition() {
        return {posX, posY};
    }

    virtual std::string getType() = 0;

    virtual void render(SDL_Renderer* renderer, const map<int, SDL_Point>& nodePositions) const = 0;
    virtual SDL_Rect getBoundingBox(const map<int, SDL_Point>& nodePositions) const = 0;

};

class Circuit {
    vector<Component*> components;
    int maxNode;
    static double currentTimeStep;
    static double currentTime;
    std::map<std::string, int> nodeMap;          // Maps node names to numbers
    std::map<int, std::string> reverseNodeMap;   // Maps node numbers to names
    int nextNodeNumber = 1;                      // Next available node number

public:
    Circuit() : maxNode(0) {}

    ~Circuit() {
        for (auto comp : components) {
            delete comp;
        }
    }

    // Add or get a node number for a given name
    int getOrCreateNode(const std::string& name) {
        if (name == "GND" || name == "0") return 0;  // Ground is always node 0

        auto it = nodeMap.find(name);
        if (it != nodeMap.end()) {
            return it->second;
        }

        // Assign new node number
        int nodeNum = nextNodeNumber++;
        nodeMap[name] = nodeNum;
        reverseNodeMap[nodeNum] = name;
        return nodeNum;
    }

    // Get node name from number
    string getNodeName(int nodeNum) const {
        if (nodeNum == 0) return "GND";
        auto it = reverseNodeMap.find(nodeNum);
        if (it != reverseNodeMap.end()) {
            return it->second;
        }
        return "UNKNOWN";
    }

    // Get all nodes as name-number pairs
    const std::map<std::string, int>& getNodeMap() const {
        return nodeMap;
    }

    // Get all nodes as number-name pairs
    const std::map<int, std::string>& getReverseNodeMap() const {
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

    // Keep the old listComponents for text output if needed
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

        // Convert node names to numbers
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

    bool deleteComponent(const string& fullName) {
        if (fullName.empty()) return false;

        char typeChar = toupper(fullName[0]);
        string name = fullName;

        ComponentType type;
        switch (typeChar) {
            case 'R': type = RESISTOR; break;
            case 'C': type = CAPACITOR; break;
            case 'L': type = INDUCTOR; break;
            case 'V': type = VOLTAGE_SOURCE; break;
            case 'I': type = CURRENT_SOURCE; break;
            case 'D': type = DIODE; break;
            case 'G': type = GROUND; break;
            default: return false;
        }

        for (auto it = components.begin(); it != components.end(); ++it) {
            if ((*it)->type == type && (*it)->name == name) {
                delete *it;
                components.erase(it);
                return true;
            }
        }

        return false;
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

    /*vector<string> listComponents(ComponentType filterType = static_cast<ComponentType>(-1)) const {
        vector<string> result;
        for (const auto& comp : components) {
            if (filterType == static_cast<ComponentType>(-1) || comp->type == filterType) {
                result.push_back(comp->getInfo());
            }
        }
        return result;
    }*/

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

    static double getTimeStep() { return currentTimeStep; }
    static double getTime() { return currentTime; }
};

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

void renderText(const std::string& text, int x, int y,  SDL_Color color = currentTheme.text) {
    SDL_Surface* surface = TTF_RenderText_Solid(font, text.c_str(), color);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect rect = {x, y, surface->w, surface->h};
    SDL_RenderCopy(renderer, texture, nullptr, &rect);
    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
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
    if (box.text == "") {
        return;
    }
    renderText(box.text, box.rect.x + 5, box.rect.y + 5, currentTheme.text);
}

SDL_Point getNodePosition(int node) {
    auto it = nodePositions.find(node);
    if (it != nodePositions.end()) {
        return it->second;
    }
    return {0, 0}; // Default position if node not found
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

    void render(SDL_Renderer* renderer, const map<int, SDL_Point>& nodePositions) const {
        SDL_Point p1 = getNodePosition(node1);
        SDL_Point p2 = getNodePosition(node2);

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

        // Draw component name
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
        double geq = value / Circuit::getTimeStep();

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

        // Draw component name
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

        double dt = Circuit::getTimeStep();
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

        // Draw component name
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

        double expArg = std::min(vd / (n * Vt), 40.0);
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

        double expArg = std::min(lastVoltage / (n * Vt), 40.0);
        current = Is * (exp(expArg) - 1);
    }

    double getCurrent(const vector<double>& /*nodeVoltages*/) const override {
        return current;
    }

    string getInfo() const override {
        return "Diode " + name + " " + getNodeName(node1) + " " + getNodeName(node2);
    }

    string getType() override { return "Diode"; }

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

        // Draw component name
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
};

class Ground : public Component {
public:
    Ground(const string& n, int node) : Component(GROUND, n, node, 0, 0.0) {}

    void stamp(vector<vector<double>>&, vector<vector<double>>&, vector<vector<double>>&, vector<vector<double>>&,
               vector<double>&, vector<double>&, int&) override {}

    string getType() override { return "Ground"; }

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

        // Draw component name
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

        // Draw component name
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

        double t = Circuit::getTime();
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

        // Draw component name
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

        double t = Circuit::getTime();
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

        // Draw component name
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

        // Draw component name
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
        double t = Circuit::getTime();
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

        // Draw component name
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
        double t = Circuit::getTime();
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

        // Draw component name
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

        // Draw component name
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

        // Draw component name
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

        // Draw component name
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

        // Draw component name
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
                case 'E': { // VCVS
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
                case 'G': { // VCCS
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
                case 'H': { // CCVS
                    string vsName, gainStr;
                    if (!(iss >> vsName >> gainStr)) {
                        cerr << "Error: Invalid CCVS parameters" << endl;
                        continue;
                    }
                    double gain = parseSpiceValue(gainStr);
                    circuit.addComponent(new CCVS(name, n1, n2, vsName, gain));
                    break;
                }
                case 'F': { // CCCS
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
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    if (TTF_Init() == -1) {
        std::cerr << "SDL_ttf could not initialize! TTF_Error: " << TTF_GetError() << std::endl;
        return false;
    }

    window = SDL_CreateWindow("Circuit Simulator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    font = TTF_OpenFont("C:\\Windows\\Fonts\\consola.ttf", 24);
    if (!font) {
        std::cerr << "Failed to load font! TTF_Error: " << TTF_GetError() << std::endl;
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

void saveUndoState(Circuit*& currentCircuit) {
    // Clear redo stack when making new changes
    for (auto circuit : redoStack) {
        delete circuit;
    }
    redoStack.clear();

    // Save current state to undo stack
    Circuit* copy = new Circuit(*currentCircuit);
    undoStack.push_back(copy);

    // Limit undo stack size
    if (undoStack.size() > 10) {
        delete undoStack.front();
        undoStack.erase(undoStack.begin());
    }
}

void showAnalysisSettings(SDL_Renderer* renderer) {
    SDL_Rect analysisWindow = {250, 150, 360, 300};

    SDL_SetRenderDrawColor(renderer, currentTheme.background.r, currentTheme.background.g, currentTheme.background.b, 255);
    SDL_RenderFillRect(renderer, &analysisWindow);

    renderText("Analysis Settings", analysisWindow.x + 20, analysisWindow.y + 20, currentTheme.text);

    renderText("Time Step (s):", analysisWindow.x + 20, analysisWindow.y + 60, currentTheme.text);
    TextBox stepBox = {analysisWindow.x + 150, analysisWindow.y + 60, 150, 30, std::to_string(tStep)};
    renderTextBox(stepBox);

    renderText("Stop Time (s):", analysisWindow.x + 20, analysisWindow.y + 110, currentTheme.text);
    TextBox stopBox = {analysisWindow.x + 150, analysisWindow.y + 110, 150, 30, std::to_string(tStop)};
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

    renderButton(undoBtn);
    renderButton(redoBtn);
    renderButton(copyBtn);
    renderButton(pasteBtn);
    renderButton(deleteBtn);
}

void showComponentProperties(SDL_Renderer* renderer, Component* component) {
    if (!component) return;

    SDL_Rect propWindow = {600, 100, 350, 400};

    SDL_SetRenderDrawColor(renderer, 230, 250, 230, 255);
    SDL_RenderFillRect(renderer, &propWindow);

    renderText("Component Properties", propWindow.x + 20, propWindow.y + 20, BLACK);

    //renderText("Type: " + component->getTypeString(), propWindow.x + 20, propWindow.y + 60, BLACK);
    renderText("Name: " + component->name, propWindow.x + 20, propWindow.y + 90, BLACK);

    renderText("Connections:", propWindow.x + 20, propWindow.y + 120, BLACK);
    renderText("Node 1: " + getNodeName(component->node1), propWindow.x + 40, propWindow.y + 150, BLACK);
    renderText("Node 2: " + getNodeName(component->node2), propWindow.x + 40, propWindow.y + 180, BLACK);

    if (component->type != GROUND && component->type != DIODE) {
        renderText("Value:", propWindow.x + 20, propWindow.y + 210, BLACK);
        TextBox valueBox = {propWindow.x + 100, propWindow.y + 210, 150, 30, std::to_string(component->value)};
        renderTextBox(valueBox);
    }

    if (component->type == SIN_VOLTAGE_SOURCE || component->type == SIN_CURRENT_SOURCE) {
        //SinSource* src = dynamic_cast<SinSource*>(component);
        renderText("Amplitude:", propWindow.x + 20, propWindow.y + 250, BLACK);
        //TextBox ampBox = {propWindow.x + 120, propWindow.y + 250, 150, 30, std::to_string(src->amplitude)};
        //renderTextBox(ampBox);
    }

    Button saveBtn = {propWindow.x + 50, propWindow.y + 320, 100, 40, "Save", GREEN};
    Button deleteBtn = {propWindow.x + 200, propWindow.y + 320, 100, 40, "Delete", RED};
    renderButton(saveBtn);
    renderButton(deleteBtn);
}

void showSaveAsDialog(SDL_Renderer* renderer, string& currentFilename) {
    SDL_Rect dialog = {250, 200, 300, 200};

    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
    SDL_RenderFillRect(renderer, &dialog);

    renderText("Save Circuit As", dialog.x + 20, dialog.y + 20, BLACK);

    renderText("Filename:", dialog.x + 20, dialog.y + 60, BLACK);
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

void handleSaveButton(Circuit* circuit, const string& currentCircuitFile) {
    if (!circuit) return;

    if (currentCircuitFile.empty()) {
        cout << "No current circuit file specified!" << endl;
        return;
    }

    if (saveCircuitToFile(*circuit, currentCircuitFile)) {
        cout << "Circuit saved to " << currentCircuitFile << endl;
    } else {
        cout << "Failed to save circuit!" << endl;
    }
}

void handleLoadButton(Circuit*& circuit, string& currentCircuitFile) {
    if (currentCircuitFile.empty()) {
        cout << "No circuit file to load!" << endl;
        return;
    }

    resetGlobalState();
    delete circuit;
    circuit = new Circuit();

    processCircuitFile(currentCircuitFile, *circuit);
    cout << "Reloaded circuit from " << currentCircuitFile << endl;
}

void calculateNodePositions(const Circuit& circuit) {
    nodePositions.clear();

    int xSpacing = 100;
    int yPos = 100;
    int xStart = 100;

    for (const auto& node : circuit.getNodeMap()) {
        int nodeNum = node.second;
        nodePositions[nodeNum] = {xStart + (nodeNum * xSpacing), yPos};
    }

    if (circuit.getNodeMap().count("GND") || circuit.getNodeMap().count("0")) {
        nodePositions[0] = {xStart, yPos + 100};  // Position ground below others
    }
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

void drawCapacitor(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, const string& name) {
    const int gap = 12;

    int px1, py1, px2, py2;
    getPerpendicularPoints(x1, y1, x2, y2, gap/2, px1, py1, px2, py2);
    int nx1, ny1, nx2, ny2;
    getPerpendicularPoints(x1, y1, x2, y2, -gap/2, nx1, ny1, nx2, ny2);

    SDL_SetRenderDrawColor(renderer, BLACK.r, BLACK.g, BLACK.b, 255);
    SDL_RenderDrawLine(renderer, px1, py1, px2, py2);
    SDL_RenderDrawLine(renderer, nx1, ny1, nx2, ny2);

    renderText(name, (x1 + x2)/2, (y1 + y2)/2 - gap, BLACK);
}

void drawInductor(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, const string& name) {
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
    for (int i = 0; i < loops; i++) {
        float centerX = x1 + dx * (i * 2 + 1) * segmentLength;
        float centerY = y1 + dy * (i * 2 + 1) * segmentLength;

        for (int angle = -90; angle <= 90; angle += 5) {
            float rad = angle * M_PI / 180.0f;
            float x = centerX + px * radius * cos(rad);
            float y = centerY + py * radius * sin(rad);
            if (angle == -90) {
                SDL_RenderDrawPoint(renderer, (int)x, (int)y);
            } else {
                SDL_RenderDrawLine(renderer, (int)x, (int)y, (int)x, (int)y);
            }
        }
    }

    renderText(name, (x1 + x2)/2 + px * radius*2, (y1 + y2)/2 + py * radius*2, BLACK);
}

void drawVoltageSource(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, const string& name) {
    const int radius = 12;
    int centerX = (x1 + x2) / 2;
    int centerY = (y1 + y2) / 2;

    SDL_SetRenderDrawColor(renderer, BLACK.r, BLACK.g, BLACK.b, 255);
    for (int angle = 0; angle < 360; angle += 5) {
        float rad = angle * M_PI / 180.0f;
        int x = centerX + radius * cos(rad);
        int y = centerY + radius * sin(rad);
        SDL_RenderDrawPoint(renderer, x, y);
    }

    SDL_RenderDrawLine(renderer, centerX-5, centerY, centerX+5, centerY);
    SDL_RenderDrawLine(renderer, centerX, centerY-5, centerX, centerY+5);
    SDL_RenderDrawLine(renderer, centerX-5, centerY+radius+5, centerX+5, centerY+radius+5);

    renderText(name, centerX, centerY + radius + 15, BLACK);
}

int findOrCreateNodeAt(int x, int y) {
    // First check if we're near an existing node (with some threshold)
    const int NODE_PROXIMITY_THRESHOLD = 15; // pixels

    for (const auto& node : nodePositions) {
        int dx = x - node.second.x;
        int dy = y - node.second.y;
        if (dx*dx + dy*dy < NODE_PROXIMITY_THRESHOLD*NODE_PROXIMITY_THRESHOLD) {
            return node.first;
        }
    }

    // If not near any existing node, create a new one
    int newNode = nextNodeNumber++;
    nodePositions[newNode] = {x, y};
    reverseNodeMap[newNode] = to_string(newNode);
    return newNode;
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
        // First click - start placement
        placementStartPoint = {x, y};
        isPlacingComponent = true;
        return;
    }

    if (isPlacingComponent) {
        // Second click - finish placement
        string name = "";
        Component* newComp = nullptr;
        double defaultValue = 0.0;

        // Find or create nodes at both positions
        int node1 = findOrCreateNodeAt(placementStartPoint.x, placementStartPoint.y);
        int node2 = findOrCreateNodeAt(x, y);

        switch(currentPlacementMode) {
            case PLACE_RESISTOR:
                name = "R" + to_string(compCount++);
                defaultValue = 1000.0;
                newComp = new Resistor(name, node1, node2, defaultValue);
                break;
            case PLACE_CAPACITOR:
                name = "C" + to_string(compCount++);
                defaultValue = 1e-6;
                newComp = new Capacitor(name, node1, node2, defaultValue);
                break;
            case PLACE_INDUCTOR:
                name = "L" + to_string(compCount++);
                defaultValue = 1e-3;
                newComp = new Inductor(name, node1, node2, defaultValue);
                break;
            case PLACE_VOLTAGE_SOURCE:
                name = "V" + to_string(compCount++);
                defaultValue = 5.0;
                newComp = new VoltageSource(name, node1, node2, defaultValue);
                break;
            case PLACE_CURRENT_SOURCE:
                name = "I" + to_string(compCount++);
                defaultValue = 0.1;
                newComp = new CurrentSource(name, node1, node2, defaultValue);
                break;
            case PLACE_DIODE:
                name = "D" + to_string(compCount++);
                newComp = new Diode(name, node1, node2);
                break;
            case PLACE_GROUND:
                name = "GND" + to_string(compCount++);
                newComp = new Ground(name, node1);
                break;
            default:
                break;
        }

        if (newComp) {
            saveUndoState(circuit);
            circuit->addComponent(newComp);
            cout << "Added " << newComp->getType() << " " << name
                 << " between nodes " << node1 << " and " << node2 << endl;
        }

        isPlacingComponent = false;
        currentPlacementMode = PLACE_NONE;
    }

}

void drawCircuit(const Circuit& circuit, SDL_Renderer* renderer) {
    // Draw all components
    for (const auto& comp : circuit.getComponents()) {
        comp->render(renderer, nodePositions);
    }

    // Draw all nodes
    for (const auto& node : nodePositions) {
        SDL_Rect nodeRect = {node.second.x - 5, node.second.y - 5, 10, 10};
        SDL_SetRenderDrawColor(renderer, BLUE.r, BLUE.g, BLUE.b, 255);
        SDL_RenderFillRect(renderer, &nodeRect);

        // Draw node label
        string label = (node.first == 0) ? "GND" : to_string(node.first);
        renderText(label, node.second.x + 12, node.second.y - 8, currentTheme.text);
    }

    // Draw placement preview if in placement mode

    if (isPlacingComponent) {
        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);
        SDL_SetRenderDrawColor(renderer, GREEN.r, GREEN.g, GREEN.b, 128);
        SDL_RenderDrawLine(renderer, placementStartPoint.x, placementStartPoint.y, mouseX, mouseY);

        // Draw component preview at current position
        switch(currentPlacementMode) {
            case PLACE_RESISTOR:
                drawResistorPreview(renderer, placementStartPoint.x, placementStartPoint.y, mouseX, mouseY);
            break;
        }
    }

    for (const auto& node : circuit.getNodeMap()) {

        SDL_Point pos = nodePositions[node.second];
        SDL_Rect nodeRect = {pos.x - 5, pos.y - 5, 10, 10};
        SDL_SetRenderDrawColor(renderer, BLUE.r, BLUE.g, BLUE.b, 255);
        SDL_RenderFillRect(renderer, &nodeRect);
       // renderText(node.first, pos.x + 10, pos.y - 10, BLACK);   runTime error
    }
}

void plotSignals(SDL_Renderer* renderer, const std::vector<double>& voltages,
                 const std::vector<double>& times, const SDL_Rect& area) {
    if (voltages.empty() || times.empty()) return;

    double minV = *std::min_element(voltages.begin(), voltages.end());
    double maxV = *std::max_element(voltages.begin(), voltages.end());
    double minT = *std::min_element(times.begin(), times.end());
    double maxT = *std::max_element(times.begin(), times.end());

    SDL_SetRenderDrawColor(renderer, BLACK.r, BLACK.g, BLACK.b, 255);
    SDL_RenderDrawLine(renderer, area.x, area.y + area.h/2, area.x + area.w, area.y + area.h/2);
    SDL_RenderDrawLine(renderer, area.x, area.y, area.x, area.y + area.h);

    for (size_t i = 1; i < times.size(); i++) {
        int x1 = area.x + static_cast<int>((times[i-1] - minT) / (maxT - minT) * area.w);
        int y1 = area.y + area.h - static_cast<int>((voltages[i-1] - minV) / (maxV - minV) * area.h);
        int x2 = area.x + static_cast<int>((times[i] - minT) / (maxT - minT) * area.w);
        int y2 = area.y + area.h - static_cast<int>((voltages[i] - minV) / (maxV - minV) * area.h);

        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
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
            selectedComponentType = "VCVS";
        }
        else if (isMouseOver(vccsBtn.rect, x, y)) {
            selectedComponentType = "VCCS";
        }
        else if (isMouseOver(ccvsBtn.rect, x, y)) {
            selectedComponentType = "CCVS";
        }
        else if (isMouseOver(cccsBtn.rect, x, y)) {
            selectedComponentType = "CCCS";
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

    // Render category buttons
    renderButton(passiveBtn);
    renderButton(sourcesBtn);
    renderButton(semiBtn);
    renderButton(depBtn);

    // Render components based on selected category
    if (showPassives) {
        renderButton(resBtn);
        renderButton(capBtn);
        renderButton(indBtn);

        // Draw component previews
        SDL_Rect resPreview = {900, 250, 60, 30};
        SDL_SetRenderDrawColor(renderer, 200, 100, 100, 255);
        SDL_RenderFillRect(renderer, &resPreview);
        renderText("R", 920, 255, WHITE);

        SDL_Rect capPreview = {900, 300, 60, 30};
        SDL_SetRenderDrawColor(renderer, 100, 100, 200, 255);
        SDL_RenderFillRect(renderer, &capPreview);
        renderText("C", 920, 305, WHITE);

        SDL_Rect indPreview = {900, 350, 60, 30};
        SDL_SetRenderDrawColor(renderer, 100, 200, 100, 255);
        SDL_RenderFillRect(renderer, &indPreview);
        renderText("L", 920, 355, WHITE);
    }

    if (showSources) {
        renderButton(vSrcBtn);
        renderButton(iSrcBtn);
        renderButton(gndBtn);

        // Draw component previews
        SDL_Rect vPreview = {1000, 250, 30, 30};
        SDL_SetRenderDrawColor(renderer, 200, 200, 100, 255);
        SDL_RenderFillRect(renderer, &vPreview);
        renderText("V", 1010, 255, WHITE);

        SDL_Rect iPreview = {1000, 300, 30, 30};
        SDL_SetRenderDrawColor(renderer, 200, 100, 200, 255);
        SDL_RenderFillRect(renderer, &iPreview);
        renderText("I", 1010, 305, WHITE);

        SDL_Rect gndPreview = {1000, 350, 30, 30};
        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
        SDL_RenderFillRect(renderer, &gndPreview);
        renderText("GND", 1010, 355, WHITE);
    }

    if (showSemis) {
        renderButton(diodeBtn);

        // Draw component preview
        SDL_Rect diodePreview = {900, 250, 60, 30};
        SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
        SDL_RenderFillRect(renderer, &diodePreview);
        renderText("D", 920, 255, WHITE);
    }

    if (showDependents) {
        renderButton(vcvsBtn);
        renderButton(vccsBtn);
        renderButton(ccvsBtn);
        renderButton(cccsBtn);

        // Draw component previews
        SDL_Rect vcvsPreview = {1000, 250, 60, 30};
        SDL_SetRenderDrawColor(renderer, 200, 150, 100, 255);
        SDL_RenderFillRect(renderer, &vcvsPreview);
        renderText("E", 1020, 255, WHITE);

        SDL_Rect vccsPreview = {1000, 300, 60, 30};
        SDL_SetRenderDrawColor(renderer, 100, 150, 200, 255);
        SDL_RenderFillRect(renderer, &vccsPreview);
        renderText("G", 1020, 305, WHITE);
    }

    // Close button
    Button closeBtn = {libWindow.x + libWindow.w - 40, libWindow.y + 10, 30, 30, "X", RED};
    renderButton(closeBtn);
}

void drawComponent(SDL_Renderer* renderer, Component* comp) {
    std::pair<int, int> pos = comp->getPosition();
    int x = pos.first;
    int y = pos.second;
    std::string type = comp->getType();

    SDL_Rect rect = { x - 20, y - 10, 40, 20 };

    if (type == "Resistor") {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderFillRect(renderer, &rect);
    }
    else if (type == "Capacitor") {
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
        SDL_RenderFillRect(renderer, &rect);
    }
}

int main(int argc, char* argv[]) {
    changeToPreviousDirectory();

    string currentCircuitFile = "";
    Circuit* circuit = new Circuit();

    if (!initSDL()) {
        return 1;
    }

    bool running = true;
    SDL_Event event;
    string inputText = "";
    bool textInputActive = false;
    TextBox* activeTextBox = nullptr;
    vector<string> txtFiles = listTxtFiles(".");
    bool FileDialog = false;
    bool SaveAsDialog = false;

    // Initialize node positions with ground node
// In main(), before the main loop:
// Initialize node positions with ground node
    nodePositions[0] = {100, 500};
    reverseNodeMap[0] = "GND";
    nextNodeNumber = 1; // Start numbering regular nodes from 1

    while (running) {
        if (undoStack.empty()) {
            saveUndoState(circuit);
        }

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN) {
                int x, y;
                SDL_GetMouseState(&x, &y);

                // Handle dark mode toggle
                if (isMouseOver(darkModeBtn.rect, x, y)) {
                    darkMode = !darkMode;
                    currentTheme = darkMode ? darkTheme : lightTheme;
                    darkModeBtn.text = darkMode ? "Light Mode" : "Dark Mode";
                    continue;
                }

                // Handle toolbar buttons
                if (y <= 40) {
                    if (x >= 10 && x <= 90) { // File button
                        FileMenu = !FileMenu;
                        EditMenu = false;
                        ComponentLibrary = false;
                        AnalysisSettings = false;
                        continue;
                    }
                    else if (x >= 100 && x <= 180) { // Edit button
                        EditMenu = !EditMenu;
                        ComponentLibrary = false;
                        FileMenu = false;
                        AnalysisSettings = false;
                        continue;
                    }
                    else if (x >= 280 && x <= 360) { // Analyze button
                        AnalysisSettings = !AnalysisSettings;
                        FileMenu = false;
                        EditMenu = false;
                        ComponentLibrary = false;
                        continue;
                    }
                    else if (x >= 370 && x <= 450) { // Components button
                        ComponentLibrary = !ComponentLibrary;
                        FileMenu = false;
                        EditMenu = false;
                        AnalysisSettings = false;
                        continue;
                    }
                }

                // Handle circuit area interactions
                if (x >= circuitArea.x && x <= circuitArea.x + circuitArea.w &&
                    y >= circuitArea.y && y <= circuitArea.y + circuitArea.h) {

                    // If in placement mode, handle component placement
                    if (currentPlacementMode != PLACE_NONE) {
                        handleComponentPlacement(circuit, x, y);
                    }
                    // Otherwise handle selection
                    else {
                        handleComponentSelection(circuit, x, y);
                    }
                }

                // Handle component library
                if (ComponentLibrary) {
                    handleComponentLibraryClick(x, y);

                    // Check for close button
                    SDL_Rect libWindow = {850, 50, 380, 600};
                    Button closeBtn = {libWindow.x + libWindow.w - 40, libWindow.y + 10, 30, 30, "X", RED};
                    if (isMouseOver(closeBtn.rect, x, y)) {
                        ComponentLibrary = false;
                    }
                    continue;
                }

                // Handle file menu
                if (FileMenu) {
                    if (x >= fileMenuRect.x + 10 && x <= fileMenuRect.x + 130) {
                        if (y >= fileMenuRect.y + 40 && y <= fileMenuRect.y + 70) { // New
                            resetGlobalState();
                            delete circuit;
                            circuit = new Circuit();
                            currentCircuitFile = "";
                            FileMenu = false;
                        }
                        else if (y >= fileMenuRect.y + 80 && y <= fileMenuRect.y + 110) { // Open
                            FileDialog = true;
                            txtFiles = listTxtFiles(".");
                        }
                        else if (y >= fileMenuRect.y + 120 && y <= fileMenuRect.y + 150) { // Save
                            if (!currentCircuitFile.empty()) {
                                saveCircuitToFile(*circuit, currentCircuitFile);
                            }
                            else {
                                SaveAsDialog = true;
                            }
                            FileMenu = false;
                        }
                        else if (y >= fileMenuRect.y + 160 && y <= fileMenuRect.y + 190) { // Save As
                            SaveAsDialog = true;
                            FileMenu = false;
                        }
                        else if (y >= fileMenuRect.y + 200 && y <= fileMenuRect.y + 230) { // Exit
                            running = false;
                        }
                    }
                    continue;
                }

                // Handle edit menu
                if (EditMenu) {
                    if (x >= editMenuRect.x + 10 && x <= editMenuRect.x + 130) {
                        if (y >= editMenuRect.y + 40 && y <= editMenuRect.y + 70) { // Undo
                            if (!undoStack.empty()) {
                                saveUndoState(circuit); // Current state becomes redo
                                delete circuit;
                                circuit = new Circuit(*undoStack.back());
                                undoStack.pop_back();
                                calculateNodePositions(*circuit);
                            }
                            EditMenu = false;
                        }
                        else if (y >= editMenuRect.y + 80 && y <= editMenuRect.y + 110) { // Redo
                            if (!redoStack.empty()) {
                                saveUndoState(circuit); // Current state becomes undo
                                delete circuit;
                                circuit = new Circuit(*redoStack.back());
                                redoStack.pop_back();
                                calculateNodePositions(*circuit);
                            }
                            EditMenu = false;
                        }
                        else if (y >= editMenuRect.y + 120 && y <= editMenuRect.y + 150) { // Copy
                            if (selectedComponent) {
                                copiedComponent = selectedComponent->getInfo();
                            }
                            EditMenu = false;
                        }
                        else if (y >= editMenuRect.y + 160 && y <= editMenuRect.y + 190) { // Paste
                            if (!copiedComponent.empty()) {
                                istringstream iss(copiedComponent);
                                string type, name, node1, node2, valueStr;
                                iss >> type >> name >> node1 >> node2 >> valueStr;

                                // Generate unique name
                                static int pasteCount = 1;
                                name = name + "_copy" + to_string(pasteCount++);

                                // Create new component
                                Component* newComp = nullptr;
                                if (type == "Resistor") {
                                    newComp = new Resistor(name, getOrCreateNode(node1), getOrCreateNode(node2), stod(valueStr));
                                }
                                // ... other component types ...

                                if (newComp) {
                                    saveUndoState(circuit);
                                    circuit->addComponent(newComp);
                                    calculateNodePositions(*circuit);
                                }
                            }
                            EditMenu = false;
                        }
                        else if (y >= editMenuRect.y + 200 && y <= editMenuRect.y + 230) { // Delete
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

                // Handle analysis settings
                if (AnalysisSettings) {
                    SDL_Rect analysisWindow = {250, 150, 350, 300};
                    if (x >= analysisWindow.x + 50 && x <= analysisWindow.x + 150 &&
                        y >= analysisWindow.y + 220 && y <= analysisWindow.y + 260) { // Run
                        try {
                            circuit->analyzeTransient(tStep, tStop);
                            showVoltage = true;
                            showCurrent = false;
                            AnalysisSettings = false;
                        }
                        catch (const exception& e) {
                            cerr << "Analysis error: " << e.what() << endl;
                        }
                    }
                    else if (x >= analysisWindow.x + 200 && x <= analysisWindow.x + 300 &&
                             y >= analysisWindow.y + 220 && y <= analysisWindow.y + 260) { // Cancel
                        AnalysisSettings = false;
                    }
                    continue;
                }

                // Handle file dialog
                if (FileDialog) {
                    SDL_Rect dialog = {200, 150, 400, 400};
                    if (x >= dialog.x + 100 && x <= dialog.x + 200 &&
                        y >= dialog.y + 330 && y <= dialog.y + 370) { // Open
                        // Find which file was clicked
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
                             y >= dialog.y + 330 && y <= dialog.y + 370) { // Cancel
                        FileDialog = false;
                    }
                    continue;
                }

                // Handle save as dialog
                if (SaveAsDialog) {
                    SDL_Rect dialog = {250, 200, 300, 200};
                    if (x >= dialog.x + 50 && x <= dialog.x + 150 &&
                        y >= dialog.y + 120 && y <= dialog.y + 160) { // Save
                        handleSaveButton(circuit, nameBox.text);
                        SaveAsDialog = false;
                    }
                    else if (x >= dialog.x + 170 && x <= dialog.x + 270 &&
                             y >= dialog.y + 120 && y <= dialog.y + 160) { // Cancel
                        SaveAsDialog = false;
                    }
                    else if (x >= dialog.x + 100 && x <= dialog.x + 280 &&
                             y >= dialog.y + 60 && y <= dialog.y + 90) { // Text box
                        textInputActive = true;
                        activeTextBox = &nameBox;
                        inputText = nameBox.text;
                    }
                    continue;
                }

                // Handle text box input
                if (x >= node1Box.rect.x && x <= node1Box.rect.x + node1Box.rect.w &&
                    y >= node1Box.rect.y && y <= node1Box.rect.y + node1Box.rect.h) {
                    textInputActive = true;
                    activeTextBox = &node1Box;
                    inputText = node1Box.text;
                }
                else if (x >= node2Box.rect.x && x <= node2Box.rect.x + node2Box.rect.w &&
                         y >= node2Box.rect.y && y <= node2Box.rect.y + node2Box.rect.h) {
                    textInputActive = true;
                    activeTextBox = &node2Box;
                    inputText = node2Box.text;
                }
                else if (x >= valueBox.rect.x && x <= valueBox.rect.x + valueBox.rect.w &&
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
            else if (event.type == SDL_TEXTINPUT && textInputActive) {
                inputText += event.text.text;
            }
            else if (event.type == SDL_KEYDOWN) {
                // Handle ESC to cancel placement
                if (event.key.keysym.sym == SDLK_ESCAPE && currentPlacementMode != PLACE_NONE) {
                    currentPlacementMode = PLACE_NONE;
                    isPlacingComponent = false;
                }
            }
        }

        // Update text boxes if we're actively editing one
        if (textInputActive) {
            if (activeTextBox) {
                activeTextBox->text = inputText;
            }
            else if (SaveAsDialog) {
                nameBox.text = inputText;
            }
        }

        // Render everything
        SDL_SetRenderDrawColor(renderer, currentTheme.background.r, currentTheme.background.g, currentTheme.background.b, 255);
        SDL_RenderClear(renderer);

        // Draw circuit area
        SDL_SetRenderDrawColor(renderer, currentTheme.circuitBg.r, currentTheme.circuitBg.g, currentTheme.circuitBg.b, 255);
        SDL_RenderFillRect(renderer, &circuitArea);
        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 255);
        SDL_RenderDrawRect(renderer, &circuitArea);

        // Draw plot area
        SDL_SetRenderDrawColor(renderer, currentTheme.plotBg.r, currentTheme.plotBg.g, currentTheme.plotBg.b, 255);
        SDL_RenderFillRect(renderer, &plotArea);
        SDL_SetRenderDrawColor(renderer, currentTheme.text.r, currentTheme.text.g, currentTheme.text.b, 255);
        SDL_RenderDrawRect(renderer, &plotArea);

        // Draw the circuit
        if (circuit) {
            drawCircuit(*circuit, renderer);
        }

        // Draw plots if we have data
        if (!voltages.empty() && showVoltage) {
            plotSignals(renderer, voltages, Vtimes, plotArea);
        }

        // Draw toolbar
        drawToolbar(renderer);

        // Draw text boxes
        renderTextBox(node1Box);
        renderTextBox(node2Box);
        renderTextBox(valueBox);

        // Draw file menu if open
        if (FileMenu) {
            showFileMenu(renderer, fileMenuRect);
        }

        // Draw edit menu if open
        if (EditMenu) {
            showEditMenu(renderer, editMenuRect);
        }

        // Draw component library if open
        if (ComponentLibrary) {
            renderComponentLibrary(renderer);
        }

        // Draw analysis settings if open
        if (AnalysisSettings) {
            showAnalysisSettings(renderer);
        }

        // Draw file dialog if open
        if (FileDialog) {
            showFileDialog(renderer, txtFiles);
        }

        // Draw save as dialog if open
        if (SaveAsDialog) {
            showSaveAsDialog(renderer, nameBox.text);
        }

        // Draw selected component properties if one is selected
        if (selectedComponent) {
            showComponentProperties(renderer, selectedComponent);
        }

        // Show placement mode status
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
                default: break;
            }

            string message = "Placing " + modeName + " - Click first connection point";
            if (isPlacingComponent) {
                message = "Placing " + modeName + " - Click second connection point (ESC to cancel)";
            }

            renderText(message, 10, SCREEN_HEIGHT - 30, currentTheme.text);
        }

        // Update screen
        SDL_RenderPresent(renderer);
    }

    // Cleanup
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    delete circuit;
    return 0;
}