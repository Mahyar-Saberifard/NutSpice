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
bool ComponentLibrary = false;
bool AnalysisSettings = false;
SDL_Rect fileMenuRect = {10, 45, 140, 250};

double tStep = 0.001;
double tStop = 0.1;
std::map<int, SDL_Point> nodePositions;

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
    }
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
    std::string getNodeName(int nodeNum) const {
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
};

class Ground : public Component {
public:
    Ground(const string& n, int node) : Component(GROUND, n, node, 0, 0.0) {}

    void stamp(vector<vector<double>>&, vector<vector<double>>&, vector<vector<double>>&, vector<vector<double>>&,
               vector<double>&, vector<double>&, int&) override {}
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

void showSaveMenu() {
    cout << "\n--- Save & Load Menu ---\n";
    cout << "Available commands:\n";
    cout << "  list              - Show available circuit files (.txt)\n";
    cout << "  load <name|num>   - Load a circuit file\n";
    cout << "  create <name>     - Start a new empty circuit file\n";
    cout << "  exit              - Quit the program\n";
    cout << "------------------------\n";
}

void showCircuitHelp() {
    cout << "--- Circuit Edit & Analysis Menu ---\n";
    cout << "Available commands:\n";
    cout << "  add R<name> <node1> <node2> <value> - Add resistor\n";
    cout << "  add C<name> <node1> <node2> <value> - Add capacitor\n";
    cout << "  add L<name> <node1> <node2> <value> - Add inductor\n";
    cout << "  add V<name> <node1> <node2> <value> - Add voltage source\n";
    cout << "  add I<name> <node1> <node2> <value> - Add current source\n";
    cout << "  add D<name> <node1> <node2> <model> - Add diode\n";
    cout << "  add GND <node> - Add ground connection\n";
    cout << "  add VS<name> <node1> <node2> <DC> <AMP> <FREQ> [PHASE] - Add sinusoidal voltage source\n";
    cout << "  add VP<name> <node1> <node2> <V1> <V2> <TD> <TR> <TF> <PW> <PER> - Add pulse voltage source\n";
    cout << "  add IS<name> <node1> <node2> <DC> <AMP> <FREQ> [PHASE] - Add sinusoidal current source\n";
    cout << "  add IP<name> <node1> <node2> <I1> <I2> <TD> <TR> <TF> <PW> <PER> - Add pulse current source\n";
    cout << "  add E<name> <node+> <node-> <ctrl+> <ctrl-> <gain> - Add VCVS\n";
    cout << "  add H<name> <node+> <node-> <v_src> <gain> - Add CCVS\n";
    cout << "  add G<name> <node+> <node-> <ctrl+> <ctrl-> <gm> - Add VCCS\n";
    cout << "  add F<name> <node+> <node-> <v_src> <gain> - Add CCCS\n";
    cout << "  delete <name> - Delete component\n";
    cout << "  .nodes - List all nodes\n";
    cout << "  .list [type] - List components\n";
    cout << "  .rename [node|element] <old> <new> - Rename a node or element\n";
    cout << "  save [filename] - Save circuit to file (uses current name if none given)\n";
    cout << "  analyze DC [SWEEP <src> <start> <stop> <step>] - Run DC analysis\n";
    cout << "  analyze TRAN <tstep> <tstop> - Run transient analysis\n";
    cout << "  return - Return to the Save & Load Menu\n";
    cout << "  help - Show this help menu\n";
    cout << "  exit - Quit program\n";
    cout << "------------------------------------\n";
}

SDL_Window* window = nullptr;
SDL_Renderer* renderer = nullptr;
const int SCREEN_WIDTH = 1280;
const int SCREEN_HEIGHT = 720;

const SDL_Color WHITE = {255, 255, 255, 255};
const SDL_Color BLACK = {0, 0, 0, 255};
const SDL_Color RED = {255, 0, 0, 255};
const SDL_Color GREEN = {0, 255, 0, 255};
const SDL_Color BLUE = {0, 0, 255, 255};
const SDL_Color GRAY = {200, 200, 200, 255};

TTF_Font* font = nullptr;

struct Button {
    SDL_Rect rect;
    std::string text;
    SDL_Color color;
    bool isActive;
};

struct TextBox {
    SDL_Rect rect;
    std::string text;
    bool isActive;
};

SDL_Rect circuitArea = {50, 50, 800, 600};
SDL_Rect plotArea = {850, 50, 380, 600};
Button fileBtn = {10, 5, 80, 30, "File", GRAY};
Button componentLibBtn = {100, 5, 80, 30, "Components", GRAY};
Button analyzeBtn = {190, 5, 80, 30, "Analyze", GRAY};
Button plotBtn = {280, 5, 80, 30, "Plot", BLUE};

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

void renderText(const std::string& text, int x, int y, SDL_Color color) {
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
    SDL_SetRenderDrawColor(renderer, BLACK.r, BLACK.g, BLACK.b, 255);
    SDL_RenderDrawRect(renderer, &button.rect);
    renderText(button.text, button.rect.x + 10, button.rect.y + 10, BLACK);
}

void renderTextBox(const TextBox& box) {
    SDL_SetRenderDrawColor(renderer, WHITE.r, WHITE.g, WHITE.b, 255);
    SDL_RenderFillRect(renderer, &box.rect);
    SDL_SetRenderDrawColor(renderer, BLACK.r, BLACK.g, BLACK.b, 255);
    SDL_RenderDrawRect(renderer, &box.rect);
    renderText(box.text, box.rect.x + 5, box.rect.y + 5, BLACK);
}

void calculateNodePositions(const Circuit& circuit) {
    nodePositions.clear();

    // Simple layout - nodes in a horizontal line
    int xSpacing = 100;
    int yPos = 100;
    int xStart = 100;

    for (const auto& node : circuit.getNodeMap()) {
        int nodeNum = node.second;
        nodePositions[nodeNum] = {xStart + (nodeNum * xSpacing), yPos};
    }

    // Special position for ground (node 0)
    if (circuit.getNodeMap().count("GND") || circuit.getNodeMap().count("0")) {
        nodePositions[0] = {xStart, yPos + 100};  // Position ground below others
    }
}

void getPerpendicularPoints(int x1, int y1, int x2, int y2, int offset,
                          int& outX1, int& outY1, int& outX2, int& outY2) {
    // Calculate direction vector
    int dx = x2 - x1;
    int dy = y2 - y1;

    // Normalize
    float length = sqrt(dx*dx + dy*dy);
    float nx = -dy/length;
    float ny = dx/length;

    // Apply offset
    outX1 = x1 + nx * offset;
    outY1 = y1 + ny * offset;
    outX2 = x2 + nx * offset;
    outY2 = y2 + ny * offset;
}

void drawResistor(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, const string& name) {
    const int segments = 5;
    const int amplitude = 10;

    // Calculate direction vector
    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = sqrt(dx*dx + dy*dy);

    // Normalize and perpendicular vector
    dx /= length;
    dy /= length;
    float px = -dy;
    float py = dx;

    // Draw zigzag
    SDL_Point points[segments + 1];
    for (int i = 0; i <= segments; i++) {
        float t = (float)i / segments;
        float x = x1 + t * (x2 - x1);
        float y = y1 + t * (y2 - y1);

        // Alternate direction for zigzag
        float offset = (i % 2) ? amplitude : -amplitude;
        points[i].x = x + px * offset;
        points[i].y = y + py * offset;
    }

    SDL_SetRenderDrawColor(renderer, BLACK.r, BLACK.g, BLACK.b, 255);
    SDL_RenderDrawLines(renderer, points, segments + 1);

    // Draw component name
    renderText(name, (x1 + x2)/2 + px * amplitude*2, (y1 + y2)/2 + py * amplitude*2, BLACK);
}

void drawCapacitor(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, const string& name) {
    const int gap = 12;

    // Get perpendicular points
    int px1, py1, px2, py2;
    getPerpendicularPoints(x1, y1, x2, y2, gap/2, px1, py1, px2, py2);
    int nx1, ny1, nx2, ny2;
    getPerpendicularPoints(x1, y1, x2, y2, -gap/2, nx1, ny1, nx2, ny2);

    // Draw the plates
    SDL_SetRenderDrawColor(renderer, BLACK.r, BLACK.g, BLACK.b, 255);
    SDL_RenderDrawLine(renderer, px1, py1, px2, py2);
    SDL_RenderDrawLine(renderer, nx1, ny1, nx2, ny2);

    // Draw component name
    renderText(name, (x1 + x2)/2, (y1 + y2)/2 - gap, BLACK);
}

void drawInductor(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, const string& name) {
    const int loops = 3;
    const int radius = 8;

    // Calculate direction and perpendicular vectors
    float dx = x2 - x1;
    float dy = y2 - y1;
    float length = sqrt(dx*dx + dy*dy);
    dx /= length;
    dy /= length;
    float px = -dy;
    float py = dx;

    // Draw each loop
    float segmentLength = length / (loops * 2);
    for (int i = 0; i < loops; i++) {
        float centerX = x1 + dx * (i * 2 + 1) * segmentLength;
        float centerY = y1 + dy * (i * 2 + 1) * segmentLength;

        // Draw semicircle
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

    // Draw component name
    renderText(name, (x1 + x2)/2 + px * radius*2, (y1 + y2)/2 + py * radius*2, BLACK);
}

void drawVoltageSource(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, const string& name) {
    const int radius = 12;
    int centerX = (x1 + x2) / 2;
    int centerY = (y1 + y2) / 2;

    // Draw circle
    SDL_SetRenderDrawColor(renderer, BLACK.r, BLACK.g, BLACK.b, 255);
    for (int angle = 0; angle < 360; angle += 5) {
        float rad = angle * M_PI / 180.0f;
        int x = centerX + radius * cos(rad);
        int y = centerY + radius * sin(rad);
        SDL_RenderDrawPoint(renderer, x, y);
    }

    // Draw + and - signs
    SDL_RenderDrawLine(renderer, centerX-5, centerY, centerX+5, centerY);
    SDL_RenderDrawLine(renderer, centerX, centerY-5, centerX, centerY+5);
    SDL_RenderDrawLine(renderer, centerX-5, centerY+radius+5, centerX+5, centerY+radius+5);

    // Draw component name
    renderText(name, centerX, centerY + radius + 15, BLACK);
}

void drawCircuit(const Circuit& circuit, SDL_Renderer* renderer) {
    // First calculate node positions
    std::map<int, SDL_Point> nodePositions;
    int yLevel = 100;
    int xSpacing = 100;

    calculateNodePositions(circuit);

    // Assign positions to nodes
    for (const auto& node : circuit.getNodeMap()) {
        int x = 100 + node.second * xSpacing;
        nodePositions[node.second] = {x, yLevel};
    }

    // Draw components
    for (const auto& comp : circuit.getComponents()) {
        SDL_Point p1 = nodePositions[comp->node1];
        SDL_Point p2 = nodePositions[comp->node2];

        // Draw the wire (only if not ground)
        if (comp->node1 != 0 && comp->node2 != 0) {
            SDL_SetRenderDrawColor(renderer, BLACK.r, BLACK.g, BLACK.b, 255);
            SDL_RenderDrawLine(renderer, p1.x, p1.y, p2.x, p2.y);
        }

        // Draw component symbol
        switch(comp->type) {
            case RESISTOR:
                drawResistor(renderer, p1.x, p1.y, p2.x, p2.y, comp->name);
            break;
            case CAPACITOR:
                drawCapacitor(renderer, p1.x, p1.y, p2.x, p2.y, comp->name);
            break;
            case INDUCTOR:
                drawInductor(renderer, p1.x, p1.y, p2.x, p2.y, comp->name);
            break;
            case VOLTAGE_SOURCE:
                drawVoltageSource(renderer, p1.x, p1.y, p2.x, p2.y, comp->name);
            break;
            case CURRENT_SOURCE:
                // You could add drawCurrentSource here
                    break;
            default:
                renderText(comp->name, (p1.x + p2.x)/2, (p1.y + p2.y)/2, BLACK);
        }
    }

    // Draw nodes on top of components
    for (const auto& node : circuit.getNodeMap()) {
        SDL_Point pos = nodePositions[node.second];
        SDL_Rect nodeRect = {pos.x - 5, pos.y - 5, 10, 10};
        SDL_SetRenderDrawColor(renderer, BLUE.r, BLUE.g, BLUE.b, 255);
        SDL_RenderFillRect(renderer, &nodeRect);
        renderText(node.first, pos.x + 10, pos.y - 10, BLACK);
    }
}

void plotSignals(SDL_Renderer* renderer, const std::vector<double>& voltages,
                const std::vector<double>& times, const SDL_Rect& area) {
    if (voltages.empty() || times.empty()) return;

    // Find min/max values for scaling
    double minV = *std::min_element(voltages.begin(), voltages.end());
    double maxV = *std::max_element(voltages.begin(), voltages.end());
    double minT = *std::min_element(times.begin(), times.end());
    double maxT = *std::max_element(times.begin(), times.end());

    // Draw axes
    SDL_SetRenderDrawColor(renderer, BLACK.r, BLACK.g, BLACK.b, 255);
    SDL_RenderDrawLine(renderer, area.x, area.y + area.h/2, area.x + area.w, area.y + area.h/2); // X-axis
    SDL_RenderDrawLine(renderer, area.x, area.y, area.x, area.y + area.h); // Y-axis

    // Plot points
    for (size_t i = 1; i < times.size(); i++) {
        int x1 = area.x + static_cast<int>((times[i-1] - minT) / (maxT - minT) * area.w);
        int y1 = area.y + area.h - static_cast<int>((voltages[i-1] - minV) / (maxV - minV) * area.h);
        int x2 = area.x + static_cast<int>((times[i] - minT) / (maxT - minT) * area.w);
        int y2 = area.y + area.h - static_cast<int>((voltages[i] - minV) / (maxV - minV) * area.h);

        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
    }
}

void showAnalysisDialog(SDL_Renderer* renderer, double& tStep, double& tStop) {
    SDL_Rect dialogRect = {300, 200, 400, 300};
    SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
    SDL_RenderFillRect(renderer, &dialogRect);
    SDL_SetRenderDrawColor(renderer, BLACK.r, BLACK.g, BLACK.b, 255);
    SDL_RenderDrawRect(renderer, &dialogRect);

    renderText("Analysis Settings", dialogRect.x + 10, dialogRect.y + 10, BLACK);

    renderText("Time Step:", dialogRect.x + 20, dialogRect.y + 50, BLACK);
    TextBox stepBox = {{dialogRect.x + 150, dialogRect.y + 50, 100, 30}, std::to_string(tStep), false};
    renderTextBox(stepBox);

    renderText("Stop Time:", dialogRect.x + 20, dialogRect.y + 100, BLACK);
    TextBox stopBox = {{dialogRect.x + 150, dialogRect.y + 100, 100, 30}, std::to_string(tStop), false};
    renderTextBox(stopBox);

    Button runBtn = {{dialogRect.x + 150, dialogRect.y + 150, 100, 40}, "Run", GREEN, false};
    renderButton(runBtn);

    // Handle input and button clicks here...
}

enum AppState {
    MAIN_VIEW,
    COMPONENT_LIBRARY,
    ANALYSIS_SETTINGS,
    PLOT_VIEW
};

AppState currentState = MAIN_VIEW;

void handleProbe(int x, int y, const Circuit& circuit) {
    // Check if click is in circuit area
    if (x >= circuitArea.x && x <= circuitArea.x + circuitArea.w &&
        y >= circuitArea.y && y <= circuitArea.y + circuitArea.h) {

        // Find the closest component/node
        // This is simplified - you'll need proper hit detection
        for (const auto& comp : circuit.listComponents()) {
            // Check if click is near this component
            // If yes, show voltage/current information
        }
        }
}

void showFileMenu(SDL_Renderer* renderer, SDL_Rect menuRect) {
    // Draw menu background
    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
    SDL_RenderFillRect(renderer, &menuRect);

    // Menu options
    Button newBtn = {menuRect.x + 10, menuRect.y + 40, 120, 30, "New Circuit", GRAY};
    Button openBtn = {menuRect.x + 10, menuRect.y + 80, 120, 30, "Open", GRAY};
    Button saveBtn = {menuRect.x + 10, menuRect.y + 120, 120, 30, "Save", GRAY};
    Button saveAsBtn = {menuRect.x + 10, menuRect.y + 160, 120, 30, "Save As", GRAY};
    Button exitBtn = {menuRect.x + 10, menuRect.y + 200, 120, 30, "Exit", RED};

    // Draw all buttons
    renderButton(newBtn);
    renderButton(openBtn);
    renderButton(saveBtn);
    renderButton(saveAsBtn);
    renderButton(exitBtn);
}

void showEditMenu(SDL_Renderer* renderer, SDL_Rect menuRect) {
    // Draw menu background
    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
    SDL_RenderFillRect(renderer, &menuRect);

    // Menu options
    Button undoBtn = {menuRect.x + 10, menuRect.y + 40, 120, 30, "Undo", GRAY};
    Button redoBtn = {menuRect.x + 10, menuRect.y + 80, 120, 30, "Redo", GRAY};
    Button copyBtn = {menuRect.x + 10, menuRect.y + 120, 120, 30, "Copy", GRAY};
    Button pasteBtn = {menuRect.x + 10, menuRect.y + 160, 120, 30, "Paste", GRAY};
    Button deleteBtn = {menuRect.x + 10, menuRect.y + 200, 120, 30, "Delete", RED};

    // Draw all buttons
    renderButton(undoBtn);
    renderButton(redoBtn);
    renderButton(copyBtn);
    renderButton(pasteBtn);
    renderButton(deleteBtn);
}

void showAnalysisSettings(SDL_Renderer* renderer, double* tStep, double* tStop) {
    SDL_Rect analysisWindow = {300, 150, 350, 300};

    // Window background
    SDL_SetRenderDrawColor(renderer, 230, 230, 250, 255);
    SDL_RenderFillRect(renderer, &analysisWindow);

    // Title
    renderText("Analysis Settings", analysisWindow.x + 20, analysisWindow.y + 20, BLACK);

    // Time step input
    renderText("Time Step (s):", analysisWindow.x + 20, analysisWindow.y + 60, BLACK);
    TextBox stepBox = {analysisWindow.x + 150, analysisWindow.y + 60, 150, 30, std::to_string(*tStep)};
    renderTextBox(stepBox);

    // Stop time input
    renderText("Stop Time (s):", analysisWindow.x + 20, analysisWindow.y + 110, BLACK);
    TextBox stopBox = {analysisWindow.x + 150, analysisWindow.y + 110, 150, 30, std::to_string(*tStop)};
    renderTextBox(stopBox);

    // Analysis type
    renderText("Analysis Type:", analysisWindow.x + 20, analysisWindow.y + 160, BLACK);
    Button dcBtn = {analysisWindow.x + 150, analysisWindow.y + 160, 80, 30, "DC", GRAY};
    Button tranBtn = {analysisWindow.x + 240, analysisWindow.y + 160, 80, 30, "TRAN", BLUE};
    renderButton(dcBtn);
    renderButton(tranBtn);

    // Action buttons
    Button runBtn = {analysisWindow.x + 50, analysisWindow.y + 220, 100, 40, "Run", GREEN};
    Button cancelBtn = {analysisWindow.x + 200, analysisWindow.y + 220, 100, 40, "Cancel", RED};
    renderButton(runBtn);
    renderButton(cancelBtn);
}

void showComponentLibrary(SDL_Renderer* renderer) {
    SDL_Rect libWindow = {200, 100, 400, 500};

    // Window background
    SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);
    SDL_RenderFillRect(renderer, &libWindow);

    // Title
    renderText("Component Library", libWindow.x + 20, libWindow.y + 20, BLACK);

    // Component categories
    const char* categories[] = {"Passives", "Sources", "Semiconductors", "Dependent Sources"};
    SDL_Rect categoryRects[4];

    for (int i = 0; i < 4; i++) {
        categoryRects[i] = {libWindow.x + 20, libWindow.y + 60 + i*50, 360, 40};
        SDL_SetRenderDrawColor(renderer, 200, 200, 255, 255);
        SDL_RenderFillRect(renderer, &categoryRects[i]);
        renderText(categories[i], categoryRects[i].x + 10, categoryRects[i].y + 10, BLACK);
    }

    // Close button
    Button closeBtn = {libWindow.x + libWindow.w - 40, libWindow.y + 10, 30, 30, "X", RED};
    renderButton(closeBtn);
}

void showComponentProperties(SDL_Renderer* renderer, Component* component) {
    if (!component) return;

    SDL_Rect propWindow = {600, 100, 350, 400};

    // Window background
    SDL_SetRenderDrawColor(renderer, 230, 250, 230, 255);
    SDL_RenderFillRect(renderer, &propWindow);

    // Title
    renderText("Component Properties", propWindow.x + 20, propWindow.y + 20, BLACK);

    // Component info
    //renderText("Type: " + component->getTypeString(), propWindow.x + 20, propWindow.y + 60, BLACK);
    renderText("Name: " + component->name, propWindow.x + 20, propWindow.y + 90, BLACK);

    // Node connections
    renderText("Connections:", propWindow.x + 20, propWindow.y + 120, BLACK);
    renderText("Node 1: " + getNodeName(component->node1), propWindow.x + 40, propWindow.y + 150, BLACK);
    renderText("Node 2: " + getNodeName(component->node2), propWindow.x + 40, propWindow.y + 180, BLACK);

    // Value editing
    if (component->type != GROUND && component->type != DIODE) {
        renderText("Value:", propWindow.x + 20, propWindow.y + 210, BLACK);
        TextBox valueBox = {propWindow.x + 100, propWindow.y + 210, 150, 30, std::to_string(component->value)};
        renderTextBox(valueBox);
    }

    // Special parameters for sources
    if (component->type == SIN_VOLTAGE_SOURCE || component->type == SIN_CURRENT_SOURCE) {
        //SinSource* src = dynamic_cast<SinSource*>(component);
        renderText("Amplitude:", propWindow.x + 20, propWindow.y + 250, BLACK);
        //TextBox ampBox = {propWindow.x + 120, propWindow.y + 250, 150, 30, std::to_string(src->amplitude)};
        //renderTextBox(ampBox);
    }

    // Action buttons
    Button saveBtn = {propWindow.x + 50, propWindow.y + 320, 100, 40, "Save", GREEN};
    Button deleteBtn = {propWindow.x + 200, propWindow.y + 320, 100, 40, "Delete", RED};
    renderButton(saveBtn);
    renderButton(deleteBtn);
}

void showOpenFileDialog(SDL_Renderer* renderer, vector<string>& files) {
    SDL_Rect dialog = {200, 150, 400, 400};

    // Window background
    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
    SDL_RenderFillRect(renderer, &dialog);

    // Title
    renderText("Open Circuit File", dialog.x + 20, dialog.y + 20, BLACK);

    // File list
    SDL_Rect fileList = {dialog.x + 20, dialog.y + 60, 360, 250};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &fileList);

