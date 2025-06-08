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
            b[i] = J[i];
        }

        for (int i = 0; i < numVSources + numInductors; i++) {
            for (int j = 0; j < numNodes; j++) {
                A[numNodes + i][j] = C[i][j];
            }
            for (int j = 0; j < numVSources + numInductors; j++) {
                A[numNodes + i][numNodes + j] = D[i][j];
            }
            b[numNodes + i] = E[i];
        }

        try {
            vector<double> x = solveSystem(A, b);

            cout << "\nDC Analysis Results:\n";
            cout << "-------------------\n";
            cout << "Node Voltages:\n";
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
    double Is;
    double Vt;
    double n;
    double current;
    double lastVoltage;

public:
    Diode(const string& n, int n1, int n2)
        : Component(DIODE, n, n1, n2, 0.0),
          Is(1e-14), Vt(0.026), n(1.0), current(0.0), lastVoltage(0.0) {}

    void stamp(vector<vector<double>>& G,
               vector<vector<double>>&,
               vector<vector<double>>&,
               vector<vector<double>>&,
               vector<double>& J,
               vector<double>&,
               int&) override {
        double vd = lastVoltage;
        double g = (Is/(n*Vt)) * exp(vd/(n*Vt));
        double Ieq = Is*(exp(vd/(n*Vt)) - 1) - g*vd;

        if (node1 != 0) {
            G[node1-1][node1-1] += g;
            if (node2 != 0) {
                G[node1-1][node2-1] -= g;
                G[node2-1][node1-1] -= g;
            }
            J[node1-1] -= Ieq;
        }
        if (node2 != 0) {
            G[node2-1][node2-1] += g;
            J[node2-1] += Ieq;
        }
    }

    void update(double dt, const vector<double>& nodeVoltages) override {
        double v1 = (node1 == 0) ? 0 : nodeVoltages[node1-1];
        double v2 = (node2 == 0) ? 0 : nodeVoltages[node2-1];
        lastVoltage = v1 - v2;
        current = Is * (exp(lastVoltage/(n*Vt)) - 1);
    }

    double getCurrent(const vector<double>& nodeVoltages) const override {
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
    SDL_RenderDrawLine(renderer, margin, height - margin, width - margin, height - margin);
    SDL_RenderDrawLine(renderer, margin, height - margin, margin, margin);

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

void processCircuitFile(const string& filename, Circuit& circuit) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Error: Could not open file " << filename << endl;
        return;
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

            iss >> valStr;
            double value = parseSpiceValue(valStr);
            int n1 = getOrCreateNode(node1);
            int n2 = getOrCreateNode(node2);

            switch(typeChar) {
                case 'R':
                    circuit.addComponent(new Resistor(name, n1, n2, value));
                    break;
                case 'C':
                    circuit.addComponent(new Capacitor(name, n1, n2, value));
                    break;
                case 'L':
                    circuit.addComponent(new Inductor(name, n1, n2, value));
                    break;
                case 'V':
                    if (type.size() > 1 && toupper(type[1]) == 'S') {
                        double dc, amp, freq, phase = 0.0;
                        if (!(iss >> dc >> amp >> freq)) {
                            cerr << "Error: Invalid sinusoidal source parameters" << endl;
                            continue;
                        }
                        if (iss >> phase) {}
                        circuit.addComponent(new SinVoltageSource(name, n1, n2, amp, freq, phase, dc));
                    } else if (type.size() > 1 && toupper(type[1]) == 'P') {
                        double v1, v2, td, tr, tf, pw, per;
                        if (!(iss >> v1 >> v2 >> td >> tr >> tf >> pw >> per)) {
                            cerr << "Error: Invalid pulse source parameters" << endl;
                            continue;
                        }
                        circuit.addComponent(new PulseVoltageSource(name, n1, n2, v1, v2, td, tr, tf, pw, per));
                    } else {
                        circuit.addComponent(new VoltageSource(name, n1, n2, value));
                    }
                    break;
                case 'I':
                    circuit.addComponent(new CurrentSource(name, n1, n2, value));
                    break;
                case 'D':
                    circuit.addComponent(new Diode(name, n1, n2));
                    break;
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

void printTransientResults(const vector<double>& times, const vector<double>& voltages,
                          const vector<double>& currents, int numNodes) {
    cout << "\nTransient Analysis Results:\n";
    cout << "--------------------------\n";
    cout << "Time (s)\t";

    for (int i = 0; i < numNodes; i++) {
        cout << "Node " << getNodeName(i+1) << " (V)\t";
    }
    cout << "Currents (A)\n";

    size_t steps = min(times.size(), voltages.size() / numNodes);
    for (size_t step = 0; step < steps; step++) {
        cout << fixed << setprecision(6) << times[step] << "\t";

        for (int node = 0; node < numNodes; node++) {
            cout << voltages[step * numNodes + node] << "\t";
        }

        if (step < currents.size()) {
            cout << currents[step];
        }
        cout << "\n";
    }
}

void loadFromFile(Circuit& circuit, const string& filename) {
    ifstream infile(filename);
    if (!infile) {
        cerr << "Error: Cannot open file " << filename << endl;
        return;
    }

    string line;
    while (getline(infile, line)) {
        if (line.empty() || line[0] == '*') continue; // Skip empty lines and comments

        istringstream iss(line);
        string name, n1_str, n2_str, value_str;
        iss >> name >> n1_str >> n2_str >> value_str;

        int n1 = getOrCreateNode(n1_str);
        int n2 = getOrCreateNode(n2_str);
        double value = parseSpiceValue(value_str);

        char type = toupper(name[0]);
        switch (type) {
            case 'R':
                circuit.addComponent(new Resistor(name, n1, n2, value));
                break;
            case 'C':
                circuit.addComponent(new Capacitor(name, n1, n2, value));
                break;
            case 'L':
                circuit.addComponent(new Inductor(name, n1, n2, value));
                break;
            case 'V':
                // You need to define a VoltageSource class like Resistor
                // Placeholder:
                // circuit.addComponent(new VoltageSource(name, n1, n2, value));
                break;
            default:
                cerr << "Unknown component: " << name << endl;
                break;
        }
    }
}

int main(int argc, char* argv[]) {
    Circuit circuit;
    string command;

    if (argc > 1) {
        processCircuitFile(argv[1], circuit);
    }

    static int VCount = 0, RCount = 0, CCount = 0, LCount = 0, ICount = 0, DCount = 0;

    cout << "Circuit Simulator - OOP Project Phase 1\n";
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
    cout << "  delete R<name> - Delete resistor\n";
    cout << "  delete C<name> - Delete capacitor\n";
    cout << "  delete L<name> - Delete inductor\n";
    cout << "  delete D<name> - Delete diode\n";
    cout << "  delete GND <node> - Delete ground connection\n";
    cout << "  .nodes - List all nodes\n";
    cout << "  .list - List all components\n";
    cout << "  .list R - List resistors\n";
    cout << "  .list C - List capacitors\n";
    cout << "  .list L - List inductors\n";
    cout << "  .list D - List diodes\n";
    cout << "  .rename node <old_name> <new_name> - Rename a node\n";
    cout << "  load <filename> - Load circuit from file\n";
    cout << "  analyze - Run simulation\n";
    cout << "  analyze DC - Run DC analysis\n";
    cout << "  analyze TRAN <tstep> <tstop> - Run transient analysis\n";
    cout << "  exit - Quit program\n";

    while (true) {
        cout << ">>> ";
        getline(cin, command);
        if (command.empty()) continue;

        istringstream iss(command);
        string cmd;
        iss >> cmd;

        if (cmd == "load") {
            string filename;
            if (iss >> filename) {
               processCircuitFile( filename,circuit);
            } else {
                cout << "ERROR: Missing filename\n";
            }
        }
        else if (cmd == "analyze") {
            string analysisType;
            iss >> analysisType;

            if (analysisType.empty() || toupper(analysisType[0]) == 'D') {
                circuit.analyzeDC();
            }
            else if (toupper(analysisType[0]) == 'T') {
                double tStep, tStop;
                if (!(iss >> tStep >> tStop)) {
                    cout << "ERROR: Missing time parameters for transient analysis\n";
                    continue;
                }
                circuit.analyzeTransient(tStep, tStop);
                printTransientResults(Vtimes, voltages, currents, circuit.listNodes().size() - 1);
            }
            break;
        }
        else if (cmd == "exit") {
            return 0;
        }
        else if (cmd == ".nodes") {
            auto nodes = circuit.listNodes();
            cout << "Available nodes:\n";
            for (const auto& node : nodes) {
                cout << node << "\n";
            }
        }
        else if (cmd == ".list") {
            string filter;
            if (iss >> filter) {
                ComponentType filterType = static_cast<ComponentType>(-1);
                if (filter == "R") filterType = RESISTOR;
                else if (filter == "C") filterType = CAPACITOR;
                else if (filter == "L") filterType = INDUCTOR;
                else if (filter == "D") filterType = DIODE;

                auto components = circuit.listComponents(filterType);
                cout << "List of components:\n";
                for (const auto& comp : components) {
                    cout << comp << "\n";
                }
            } else {
                auto components = circuit.listComponents();
                cout << "List of all components:\n";
                for (const auto& comp : components) {
                    cout << comp << "\n";
                }
            }
        }
        else if (cmd == "add") {
            string typeName;
            iss >> typeName;

            if (typeName.empty()) {
                cout << "ERROR: Invalid syntax\n";
                continue;
            }

            char typeChar = toupper(typeName[0]);
            string name = typeName.substr(1);

            try {
                if (typeChar == 'G' && toupper(typeName[1]) == 'N' && toupper(typeName[2]) == 'D') {
                    string node;
                    if (!(iss >> node)) {
                        cout << "ERROR: Missing node for ground connection\n";
                        continue;
                    }
                    int n = getOrCreateNode(node);
                    circuit.addComponent(new Ground("GND", n));
                    cout << "SUCCESS: Ground connection added to node " << node << "\n";
                    continue;
                }

                string node1, node2, valStr;
                if (!(iss >> node1 >> node2 >> valStr)) {
                    cout << "ERROR: Missing parameters\n";
                    continue;
                }

                int n1 = getOrCreateNode(node1);
                int n2 = getOrCreateNode(node2);

                if (typeName.size() > 1 && toupper(typeName[1]) == 'S') {
                    double dc, amp, freq, phase = 0.0;
                    if (!(iss >> dc >> amp >> freq)) {
                        cout << "ERROR: Missing parameters for sinusoidal source\n";
                        continue;
                    }
                    if (iss >> phase) {}
                    circuit.addComponent(new SinVoltageSource(name, n1, n2, amp, freq, phase, dc));
                    cout << "SUCCESS: Sinusoidal voltage source " << name << " added between "
                         << node1 << " and " << node2 << "\n";
                    continue;
                } else if (typeName.size() > 1 && toupper(typeName[1]) == 'P') {
                    double v1, v2, td, tr, tf, pw, per;
                    if (!(iss >> v1 >> v2 >> td >> tr >> tf >> pw >> per)) {
                        cout << "ERROR: Missing parameters for pulse source\n";
                        continue;
                    }
                    circuit.addComponent(new PulseVoltageSource(name, n1, n2, v1, v2, td, tr, tf, pw, per));
                    cout << "SUCCESS: Pulse voltage source " << name << " added between "
                         << node1 << " and " << node2 << "\n";
                    continue;
                }

                double value = parseSpiceValue(valStr);

                if ((typeChar == 'R' || typeChar == 'C' || typeChar == 'L') && value <= 0) {
                    cout << "ERROR: " << (typeChar == 'R' ? "Resistance" :
                                         typeChar == 'C' ? "Capacitance" : "Inductance")
                         << " cannot be zero or negative\n";
                    continue;
                }

                switch (typeChar) {
                    case 'R':
                        circuit.addComponent(new Resistor(name, n1, n2, value));
                        cout << "SUCCESS: Resistor " << name << " added between "
                             << node1 << " and " << node2 << "\n";
                        break;
                    case 'C':
                        circuit.addComponent(new Capacitor(name, n1, n2, value));
                        cout << "SUCCESS: Capacitor " << name << " added between "
                             << node1 << " and " << node2 << "\n";
                        break;
                    case 'L':
                        circuit.addComponent(new Inductor(name, n1, n2, value));
                        cout << "SUCCESS: Inductor " << name << " added between "
                             << node1 << " and " << node2 << "\n";
                        break;
                    case 'V':
                        circuit.addComponent(new VoltageSource(name, n1, n2, value));
                        cout << "SUCCESS: Voltage source " << name << " added between "
                             << node1 << " and " << node2 << "\n";
                        break;
                    case 'I':
                        circuit.addComponent(new CurrentSource(name, n1, n2, value));
                        cout << "SUCCESS: Current source " << name << " added between "
                             << node1 << " and " << node2 << "\n";
                        break;
                    case 'D':
                        circuit.addComponent(new Diode(name, n1, n2));
                        cout << "SUCCESS: Diode " << name << " added between "
                             << node1 << " and " << node2 << "\n";
                        break;
                    default:
                        cout << "ERROR: Unknown component type '" << typeChar << "'\n";
                }
            } catch (const exception& e) {
                cout << "ERROR: Invalid input format\n";
            }
        }
        else if (cmd == "delete") {
            string typeName;
            if (!(iss >> typeName)) {
                cout << "ERROR: Missing component type\n";
                continue;
            }

            char typeChar = toupper(typeName[0]);
            string name = typeName.substr(1);

            if (typeChar == 'G' && toupper(typeName[1]) == 'N' && toupper(typeName[2]) == 'D') {
                string node;
                if (!(iss >> node)) {
                    cout << "ERROR: Missing node for ground deletion\n";
                    continue;
                }
                bool success = circuit.deleteComponent('G', "GND");
                if (success) {
                    cout << "SUCCESS: Ground connection removed\n";
                } else {
                    cout << "ERROR: Ground connection not found\n";
                }
                continue;
            }

            bool success = circuit.deleteComponent(typeChar, name);
            if (success) {
                cout << "SUCCESS: Component " << typeName << " deleted\n";
            } else {
                cout << "ERROR: Cannot delete " << typeName << "; component not found\n";
            }
        }
        else if (cmd == ".rename") {
            string subcmd, oldName, newName;
            if (!(iss >> subcmd >> oldName >> newName)) {
                cout << "ERROR: Invalid syntax - correct format: .rename node <old_name> <new_name>\n";
                continue;
            }

            if (subcmd == "node") {
                bool success = circuit.renameNode(oldName, newName);
                if (success) {
                    cout << "SUCCESS: Node renamed from " << oldName << " to " << newName << "\n";
                } else {
                    if (oldName == "GND" || oldName == "0" || newName == "GND" || newName == "0") {
                        cout << "ERROR: Cannot rename ground node\n";
                    } else if (nodeMap.find(newName) != nodeMap.end()) {
                        cout << "ERROR: Node name " << newName << " already exists\n";
                    } else {
                        cout << "ERROR: Node " << oldName << " does not exist\n";
                    }
                }
            } else {
                cout << "ERROR: Invalid syntax - correct format: .rename node <old_name> <new_name>\n";
            }
        }
        else {
            cout << "ERROR: Unknown command '" << cmd << "'\n";
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