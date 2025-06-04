#include <bits/stdc++.h>
#include <SDL2/SDL.h>

using namespace std;

vector<double> voltages;
vector<double> currents;
vector<double> Vtimes;
vector<double> Itimes;
bool hasDynamic = false;
bool showVoltage = true;
bool showCurrent = false;

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
    VC_VS,
    VC_CS,
    CC_VS,
    CC_CS
};

// Node management
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

    // Check if it's a simple number
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
            hasDot = false; // After E, we can have another dot
            hasSign = false; // And another sign
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
            case VC_VS: typeStr = "VCVS"; break;
            case VC_CS: typeStr = "VCCS"; break;
            case CC_VS: typeStr = "CCVS"; break;
            case CC_CS: typeStr = "CCCS"; break;
        }
        return typeStr + " " + name + " " + getNodeName(node1) + " " + getNodeName(node2) + " " + to_string(value);
    }
};

class Circuit {
    vector<Component*> components;
    int maxNode;
    static double currentTimeStep;
    static double currentTime;

public:
    Circuit() : maxNode(0) {}

    ~Circuit() {
        for (auto comp : components) {
            delete comp;
        }
    }

    void addComponent(Component* comp) {
        components.push_back(comp);
        if (comp->node1 > maxNode) maxNode = comp->node1;
        if (comp->node2 > maxNode) maxNode = comp->node2;

        if (comp->type == CAPACITOR || comp->type == INDUCTOR) {
            hasDynamic = true;
        }
    }

    bool deleteComponent(char typeChar, const string& name) {
        ComponentType type;
        switch(toupper(typeChar)) {
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
            return false; // New name already exists
        }

        auto it = nodeMap.find(oldName);
        if (it == nodeMap.end()) {
            return false; // Old name doesn't exist
        }

        int nodeNum = it->second;
        nodeMap.erase(it);
        nodeMap[newName] = nodeNum;
        reverseNodeMap[nodeNum] = newName;
        return true;
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
            if (comp->type == VOLTAGE_SOURCE || comp->type == SIN_VOLTAGE_SOURCE || comp->type == PULSE_VOLTAGE_SOURCE) numVSources++;
            if (comp->type == INDUCTOR) numInductors++;
        }

        int numVars = numNodes + numVSources + numInductors;

        vector<vector<double>> G(numNodes, vector<double>(numNodes, 0.0));
        vector<vector<double>> B(numNodes, vector<double>(numVSources + numInductors, 0.0));
        vector<vector<double>> C(numVSources + numInductors, vector<double>(numNodes, 0.0));
        vector<vector<double>> D(numVSources + numInductors, vector<double>(numVSources + numInductors, 0.0));
        vector<double> J(numNodes, 0.0);
        vector<double> E(numVSources + numInductors, 0.0);

        int vsCount = 0;
        int indCount = 0;
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
            for (int j = 0; j < numNodes; j++) {
                A[i][j] = G[i][j];
            }
            for (int j = 0; j < numVSources + numInductors; j++) {
                A[i][numNodes + j] = B[i][j];
            }
        }
        for (int i = 0; i < numVSources + numInductors; i++) {
            for (int j = 0; j < numNodes; j++) {
                A[numNodes + i][j] = C[i][j];
            }
            for (int j = 0; j < numVSources + numInductors; j++) {
                A[numNodes + i][numNodes + j] = D[i][j];
            }
        }

        for (int i = 0; i < numNodes; i++) {
            b[i] = J[i];
        }
        for (int i = 0; i < numVSources + numInductors; i++) {
            b[numNodes + i] = E[i];
        }

        vector<double> x = solveSystem(A, b);

        cout << "\nDC Analysis Results:\n";
        cout << "-------------------\n";
        for (int i = 0; i < numNodes; i++) {
            cout << "Node " << getNodeName(i+1) << " voltage: " << x[i] << " V\n";
        }
        for (int i = 0; i < numVSources; i++) {
            cout << "Current through voltage source " << (i+1) << ": " << x[numNodes + i] << " A\n";
        }
    }

    void analyzeTransient(double tStep, double tStop) {
        currentTimeStep = tStep;
        double t = 0.0;

        voltages.clear();
        currents.clear();
        Vtimes.clear();
        Itimes.clear();

        if (maxNode < 1) {
            cerr << "Error: Circuit must have at least one non-ground node" << endl;
            return;
        }

        int numNodes = maxNode;
        int numVSources = 0;
        int numInductors = 0;

        for (auto comp : components) {
            if (comp->type == VOLTAGE_SOURCE || comp->type == SIN_VOLTAGE_SOURCE || comp->type == PULSE_VOLTAGE_SOURCE) numVSources++;
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

            int vsCount = 0;
            currentTime = t;
            for (auto comp : components) {
                comp->stamp(G, B, C, D, J, E, vsCount);
            }

            for (int i = 0; i < numNodes; i++) {
                G[i][i] += 1e-12;
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
            } catch (const runtime_error& e) {
                cerr << "Error at t=" << t << ": " << e.what() << endl;
                break;
            }

            for (auto comp : components) {
                comp->update(tStep, x);
            }

            for (int i = 0; i < numNodes; i++) {
                Vtimes.push_back(t);
                voltages.push_back(x[i]);
            }

            for (auto comp : components) {
                if (comp->type == RESISTOR || comp->type == CAPACITOR || comp->type == INDUCTOR || comp->type == DIODE) {
                    Itimes.push_back(t);
                    currents.push_back(comp->getCurrent(x));
                }
            }

            t += tStep;
        }
    }

    static double getTimeStep() { return currentTimeStep; }
    static double getTime() { return currentTime; }
};