    // Render files
    for (size_t i = 0; i < files.size(); i++) {
        SDL_Rect fileRect = {fileList.x + 10, fileList.y + 10 + (int)i*30, fileList.w - 20, 25};
        SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
        SDL_RenderFillRect(renderer, &fileRect);
        renderText(files[i], fileRect.x + 5, fileRect.y + 5, BLACK);
    }

    // Action buttons
    Button openBtn = {dialog.x + 100, dialog.y + 330, 100, 40, "Open", GREEN};
    Button cancelBtn = {dialog.x + 220, dialog.y + 330, 100, 40, "Cancel", RED};
    renderButton(openBtn);
    renderButton(cancelBtn);
}

void showSaveAsDialog(SDL_Renderer* renderer, string& currentFilename) {
    SDL_Rect dialog = {250, 200, 300, 200};

    // Window background
    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
    SDL_RenderFillRect(renderer, &dialog);

    // Title
    renderText("Save Circuit As", dialog.x + 20, dialog.y + 20, BLACK);

    // Filename input
    renderText("Filename:", dialog.x + 20, dialog.y + 60, BLACK);
    TextBox nameBox = {dialog.x + 100, dialog.y + 60, 180, 30, currentFilename};
    renderTextBox(nameBox);

    // Action buttons
    Button saveBtn = {dialog.x + 50, dialog.y + 120, 100, 40, "Save", GREEN};
    Button cancelBtn = {dialog.x + 170, dialog.y + 120, 100, 40, "Cancel", RED};
    renderButton(saveBtn);
    renderButton(cancelBtn);
}

void showResultsWindow(SDL_Renderer* renderer, const vector<double>& voltages,
                      const vector<double>& times, const Circuit& circuit) {
    SDL_Rect resultsWindow = {150, 100, 600, 500};

    // Window background
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &resultsWindow);

    // Title
    renderText("Simulation Results", resultsWindow.x + 20, resultsWindow.y + 20, BLACK);

    // Plot area
    SDL_Rect plotArea = {resultsWindow.x + 50, resultsWindow.y + 60, 500, 300};
    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
    SDL_RenderFillRect(renderer, &plotArea);

    // Draw plot (using your existing plotSignals function)
    plotSignals(renderer, voltages, times, plotArea);

    // Node voltage table
    SDL_Rect tableArea = {resultsWindow.x + 50, resultsWindow.y + 380, 500, 100};
    SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);
    SDL_RenderFillRect(renderer, &tableArea);

    // Close button
    Button closeBtn = {resultsWindow.x + resultsWindow.w - 50, resultsWindow.y + 10, 40, 40, "X", RED};
    renderButton(closeBtn);
}

void drawToolbar(SDL_Renderer* renderer) {
    SDL_Rect toolbar = {0, 0, SCREEN_WIDTH, 40};
    SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
    SDL_RenderFillRect(renderer, &toolbar);

    Button fileBtn = {10, 5, 80, 30, "File", GRAY, false};
    Button editBtn = {100, 5, 80, 30, "Edit", GRAY, false};
    Button viewBtn = {190, 5, 80, 30, "View", GRAY, false};
    Button analyzeBtn = {280, 5, 80, 30, "Analyze", GRAY, false};
    Button toolsBtn = {370, 5, 80, 30, "Tools", GRAY, false};

    renderButton(fileBtn);
    renderButton(editBtn);
    renderButton(viewBtn);
    renderButton(analyzeBtn);
    renderButton(toolsBtn);
}