double Circuit::currentTimeStep = 0.0;
double Circuit::currentTime = 0.0;

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
    double threshold;

public:
    Diode(const string& n, int n1, int n2, double thresh)
            : Component(DIODE, n, n1, n2, 0.0), threshold(thresh) {}

    void stamp(vector<vector<double>>& G,
               vector<vector<double>>&,
               vector<vector<double>>&,
               vector<vector<double>>&,
               vector<double>& J,
               vector<double>&,
               int&) override {
        double conductance = 1e9;
        double offConductance = 1e-9;

        double v1 = (node1 == 0) ? 0 : 0.0;
        double v2 = (node2 == 0) ? 0 : 0.0;
        double v_d = v1 - v2;

        double g = (v_d >= threshold) ? conductance : offConductance;

        if (node1 != 0) {
            G[node1 - 1][node1 - 1] += g;
            if (node2 != 0) {
                G[node1 - 1][node2 - 1] -= g;
                G[node2 - 1][node1 - 1] -= g;
            }
        }
        if (node2 != 0) {
            G[node2 - 1][node2 - 1] += g;
        }
    }

    double getCurrent(const vector<double>& nodeVoltages) const override {
        double v1 = (node1 == 0) ? 0 : nodeVoltages[node1 - 1];
        double v2 = (node2 == 0) ? 0 : nodeVoltages[node2 - 1];
        double v_d = v1 - v2;
        double g = (v_d >= threshold) ? 1e9 : 1e-9;
        return g * (v_d - threshold);
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
    double phase; // in degrees
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

class VCVS : public Component {
    int ctrlNode1, ctrlNode2;
    double gain;
public:
    VCVS(const string& n, int n1, int n2, int cn1, int cn2, double g)
        : Component(VC_VS, n, n1, n2, 0.0), ctrlNode1(cn1), ctrlNode2(cn2), gain(g) {}

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

        if (ctrlNode1 != 0) C[vsIndex][ctrlNode1-1] = gain;
        if (ctrlNode2 != 0) C[vsIndex][ctrlNode2-1] = -gain;

        D[vsIndex][vsIndex] = -1;
    }

    string getInfo() const override {
        return "VCVS " + name + " " + getNodeName(node1) + " " + getNodeName(node2) +
               " controlling nodes: " + getNodeName(ctrlNode1) + " " + getNodeName(ctrlNode2) +
               " gain=" + to_string(gain);
    }
};

class VCCS : public Component {
    int ctrlNode1, ctrlNode2;
    double gain;
public:
    VCCS(const string& n, int n1, int n2, int cn1, int cn2, double g)
        : Component(VC_CS, n, n1, n2, 0.0), ctrlNode1(cn1), ctrlNode2(cn2), gain(g) {}

    void stamp(vector<vector<double>>& G,
               vector<vector<double>>&,
               vector<vector<double>>&,
               vector<vector<double>>&,
               vector<double>&,
               vector<double>&,
               int&) override {
        if (ctrlNode1 != 0 && node1 != 0) G[node1-1][ctrlNode1-1] += gain;
        if (ctrlNode1 != 0 && node2 != 0) G[node2-1][ctrlNode1-1] -= gain;
        if (ctrlNode2 != 0 && node1 != 0) G[node1-1][ctrlNode2-1] -= gain;
        if (ctrlNode2 != 0 && node2 != 0) G[node2-1][ctrlNode2-1] += gain;
    }