void drawStatusBar(SDL_Renderer* renderer, const std::string& message) {
    SDL_Rect statusBar = {0, SCREEN_HEIGHT - 30, SCREEN_WIDTH, 30};
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderFillRect(renderer, &statusBar);

    renderText(message, 10, SCREEN_HEIGHT - 25, BLACK);
}

bool isPointNearLine(int px, int py, int x1, int y1, int x2, int y2, int threshold) {
    // Calculate distance from point to line segment
    float lineLength = sqrtf((x2-x1)*(x2-x1) + (y2-y1)*(y2-y1));
    if (lineLength == 0) return false;  // Not a line

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

void handleComponentSelection(Circuit* circuit, int x, int y) {
    if (!circuit) return;

    // First make sure positions are calculated
    calculateNodePositions(*circuit);

    // Check if click is in circuit area
    if (x >= circuitArea.x && x <= circuitArea.x + circuitArea.w &&
        y >= circuitArea.y && y <= circuitArea.y + circuitArea.h) {

        // Check component clicks
        for (const auto& comp : circuit->getComponents()) {
            SDL_Point p1 = nodePositions[comp->node1];
            SDL_Point p2 = nodePositions[comp->node2];

            // Simple line intersection test (for wires)
            // In a real app you'd use proper hit testing for each component type
            if (isPointNearLine(x, y, p1.x, p1.y, p2.x, p2.y, 10)) {
                cout << "Selected component: " << comp->getInfo() << endl;
                return;
            }
        }

        // Check node clicks
        for (const auto& node : circuit->getNodeMap()) {
            SDL_Point pos = nodePositions[node.second];
            if (abs(x - pos.x) < 10 && abs(y - pos.y) < 10) {
                cout << "Selected node: " << node.first << " (Node " << node.second << ")" << endl;
                return;
            }
        }
        }
}

void handleMainViewClick(Circuit circuit, int x, int y) {
    if (isMouseOver(circuitArea, x, y)) {
        selectedComponent = nullptr;
        calculateNodePositions(circuit);

        for (const auto& comp : circuit.getComponents()) {
            SDL_Point p1 = nodePositions[comp->node1];
            SDL_Point p2 = nodePositions[comp->node2];

            if (isPointNearLine(x, y, p1.x, p1.y, p2.x, p2.y, 10)) {
                selectedComponent = comp;
                break;
            }
        }
    }
}

void handleLibraryClick(int x, int y) {
    SDL_Rect libWindow = {200, 100, 400, 500};

    // Check close button
    SDL_Rect closeBtn = {libWindow.x + libWindow.w - 40, libWindow.y + 10, 30, 30};
    if (isMouseOver(closeBtn, x, y)) {
        currentState = MAIN_VIEW;
        return;
    }

    // Check category clicks
    for (int i = 0; i < 4; i++) {
        SDL_Rect catRect = {libWindow.x + 20, libWindow.y + 60 + i*60, 360, 50};
        if (isMouseOver(catRect, x, y)) {
            cout << "Selected category: " << i << endl;
            // Here you would add the selected component to the circuit
            currentState = MAIN_VIEW;
            break;
        }
    }
}

void handlePlotClick(int x, int y) {
    SDL_Rect backBtn = {50, 50, 100, 40};
    if (isMouseOver(backBtn, x, y)) {
        currentState = MAIN_VIEW;
    }
}

void handleToolbarClick(int x, int y) {
    if (isMouseOver(fileBtn.rect, x, y)) {
        // File menu handled separately
    }
    else if (isMouseOver(componentLibBtn.rect, x, y)) {
        currentState = COMPONENT_LIBRARY;
    }
    else if (isMouseOver(analyzeBtn.rect, x, y)) {
        currentState = ANALYSIS_SETTINGS;
    }
    else if (isMouseOver(plotBtn.rect, x, y)) {
        currentState = PLOT_VIEW;
    }
}

void handleAnalysisClick(int x, int y) {
    SDL_Rect analysisWindow = {250, 150, 350, 300};

    // Check if click is in analysis window
    if (!isMouseOver(analysisWindow, x, y)) {
        currentState = MAIN_VIEW;
        return;
    }

    // Check Run button
    SDL_Rect runBtn = {analysisWindow.x + 50, analysisWindow.y + 220, 100, 40};
    if (isMouseOver(runBtn, x, y)) {
        currentState = PLOT_VIEW;
        // Start analysis here
    }

    // Check Cancel button
    SDL_Rect cancelBtn = {analysisWindow.x + 200, analysisWindow.y + 220, 100, 40};
    if (isMouseOver(cancelBtn, x, y)) {
        currentState = MAIN_VIEW;
    }
}

void handleMouseClick(Circuit circuit, int x, int y) {
    switch (currentState) {
        case MAIN_VIEW:
            handleMainViewClick(circuit, x, y);
        break;
        case COMPONENT_LIBRARY:
            handleLibraryClick(x, y);
        break;
        case ANALYSIS_SETTINGS:
            handleAnalysisClick(x, y);
                break;
        case PLOT_VIEW:
            handlePlotClick(x, y);
        break;
    }
}

void handleAnalyzeButton(Circuit* circuit) {
    if (!circuit) return;

    try {
        // Default analysis parameters
        double tStep = 0.001;  // 1ms
        double tStop = 0.1;    // 100ms

        cout << "Running transient analysis..." << endl;
        circuit->analyzeTransient(tStep, tStop);
        circuit->printTransientResults(Vtimes, voltages, currents, circuit->listNodes().size() - 1);
    } catch (const exception& e) {
        cout << "Analysis error: " << e.what() << endl;
    }
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
    // For simplicity, we'll just reload the current file
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

void handleToolbarButton(int x, int y) {
    // Check which toolbar button was clicked
    if (x >= 10 && x <= 90 && y >= 5 && y <= 35) { // File
        currentState = (currentState == MAIN_VIEW) ? COMPONENT_LIBRARY : MAIN_VIEW;
    }
    else if (x >= 100 && x <= 180 && y >= 5 && y <= 35) { // Edit
        // Handle edit operations
    }
    else if (x >= 190 && x <= 270 && y >= 5 && y <= 35) { // View
        showVoltage = !showVoltage;
        showCurrent = !showCurrent;
    }
    else if (x >= 280 && x <= 360 && y >= 5 && y <= 35) { // Analyze
        currentState = ANALYSIS_SETTINGS;
    }
    else if (x >= 370 && x <= 450 && y >= 5 && y <= 35) { // Tools
        // Show tools menu
    }
}

void renderMainView(SDL_Renderer* renderer, Circuit& circuit) {
    // Draw the main circuit view
    drawCircuit(circuit, renderer);

    // Show component properties if one is selected
    if (selectedComponent) {
        showComponentProperties(renderer, selectedComponent);
    }
}

void renderComponentLibrary(SDL_Renderer* renderer) {
    SDL_Rect libWindow = {200, 100, 400, 500};

    // Window background
    SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);
    SDL_RenderFillRect(renderer, &libWindow);

    // Title
    renderText("Component Library", libWindow.x + 20, libWindow.y + 20, BLACK);

    // Component categories
    const char* categories[] = {"Passives", "Sources", "Semiconductors", "Custom"};
    for (int i = 0; i < 4; i++) {
        SDL_Rect catRect = {libWindow.x + 20, libWindow.y + 60 + i*60, 360, 50};
        SDL_SetRenderDrawColor(renderer, 200, 200, 255, 255);
        SDL_RenderFillRect(renderer, &catRect);
        renderText(categories[i], catRect.x + 10, catRect.y + 15, BLACK);
    }

    // Close button
    Button closeBtn = {libWindow.x + libWindow.w - 40, libWindow.y + 10, 30, 30, "X", RED};
    renderButton(closeBtn);
}

void renderAnalysisSettings(SDL_Renderer* renderer) {
    SDL_Rect analysisWindow = {250, 150, 350, 300};

    // Window background
    SDL_SetRenderDrawColor(renderer, 230, 230, 250, 255);
    SDL_RenderFillRect(renderer, &analysisWindow);

    // Title
    renderText("Analysis Settings", analysisWindow.x + 20, analysisWindow.y + 20, BLACK);

    // Input fields
    renderText("Time Step (s):", analysisWindow.x + 20, analysisWindow.y + 60, BLACK);
    TextBox stepBox = {analysisWindow.x + 150, analysisWindow.y + 60, 150, 30, std::to_string(tStep)};
    renderTextBox(stepBox);

    renderText("Stop Time (s):", analysisWindow.x + 20, analysisWindow.y + 110, BLACK);
    TextBox stopBox = {analysisWindow.x + 150, analysisWindow.y + 110, 150, 30, std::to_string(tStop)};
    renderTextBox(stopBox);

    // Action buttons
    Button runBtn = {analysisWindow.x + 50, analysisWindow.y + 220, 100, 40, "Run", GREEN};
    Button cancelBtn = {analysisWindow.x + 200, analysisWindow.y + 220, 100, 40, "Cancel", RED};
    renderButton(runBtn);
    renderButton(cancelBtn);
}

void renderPlotView(SDL_Renderer* renderer) {
    // Clear with a light gray background
    SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
    SDL_RenderClear(renderer);

    // Plot area
    SDL_Rect plotArea = {100, 100, SCREEN_WIDTH-200, SCREEN_HEIGHT-200};
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &plotArea);

    // Draw plot if we have data
    if (!voltages.empty()) {
        plotSignals(renderer, voltages, Vtimes, plotArea);
    }

    // Back button
    Button backBtn = {50, 50, 100, 40, "Back", GRAY};
    renderButton(backBtn);
}