    string getInfo() const override {
        return "VCCS " + name + " " + getNodeName(node1) + " " + getNodeName(node2) +
               " controlling nodes: " + getNodeName(ctrlNode1) + " " + getNodeName(ctrlNode2) +
               " gain=" + to_string(gain);
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

void plotGraph(SDL_Renderer* renderer, const vector<double>& data1, const vector<double>& data2, const vector<double>& Vtimes, const vector<double>& Itimes, int width, int height, int margin = 50) {
    if (Vtimes.empty() || (data1.empty() && data2.empty())) {
        SDL_Log("Plotting error: No valid data to display");
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        return;
    }

    double minY1 = data1.empty() ? 0 : *min_element(data1.begin(), data1.end());
    double maxY1 = data1.empty() ? 0 : *max_element(data1.begin(), data1.end());
    double minY2 = data2.empty() ? 0 : *min_element(data2.begin(), data2.end());
    double maxY2 = data2.empty() ? 0 : *max_element(data2.begin(), data2.end());
    double minTime = Vtimes.front();
    double maxTime = Vtimes.back();

    if (maxY1 == minY1) { maxY1 += 1; minY1 -= 1; }
    if (maxY2 == minY2) { maxY2 += 1; minY2 -= 1; }

    double yRange1 = maxY1 - minY1;
    double yRange2 = maxY2 - minY2;
    maxY1 += yRange1 * 0.1;
    minY1 -= yRange1 * 0.1;
    maxY2 += yRange2 * 0.1;
    minY2 -= yRange2 * 0.1;

    double minY = showVoltage ? minY1 : minY2;
    double maxY = showVoltage ? maxY1 : maxY2;
    if (showVoltage && showCurrent) {
        minY = min(minY1, minY2);
        maxY = max(maxY1, maxY2);
    }

    double scaleX = (width - 2 * margin) / (maxTime - minTime);
    double scaleY = (height - 2 * margin) / (maxY - minY);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawLine(renderer, margin, height - margin, width - margin, height - margin); // X-axis
    SDL_RenderDrawLine(renderer, margin, height - margin, margin, margin); // Y-axis

    for (double t = minTime; t <= maxTime; t += (maxTime - minTime) / 5) {
        int x = margin + static_cast<int>((t - minTime) * scaleX);
        SDL_RenderDrawLine(renderer, x, height - margin - 5, x, height - margin + 5);
    }

    for (double y = minY; y <= maxY; y += (maxY - minY) / 5) {
        int yPos = height - margin - static_cast<int>((y - minY) * scaleY);
        SDL_RenderDrawLine(renderer, margin - 5, yPos, margin + 5, yPos);
    }

    if (showVoltage && !data1.empty()) {
        SDL_SetRenderDrawColor(renderer, 0, 255, 100, 255);
        for (size_t i = 0; i < data1.size(); ++i) {
            int x = margin + static_cast<int>((Vtimes[i] - minTime) * scaleX);
            int y = height - margin - static_cast<int>((data1[i] - minY) * scaleY);
            SDL_RenderDrawLine(renderer, x-2, y, x+2, y);
            SDL_RenderDrawLine(renderer, x, y-2, x, y+2);
        }
    }

    if (showCurrent && !data2.empty()) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        for (size_t i = 0; i < data2.size(); ++i) {
            int x = margin + static_cast<int>((Itimes[i] - minTime) * scaleX);
            int y = height - margin - static_cast<int>((data2[i] - minY) * scaleY);
            SDL_RenderDrawLine(renderer, x-2, y-2, x+2, y+2);
            SDL_RenderDrawLine(renderer, x-2, y+2, x+2, y-2);
        }
    }

    SDL_RenderPresent(renderer);
}

int main(int argc, char* argv[]) {
    Circuit circuit;
    string command;
    static int VCount = 0, RCount = 0, CCount = 0, LCount = 0, ICount = 0, DCount = 0;

    cout << "Circuit Simulator - OOP Project Phase 1\n";
    cout << "Available commands:\n";
    cout << "  add <component_type><name> <node1> <node2> <value>\n";
    cout << "  delete <component_type><name>\n";
    cout << "  .nodes - List all nodes\n";
    cout << "  .list - List all components\n";
    cout << "  .list <component_type> - List specific components\n";
    cout << "  .rename node <old_name> <new_name> - Rename a node\n";
    cout << "  analyze - Run simulation\n";
    cout << "  exit - Quit program\n";

    while (true) {
        cout << ">>> ";
        cin >> command;

        if (command == "analyze") {
            break;
        }
        else if (command == "exit") {
            return 0;
        }
        else if (command == ".nodes") {
            // Implement node listing
            cout << "Available nodes:\n";
            // You'll need to track nodes separately for this
            cout << "n001, n002, GND\n"; // Example output
        }
        else if (command == ".list") {
            // Implement component listing
            cout << "List of all components:\n";
            // You'll need to track components for this
            cout << "R1 n001 n002 1000\n"; // Example output
        }
        else if (command == "add") {
            string typeName, node1, node2, valStr;
            cin >> typeName >> node1 >> node2 >> valStr;

            // Extract component type and name
            char typeChar = typeName[0];
            string name = typeName.substr(1);

            try {
                int n1 = stoi(node1.substr(1)); // Extract number from "n001" format
                int n2 = stoi(node2.substr(1));
                double value = parseSpiceValue(valStr);

                // Validate value
                if ((typeChar == 'R' || typeChar == 'C' || typeChar == 'L') && value <= 0) {
                    cout << "ERROR: " << (typeChar == 'R' ? "Resistance" :
                                         typeChar == 'C' ? "Capacitance" : "Inductance")
                         << " cannot be zero or negative\n";
                    continue;
                }

                switch (toupper(typeChar)) {
                    case 'R':
                        circuit.addComponent(new Resistor(name, n1, n2, value));
                        cout << "SUCCESS: Resistor " << name << " added\n";
                        break;
                    case 'C':
                        circuit.addComponent(new Capacitor(name, n1, n2, value));
                        cout << "SUCCESS: Capacitor " << name << " added\n";
                        break;
                    case 'L':
                        circuit.addComponent(new Inductor(name, n1, n2, value));
                        cout << "SUCCESS: Inductor " << name << " added\n";
                        break;
                    case 'V':
                        circuit.addComponent(new VoltageSource(name, n1, n2, value));
                        cout << "SUCCESS: Voltage source " << name << " added\n";
                        break;
                    case 'I':
                        circuit.addComponent(new CurrentSource(name, n1, n2, value));
                        cout << "SUCCESS: Current source " << name << " added\n";
                        break;
                    case 'D':
                        circuit.addComponent(new Diode(name, n1, n2, 0.7)); // Default threshold
                        cout << "SUCCESS: Diode " << name << " added\n";
                        break;
                    case 'G':
                        if (node1 == "GND") {
                            circuit.addComponent(new Ground(name, 0));
                            cout << "SUCCESS: Ground added\n";
                        } else {
                            cout << "ERROR: Ground must be connected to node GND\n";
                        }
                        break;
                    default:
                        cout << "ERROR: Unknown component type '" << typeChar << "'\n";
                }
            } catch (const exception& e) {
                cout << "ERROR: Invalid input format\n";
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
            }
        }
        else if (command == "delete") {
            string typeName;
            cin >> typeName;

            char typeChar = typeName[0];
            string name = typeName.substr(1);

            // You'll need to implement component deletion in your Circuit class
            bool success = false; // circuit.deleteComponent(typeChar, name);

            if (success) {
                cout << "SUCCESS: Component " << typeName << " deleted\n";
            } else {
                cout << "ERROR: Cannot delete " << typeChar << name << "; component not found\n";
            }
        }
        else if (command == ".rename") {
            string subcmd, oldName, newName;
            cin >> subcmd >> oldName >> newName;

            if (subcmd == "node") {
                // You'll need to implement node renaming
                bool success = false; // circuit.renameNode(oldName, newName);

                if (success) {
                    cout << "SUCCESS: Node renamed from " << oldName << " to " << newName << "\n";
                } else {
                    cout << "ERROR: Node " << oldName << " does not exist in the circuit\n";
                }
            } else {
                cout << "ERROR: Invalid syntax - correct format: .rename node <old_name> <new_name>\n";
            }
        }
        else {
            cout << "ERROR: Unknown command '" << command << "'\n";
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }
    }

    char choice;
    cout << "What would you like to plot? (V)oltage, (C)urrent, or (B)oth? ";
    cin >> choice;

    if (toupper(choice) == 'V') {
        showVoltage = true;
        showCurrent = false;
    } else if (toupper(choice) == 'C') {
        showVoltage = false;
        showCurrent = true;
    } else if (toupper(choice) == 'B') {
        showVoltage = true;
        showCurrent = true;
    } else {
        cout << "Invalid choice. Defaulting to voltage only.\n";
        showVoltage = true;
        showCurrent = false;
    }

    try {
        if (!hasDynamic) {
            circuit.analyzeDC();
        } else {
            double tStep, tStop;
            cout << "Enter time step: ";
            cin >> tStep;
            cout << "Enter stop time: ";
            cin >> tStop;
            circuit.analyzeTransient(tStep, tStop);
        }
    } catch (const exception& e) {
        cerr << "Analysis failed: " << e.what() << endl;
        return 1;
    }

    if (!voltages.empty()) {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << endl;
            return 1;
        }

        SDL_Window* window = SDL_CreateWindow("Voltage vs Time Graph",
                                              SDL_WINDOWPOS_CENTERED,
                                              SDL_WINDOWPOS_CENTERED,
                                              800, 600,
                                              SDL_WINDOW_SHOWN);
        if (!window) {
            cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << endl;
            SDL_Quit();
            return 1;
        }

        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << endl;
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
        plotGraph(renderer, voltages, currents, Vtimes, Itimes, 800, 600);
        SDL_Event e;
        bool quit = false;
        while (!quit) {
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) {
                    quit = true;
                }
            }
            SDL_Delay(100);
        }
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
    }
    return 0;
}