int main(int argc, char* argv[]) {
    changeToPreviousDirectory();

    string command;
    string currentCircuitFile = "";
    Circuit* circuit = new Circuit();

    FileMenu = false;
    ComponentLibrary = false;
    AnalysisSettings = false;
    selectedComponent = nullptr;

    showSaveMenu();

    if (!initSDL()) {
        return 1;
    }

    bool running = true;
    SDL_Event event;

    Button addCompBtn = {{20, 20, 150, 40}, "Add Component", GREEN, false};
    Button analyzeBtn = {{20, 70, 150, 40}, "Analyze", BLUE, false};
    Button saveBtn = {{20, 120, 150, 40}, "Save", GRAY, false};
    Button loadBtn = {{20, 170, 150, 40}, "Load", GRAY, false};

    TextBox node1Box = {{200, 20, 100, 40}, "Node1", false};
    TextBox node2Box = {{310, 20, 100, 40}, "Node2", false};
    TextBox valueBox = {{420, 20, 100, 40}, "Value", false};

    while (running) {
        while (SDL_PollEvent(&event)) {

            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_F5:
                        handleAnalyzeButton(circuit);
                    break;
                    case SDLK_s:
                        if (SDL_GetModState() & KMOD_CTRL) {
                            handleSaveButton(circuit, currentCircuitFile);
                        }
                    break;
                    case SDLK_l:
                        if (SDL_GetModState() & KMOD_CTRL) {
                            handleLoadButton(circuit, currentCircuitFile);
                        }
                    break;
                    case SDLK_ESCAPE:
                            break;
                }
            }

            if (event.type == SDL_MOUSEBUTTONDOWN) {
                int x, y;
                SDL_GetMouseState(&x, &y);

                if (y <= 40) {
                    handleToolbarButton(x, y);
                }
                else if (isMouseOver(circuitArea, x, y)) {
                    handleComponentSelection(circuit, x, y);
                }
                else if (isMouseOver(analyzeBtn.rect, x, y)) {
                    handleAnalyzeButton(circuit);
                }
                else if (isMouseOver(saveBtn.rect, x, y)) {
                    handleSaveButton(circuit, currentCircuitFile);
                }
                else if (isMouseOver(loadBtn.rect, x, y)) {
                    handleLoadButton(circuit, currentCircuitFile);
                }
                else if (isMouseOver(node1Box.rect, x, y)) {
                    node1Box.isActive = true;
                    node2Box.isActive = false;
                    valueBox.isActive = false;
                }
                else if (isMouseOver(node2Box.rect, x, y)) {
                    node1Box.isActive = false;
                    node2Box.isActive = true;
                    valueBox.isActive = false;
                }
                else if (isMouseOver(valueBox.rect, x, y)) {
                    node1Box.isActive = false;
                    node2Box.isActive = false;
                    valueBox.isActive = true;
                }
                else {
                    node1Box.isActive = false;
                    node2Box.isActive = false;
                    valueBox.isActive = false;
                }
            }

            if (event.type == SDL_TEXTINPUT) {
                if (node1Box.isActive) {
                    node1Box.text += event.text.text;
                }
                else if (node2Box.isActive) {
                    node2Box.text += event.text.text;
                }
                else if (valueBox.isActive) {
                    if (isdigit(event.text.text[0]) || event.text.text[0] == '.') {
                        valueBox.text += event.text.text;
                    }
                }
            }

            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_BACKSPACE) {
                if (node1Box.isActive && !node1Box.text.empty()) {
                    node1Box.text.pop_back();
                }
                else if (node2Box.isActive && !node2Box.text.empty()) {
                    node2Box.text.pop_back();
                }
                else if (valueBox.isActive && !valueBox.text.empty()) {
                    valueBox.text.pop_back();
                }
            }

            if (event.type == SDL_QUIT) {
                running = false;
            }

            if (showFileMenu) showFileMenu(renderer, fileMenuRect);
            if (showComponentLibrary) showComponentLibrary(renderer);
            if (showAnalysisSettings) showAnalysisSettings(renderer, &tStep, &tStop);
            if (selectedComponent) showComponentProperties(renderer, selectedComponent);

            if (!voltages.empty()) {
                plotSignals(renderer, voltages, Vtimes, plotArea);
            }
        }

        SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
        SDL_RenderClear(renderer);


        SDL_SetRenderDrawColor(renderer, WHITE.r, WHITE.g, WHITE.b, 255);
        SDL_RenderFillRect(renderer, &circuitArea);
        SDL_SetRenderDrawColor(renderer, BLACK.r, BLACK.g, BLACK.b, 255);
        SDL_RenderDrawRect(renderer, &circuitArea);

        SDL_SetRenderDrawColor(renderer, WHITE.r, WHITE.g, WHITE.b, 255);
        SDL_RenderFillRect(renderer, &plotArea);
        SDL_SetRenderDrawColor(renderer, BLACK.r, BLACK.g, BLACK.b, 255);
        SDL_RenderDrawRect(renderer, &plotArea);

        renderButton(addCompBtn);
        renderButton(analyzeBtn);
        renderButton(saveBtn);
        renderButton(loadBtn);

        renderTextBox(node1Box);
        renderTextBox(node2Box);
        renderTextBox(valueBox);

        drawToolbar(renderer);

        switch (currentState) {
            case MAIN_VIEW:
                renderMainView(renderer, *circuit);
            break;
            case COMPONENT_LIBRARY:
                renderComponentLibrary(renderer);
            break;
            case ANALYSIS_SETTINGS:
                renderAnalysisSettings(renderer);
            break;
            case PLOT_VIEW:
                renderPlotView(renderer);
            break;
        }

        drawStatusBar(renderer, "Ready");

        SDL_RenderPresent(renderer);
    }

    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    /*while (true) {
        if (inCircuitMode) {
            cout << "[" << currentCircuitFile << "] >>> ";
        } else {
            cout << "Saves> ";
        }

        if (!getline(cin, command)) {
            break;
        }
        if (command.empty()) continue;

        istringstream iss(command);
        string cmd;
        iss >> cmd;
        transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

        // --- STATE 1: SAVE/LOAD MENU ---
        if (!inCircuitMode) {
            if (cmd == "list") {
                cout << "Available circuit files (.txt) in current directory:\n";
                vector<string> files = listTxtFiles(".");
                if (files.empty()) {
                    cout << "  No .txt files found.\n";
                } else {
                    for (size_t i = 0; i < files.size(); i++) {
                        cout << "  [" << i + 1 << "] " << files[i] << endl;
                    }
                }
            } else if (cmd == "load") {
                string filename;
                if (iss >> filename) {
                    resetGlobalState();
                    circuit = new Circuit();

                    if (all_of(filename.begin(), filename.end(), ::isdigit)) {
                        vector<string> files = listTxtFiles(".");
                        try {
                            int index = stoi(filename) - 1;
                            if (index >= 0 && index < files.size()) {
                                filename = files[index];
                            } else {
                                throw out_of_range("Invalid index");
                            }
                        } catch (const exception&) {
                            cout << "ERROR: Invalid file number.\n";
                            delete circuit;
                            circuit = nullptr;
                            continue;
                        }
                    }

                    if (filename.find(".txt") == string::npos) {
                        filename += ".txt";
                    }

                    processCircuitFile(filename, *circuit);
                    currentCircuitFile = filename;
                    inCircuitMode = true;
                    cout << "SUCCESS: Loaded '" << currentCircuitFile << "'. Entering circuit mode.\n";
                    showCircuitHelp();

                } else {
                    cout << "ERROR: Missing filename. Usage: load <name|num>\n";
                }
            } else if (cmd == "create") {
                string filename;
                if (iss >> filename) {
                    resetGlobalState();
                    delete circuit;
                    circuit = new Circuit();

                    if (filename.find(".txt") == string::npos) {
                        filename += ".txt";
                    }

                    ofstream testFile(filename);
                    if (!testFile.is_open()) {
                        cout << "ERROR: Cannot create file '" << filename << "'. Check permissions.\n";
                        delete circuit;
                        circuit = nullptr;
                        continue;
                    }
                    testFile.close();

                    currentCircuitFile = filename;
                    inCircuitMode = true;
                    cout << "SUCCESS: Created new circuit '" << currentCircuitFile << "'. Entering circuit mode.\n";
                    showCircuitHelp();
                } else {
                    cout << "ERROR: Missing filename. Usage: create <name>\n";
                }
            } else if (cmd == "exit") {
                break;
            } else {
                cout << "ERROR: Invalid command in Save Menu. Use 'list', 'load', 'create', or 'exit'.\n";
            }
        }
        // --- STATE 2: CIRCUIT EDIT/ANALYSIS MENU ---
        else {
            if (cmd == "return") {
                inCircuitMode = false;
                currentCircuitFile = "";
                delete circuit;
                circuit = nullptr;
                resetGlobalState();
                cout << "Returning to Save & Load Menu.\n";
                showSaveMenu();
                continue;
            } else if (cmd == "exit") {
                break;
            } else if (cmd == "help") {
                showCircuitHelp();
                continue;
            } else if (cmd == "save") {
                string filename;
                iss >> filename;
                if (filename.empty()) {
                    filename = currentCircuitFile;
                }
                if (filename.find(".txt") == string::npos) {
                    filename += ".txt";
                }
                if (saveCircuitToFile(*circuit, filename)) {
                    cout << "SUCCESS: Circuit saved to " << filename << endl;
                    currentCircuitFile = filename;
                } else {
                    cout << "ERROR: Could not save to file " << filename << endl;
                }
            } else if (cmd == "analyze") {
                string analysisType;
                iss >> analysisType;
                transform(analysisType.begin(), analysisType.end(), analysisType.begin(), ::tolower);

                try {
                    if (!circuit->hasGround()) {
                        throw runtime_error("No ground node detected in the circuit");
                    }

                    if (analysisType.empty() || analysisType == "dc") {
                        string sweep_keyword;
                        if(iss >> sweep_keyword) {
                            transform(sweep_keyword.begin(), sweep_keyword.end(), sweep_keyword.begin(), ::tolower);
                            if(sweep_keyword == "sweep") {
                                string sourceName;
                                double start, stop, step;
                                if (!(iss >> sourceName >> start >> stop >> step)) {
                                    cout << "ERROR: Missing params for DC sweep. Usage: analyze DC SWEEP <src> <start> <stop> <step>\n";
                                    continue;
                                }
                                if (step == 0 || (step > 0 && start > stop) || (step < 0 && start < stop)) {
                                    cout << "ERROR: Invalid sweep parameters.\n";
                                    continue;
                                }
                                circuit->analyzeDCSweep(sourceName, start, stop, step);
                            }
                        } else {
                           circuit->analyzeDC();
                        }
                    } else if (analysisType == "tran") {
                        double tStep, tStop;
                        if (!(iss >> tStep >> tStop)) {
                            cout << "ERROR: Missing time parameters for transient analysis. Usage: analyze TRAN <tstep> <tstop>\n";
                            continue;
                        }
                        if (tStep <= 0 || tStop <= 0 || tStep > tStop) {
                            cout << "ERROR: Time parameters must be positive and tStep <= tStop.\n";
                            continue;
                        }
                        circuit->analyzeTransient(tStep, tStop);
                        circuit->printTransientResults(Vtimes, voltages, currents, circuit->listNodes().size() - 1);
                    } else {
                        cout << "ERROR: Unknown analysis type '" << analysisType << "'. Use 'DC' or 'TRAN'.\n";
                    }
                } catch (const runtime_error& e) {
                    cout << "ERROR: " << e.what() << "\n";
                }
            } else if (cmd == "add") {
                string typeName;
                if (!(iss >> typeName)) { cout << "ERROR: Missing component type. Usage: add <type><name> ...\n"; continue; }

                string name = typeName;
                char typeChar = toupper(typeName[0]);
                try {
                    if (circuit->hasComponent(name)) {
                        cout << "ERROR: Component '" << name << "' already exists.\n"; continue;
                    }

                    if (name == "GND" || name == "gnd") {
                        string node;
                        if (!(iss >> node)) { cout << "ERROR: Missing node for ground. Usage: add GND <node>\n"; continue; }
                        int n = getOrCreateNode(node);
                        circuit->addComponent(new Ground("GND", n));
                        cout << "SUCCESS: Ground connection added to node " << node << "\n";
                    }
                    // --- Passive Components ---
                    else if (typeChar == 'R') {
                        string n1_str, n2_str, val_str;
                        if (!(iss >> n1_str >> n2_str >> val_str)) { cout << "ERROR: Usage: add R<name> <node1> <node2> <value>\n"; continue; }
                        int n1 = getOrCreateNode(n1_str), n2 = getOrCreateNode(n2_str);
                        double value = parseSpiceValue(val_str);
                        if (value <= 0) { cout << "ERROR: Resistance must be positive.\n"; continue; }
                        circuit->addComponent(new Resistor(name, n1, n2, value));
                        cout << "SUCCESS: Resistor '" << name << "' added.\n";
                    } else if (typeChar == 'C') {
                        string n1_str, n2_str, val_str;
                        if (!(iss >> n1_str >> n2_str >> val_str)) { cout << "ERROR: Usage: add C<name> <node1> <node2> <value>\n"; continue; }
                        int n1 = getOrCreateNode(n1_str), n2 = getOrCreateNode(n2_str);
                        double value = parseSpiceValue(val_str);
                        if (value <= 0) { cout << "ERROR: Capacitance must be positive.\n"; continue; }
                        circuit->addComponent(new Capacitor(name, n1, n2, value));
                        cout << "SUCCESS: Capacitor '" << name << "' added.\n";
                    } else if (typeChar == 'L') {
                        string n1_str, n2_str, val_str;
                        if (!(iss >> n1_str >> n2_str >> val_str)) { cout << "ERROR: Usage: add L<name> <node1> <node2> <value>\n"; continue; }
                        int n1 = getOrCreateNode(n1_str), n2 = getOrCreateNode(n2_str);
                        double value = parseSpiceValue(val_str);
                        if (value <= 0) { cout << "ERROR: Inductance must be positive.\n"; continue; }
                        circuit->addComponent(new Inductor(name, n1, n2, value));
                        cout << "SUCCESS: Inductor '" << name << "' added.\n";
                    } else if (typeChar == 'D') {
                        string n1_str, n2_str;
                        if (!(iss >> n1_str >> n2_str)) { cout << "ERROR: Usage: add D<name> <node1> <node2>\n"; continue; }
                        int n1 = getOrCreateNode(n1_str), n2 = getOrCreateNode(n2_str);
                        circuit->addComponent(new Diode(name, n1, n2));
                        cout << "SUCCESS: Diode '" << name << "' added.\n";
                    }
                    // --- Independent Sources ---
                    else if (typeChar == 'V') {
                        if (typeName.size() > 1 && toupper(typeName[1]) == 'S') {
                            string n1_str, n2_str, dc_str, amp_str, freq_str, phase_str = "0";
                            if (!(iss >> n1_str >> n2_str >> dc_str >> amp_str >> freq_str)) {
                                cout << "ERROR: Usage: add VS<name> <n1> <n2> <DC> <AMP> <FREQ> [PHASE]\n"; continue;
                            }
                            iss >> phase_str;
                            double dc = parseSpiceValue(dc_str);
                            double amp = parseSpiceValue(amp_str);
                            double freq = parseSpiceValue(freq_str);
                            double phase = parseSpiceValue(phase_str);
                            circuit->addComponent(new SinVoltageSource(name, getOrCreateNode(n1_str), getOrCreateNode(n2_str), amp, freq, phase, dc));
                            cout << "SUCCESS: Sinusoidal Voltage Source '" << name << "' added.\n";
                        } else if (typeName.size() > 1 && toupper(typeName[1]) == 'P') {
                            string n1_str, n2_str, v1_str, v2_str, td_str, tr_str, tf_str, pw_str, per_str;
                            if (!(iss >> n1_str >> n2_str >> v1_str >> v2_str >> td_str >> tr_str >> tf_str >> pw_str >> per_str)) {
                                cout << "ERROR: Usage: add VP<name> <n1> <n2> <V1> <V2> <TD> <TR> <TF> <PW> <PER>\n"; continue;
                            }
                            double v1 = parseSpiceValue(v1_str);
                            double v2 = parseSpiceValue(v2_str);
                            double td = parseSpiceValue(td_str);
                            double tr = parseSpiceValue(tr_str);
                            double tf = parseSpiceValue(tf_str);
                            double pw = parseSpiceValue(pw_str);
                            double per = parseSpiceValue(per_str);
                            circuit->addComponent(new PulseVoltageSource(name, getOrCreateNode(n1_str), getOrCreateNode(n2_str),
                                                v1, v2, td, tr, tf, pw, per));
                            cout << "SUCCESS: Pulse Voltage Source '" << name << "' added.\n";
                        } else {
                            string n1_str, n2_str, val_str;
                            if (!(iss >> n1_str >> n2_str >> val_str)) { cout << "ERROR: Usage: add V<name> <node1> <node2> <value>\n"; continue; }
                            circuit->addComponent(new VoltageSource(name, getOrCreateNode(n1_str), getOrCreateNode(n2_str), parseSpiceValue(val_str)));
                            cout << "SUCCESS: DC Voltage Source '" << name << "' added.\n";
                        }
                    } else if (typeChar == 'I') {
                        if (typeName.size() > 1 && toupper(typeName[1]) == 'S') {
                            string n1_str, n2_str, dc_str, amp_str, freq_str, phase_str = "0";
                            if (!(iss >> n1_str >> n2_str >> dc_str >> amp_str >> freq_str)) {
                                cout << "ERROR: Usage: add IS<name> <n1> <n2> <DC> <AMP> <FREQ> [PHASE]\n"; continue;
                            }
                            iss >> phase_str;
                            double dc = parseSpiceValue(dc_str);
                            double amp = parseSpiceValue(amp_str);
                            double freq = parseSpiceValue(freq_str);
                            double phase = parseSpiceValue(phase_str);
                            circuit->addComponent(new SinCurrentSource(name, getOrCreateNode(n1_str), getOrCreateNode(n2_str), amp, freq, phase, dc));
                            cout << "SUCCESS: Sinusoidal Current Source '" << name << "' added.\n";
                        } else if (typeName.size() > 1 && toupper(typeName[1]) == 'P') {
                            string n1_str, n2_str, i1_str, i2_str, td_str, tr_str, tf_str, pw_str, per_str;
                            if (!(iss >> n1_str >> n2_str >> i1_str >> i2_str >> td_str >> tr_str >> tf_str >> pw_str >> per_str)) {
                                cout << "ERROR: Usage: add IP<name> <n1> <n2> <I1> <I2> <TD> <TR> <TF> <PW> <PER>\n"; continue;
                            }
                            double i1 = parseSpiceValue(i1_str);
                            double i2 = parseSpiceValue(i2_str);
                            double td = parseSpiceValue(td_str);
                            double tr = parseSpiceValue(tr_str);
                            double tf = parseSpiceValue(tf_str);
                            double pw = parseSpiceValue(pw_str);
                            double per = parseSpiceValue(per_str);
                            circuit->addComponent(new PulseCurrentSource(name, getOrCreateNode(n1_str), getOrCreateNode(n2_str),
                                                i1, i2, td, tr, tf, pw, per));
                            cout << "SUCCESS: Pulse Current Source '" << name << "' added.\n";
                        } else {
                            string n1_str, n2_str, val_str;
                            if (!(iss >> n1_str >> n2_str >> val_str)) { cout << "ERROR: Usage: add I<name> <node1> <node2> <value>\n"; continue; }
                            circuit->addComponent(new CurrentSource(name, getOrCreateNode(n1_str), getOrCreateNode(n2_str), parseSpiceValue(val_str)));
                            cout << "SUCCESS: DC Current Source '" << name << "' added.\n";
                        }
                    }
                    // --- Dependent Sources ---
                    else if (typeChar == 'E') { // VCVS
                        string n1_str, n2_str, cn1_str, cn2_str, gain_str;
                        if (!(iss >> n1_str >> n2_str >> cn1_str >> cn2_str >> gain_str)) {
                            cout << "ERROR: Usage: add E<name> <n+> <n-> <c_n+> <c_n-> <gain>\n"; continue;
                        }
                        circuit->addComponent(new VCVS(name, getOrCreateNode(n1_str), getOrCreateNode(n2_str),
                                            getOrCreateNode(cn1_str), getOrCreateNode(cn2_str), parseSpiceValue(gain_str)));
                        cout << "SUCCESS: VCVS '" << name << "' added.\n";
                    } else if (typeChar == 'G') { // VCCS
                        string n1_str, n2_str, cn1_str, cn2_str, gm_str;
                        if (!(iss >> n1_str >> n2_str >> cn1_str >> cn2_str >> gm_str)) {
                            cout << "ERROR: Usage: add G<name> <n+> <n-> <c_n+> <c_n-> <transconductance>\n"; continue;
                        }
                        circuit->addComponent(new VCCS(name, getOrCreateNode(n1_str), getOrCreateNode(n2_str),
                                            getOrCreateNode(cn1_str), getOrCreateNode(cn2_str), parseSpiceValue(gm_str)));
                        cout << "SUCCESS: VCCS '" << name << "' added.\n";
                    } else if (typeChar == 'H') { // CCVS
                        string n1_str, n2_str, v_src_name, gain_str;
                        if (!(iss >> n1_str >> n2_str >> v_src_name >> gain_str)) {
                            cout << "ERROR: Usage: add H<name> <n+> <n-> <v_source_name> <gain>\n"; continue;
                        }
                        circuit->addComponent(new CCVS(name, getOrCreateNode(n1_str), getOrCreateNode(n2_str),
                                                    v_src_name, parseSpiceValue(gain_str)));
                        cout << "SUCCESS: CCVS '" << name << "' added.\n";
                    } else if (typeChar == 'F') { // CCCS
                        string n1_str, n2_str, v_src_name, gain_str;
                        if (!(iss >> n1_str >> n2_str >> v_src_name >> gain_str)) {
                            cout << "ERROR: Usage: add F<name> <n+> <n-> <v_source_name> <gain>\n"; continue;
                        }
                        circuit->addComponent(new CCCS(name, getOrCreateNode(n1_str), getOrCreateNode(n2_str),
                                                    v_src_name, parseSpiceValue(gain_str)));
                        cout << "SUCCESS: CCCS '" << name << "' added.\n";
                    }
                    else {
                        cout << "ERROR: Unknown component type '" << typeName << "'.\n";
                    }
                } catch (const exception& e) {
                    cout << "ERROR: Invalid input format. " << e.what() << "\n";
                }
            } else if (cmd == ".list") {
                string filter;
                iss >> filter;
                ComponentType filterType = static_cast<ComponentType>(-1);
                if (!filter.empty()) {
                    if (toupper(filter[0]) == 'R') filterType = RESISTOR;
                    else if (toupper(filter[0]) == 'C') filterType = CAPACITOR;
                    else if (toupper(filter[0]) == 'L') filterType = INDUCTOR;
                    else if (toupper(filter[0]) == 'V') filterType = VOLTAGE_SOURCE;
                }

                auto components = circuit->listComponents(filterType);
                cout << "Components in '" << currentCircuitFile << "':\n";
                if (components.empty()) {
                    cout << "  (No components to display)\n";
                } else {
                    for (const auto& comp : components) {
                        cout << "  " << comp << "\n";
                    }
                }
            } else if (cmd == ".nodes") {
                 auto nodes = circuit->listNodes();
                 cout << "Available nodes in '" << currentCircuitFile << "':\n";
                 for (const auto& node : nodes) {
                     cout << "  " << node << "\n";
                 }
            } else if (cmd == "delete") {
                string name;
                if (!(iss >> name)) { cout << "ERROR: Missing component name.\n"; continue; }
                if (circuit->deleteComponent(name)) {
                    cout << "SUCCESS: Component '" << name << "' deleted.\n";
                } else {
                    cout << "ERROR: Component '" << name << "' not found.\n";
                }
            } else if (cmd == ".rename") {
                 string subcmd, oldName, newName;
                if (!(iss >> subcmd >> oldName >> newName)) { cout << "ERROR: Invalid syntax. Usage: .rename [node|element] <old> <new>\n"; continue; }
                if (subcmd == "node") {
                    if (circuit->renameNode(oldName, newName)) {
                        cout << "SUCCESS: Node renamed from " << oldName << " to " << newName << ".\n";
                    } else { cout << "ERROR: Could not rename node. Check if old name exists and new name is not taken.\n"; }
                } else if (subcmd == "element") {
                    if (circuit->renameComponent(oldName, newName)) {
                        cout << "SUCCESS: Element renamed from " << oldName << " to " << newName << ".\n";
                    } else { cout << "ERROR: Could not rename element. Check if old name exists and new name is not taken.\n"; }
                } else { cout << "ERROR: Invalid subcommand. Use 'node' or 'element'.\n"; }
            }
            else {
                cout << "ERROR: Unknown command '" << cmd << "'. Type 'help' for a list of commands.\n";
            }
        }
    }*/

    delete circuit;

    cout << "Exiting simulator. Goodbye!\n";
    return 0;
}
