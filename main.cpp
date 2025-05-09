#include <bits/stdc++.h>
#include <SDL2/SDL.h>

using namespace std;


vector<double> voltages;
vector<double> times;


enum ComponentType {
    RESISTOR,
    CAPACITOR,
    INDUCTOR,
    VOLTAGE_SOURCE,
    CURRENT_SOURCE,
    DIODE,
    GROUND
};

class Component {
public:
    ComponentType type;
    string name;
    int node1, node2;
    double value;

    Component(ComponentType t, const string& n, int n1, int n2, double val) : type(t), name(n), node1(n1), node2(n2), value(val) {}

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
};

class Circuit {
    vector<Component*> components;
    int maxNode;
    static double currentTimeStep;

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
            if (comp->type == VOLTAGE_SOURCE) numVSources++;
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
            cout << "Node " << (i+1) << " voltage: " << x[i] << " V\n";
        }
        for (int i = 0; i < numVSources; i++) {
            cout << "Current through voltage source " << (i+1) << ": " << x[numNodes + i] << " A\n";
        }
    }

    void analyzeTransient(double tStep, double tStop) {
        currentTimeStep = tStep;
        double t = 0.0;
        int stepCount = 0;

        int numNodes = maxNode;
        int numVSources = 0;
        int numInductors = 0;

        for (auto comp : components) {
            if (comp->type == VOLTAGE_SOURCE) numVSources++;
            if (comp->type == INDUCTOR) numInductors++;
        }

        int numVars = numNodes + numVSources + numInductors;

        cout << "\nStarting transient analysis from t=0 to t=" << tStop << " with step=" << tStep << "\n";

        while (t < tStop) {
            vector<vector<double>> G(numNodes, vector<double>(numNodes, 0.0));
            vector<vector<double>> B(numNodes, vector<double>(numVSources + numInductors, 0.0));
            vector<vector<double>> C(numVSources + numInductors, vector<double>(numNodes, 0.0));
            vector<vector<double>> D(numVSources + numInductors, vector<double>(numVSources + numInductors, 0.0));
            vector<double> J(numNodes, 0.0);
            vector<double> E(numVSources + numInductors, 0.0);

            int vsCount = 0;
            int indCount = 0;
            for (auto comp : components) {
                comp->stamp(G, B, C, D, J, E, vsCount);
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

            for (auto comp : components) {
                comp->update(tStep, x);
            }

            if (stepCount % 10 == 0) {
                cout << "\nTime = " << t << " seconds:\n";
                for (int i = 0; i < numNodes; i++) {
                    times.push_back(t);
                    voltages.push_back(x[i]);
                    cout << "  Node " << (i+1) << " voltage: " << x[i] << " V\n";
                }
                for (auto comp : components) {
                    if (comp->type == CAPACITOR || comp->type == INDUCTOR) {
                        cout << "  Current through " << comp->name << ": " << comp->getCurrent(x) << " A\n";
                    }
                }
            }

            t += tStep;
            stepCount++;
        }
    }

    static double getTimeStep() { return currentTimeStep; }

    vector<Component*> getComponents() { return components; }

};

class Resistor : public Component {
public:
    Resistor(const string& n, int n1, int n2, double val) : Component(RESISTOR, n, n1, n2, val) {}

    void stamp(vector<vector<double>>& G,
              vector<vector<double>>& B,
              vector<vector<double>>& C,
              vector<vector<double>>& D,
              vector<double>& J,
              vector<double>& E,
              int& nextVariable) override {
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
        double v1 = (node1 == 0) ? 0 : nodeVoltages[node1-1];
        double v2 = (node2 == 0) ? 0 : nodeVoltages[node2-1];
        return (v1 - v2) / value;
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
};

class Capacitor : public Component {
    double prevVoltage;
    double companionCurrent;
public:
    Capacitor(const string& n, int n1, int n2, double val) : Component(CAPACITOR, n, n1, n2, val), prevVoltage(0.0), companionCurrent(0.0) {}

    void stamp(vector<vector<double>>& G,
              vector<vector<double>>& B,
              vector<vector<double>>& C,
              vector<vector<double>>& D,
              vector<double>& J,
              vector<double>& E,
              int& nextVariable) override {
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
        prevVoltage = v1 - v2;
    }

    double getCurrent(const vector<double>& nodeVoltages) const override {
        double v1 = (node1 == 0) ? 0 : nodeVoltages[node1-1];
        double v2 = (node2 == 0) ? 0 : nodeVoltages[node2-1];
        double current = value * ((v1 - v2) - prevVoltage) / Circuit::getTimeStep();
        return current;
    }
};

class Inductor : public Component {
    double prevCurrent;
public:
    Inductor(const string& n, int n1, int n2, double val) : Component(INDUCTOR, n, n1, n2, val), prevCurrent(0.0) {}

    void stamp(vector<vector<double>>& G,
              vector<vector<double>>& B,
              vector<vector<double>>& C,
              vector<vector<double>>& D,
              vector<double>& J,
              vector<double>& E,
              int& nextVariable) override {

        int inductorVarIndex = nextVariable++;

        if (node1 != 0) B[node1-1][inductorVarIndex] = 1;
        if (node2 != 0) B[node2-1][inductorVarIndex] = -1;

        if (node1 != 0) C[inductorVarIndex][node1-1] = 1;
        if (node2 != 0) C[inductorVarIndex][node2-1] = -1;

        D[inductorVarIndex][inductorVarIndex] = -value / Circuit::getTimeStep();

        E[inductorVarIndex] = -prevCurrent;
    }

    void update(double dt, const vector<double>& nodeVoltages) override {
        prevCurrent = nodeVoltages.back();
    }

    double getCurrent(const vector<double>& nodeVoltages) const override {
        return nodeVoltages.back();
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

double Circuit::currentTimeStep = 0.0;


void plotVoltageTimeGraph(SDL_Renderer* renderer,
                         const std::vector<double>& voltages,
                         const std::vector<double>& times,
                         int width, int height,
                         int margin = 50) {
    if (voltages.empty() || times.empty() || voltages.size() != times.size()) {
        SDL_Log("Error: Invalid input data for plotting");
        return;
    }

    float minVoltage = voltages[0], maxVoltage = voltages[0];
    float minTime = times[0], maxTime = times[0];

    for (size_t i = 1; i < voltages.size(); ++i) {
        if (voltages[i] < minVoltage) minVoltage = voltages[i];
        if (voltages[i] > maxVoltage) maxVoltage = voltages[i];
        if (times[i] < minTime) minTime = times[i];
        if (times[i] > maxTime) maxTime = times[i];
    }

    float voltageRange = maxVoltage - minVoltage;
    float timeRange = maxTime - minTime;
    maxVoltage += voltageRange * 0.1f;
    minVoltage -= voltageRange * 0.1f;
    maxTime += timeRange * 0.1f;
    minTime -= timeRange * 0.1f;

    float scaleX = (width - 2 * margin) / (maxTime - minTime);
    float scaleY = (height - 2 * margin) / (maxVoltage - minVoltage);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawLine(renderer, margin, height - margin, width - margin, height - margin);
    SDL_RenderDrawLine(renderer, margin, height - margin, margin, margin);

    for (float t = minTime; t <= maxTime; t += (maxTime - minTime) / 5) {
        int x = margin + static_cast<int>((t - minTime) * scaleX);
        SDL_RenderDrawLine(renderer, x, height - margin - 5, x, height - margin + 5);
    }

    for (float v = minVoltage; v <= maxVoltage; v += (maxVoltage - minVoltage) / 5) {
        int y = height - margin - static_cast<int>((v - minVoltage) * scaleY);
        SDL_RenderDrawLine(renderer, margin - 5, y, margin + 5, y);
    }

    SDL_SetRenderDrawColor(renderer, 0, 255, 100, 255);
    for (size_t i = 0; i < voltages.size(); ++i) {
        int x = margin + static_cast<int>((times[i] - minTime) * scaleX);
        int y = height - margin - static_cast<int>((voltages[i] - minVoltage) * scaleY);
        SDL_RenderDrawPoint(renderer, x, y);

        SDL_RenderDrawPoint(renderer, x+1, y);
        SDL_RenderDrawPoint(renderer, x-1, y);
        SDL_RenderDrawPoint(renderer, x, y+1);
        SDL_RenderDrawPoint(renderer, x, y-1);
    }

    SDL_RenderPresent(renderer);
}


int SDL_main(int argc, char* argv[]) {
    Circuit circuit;
    string type;
    int n1, n2, VCount = 0, RCount = 0, CCount = 0, LCount = 0, ICount = 0, DCount = 0;
    double val;

    cout << "Format: Type(V,R,C,L,I,D) node1 node2 value\nType 'analyze' to run.\n";

    while (true) {
        cout << "> ";
        cin >> type;

        if (type == "analyze") {
            break;
        } else {
            cin >> n1 >> n2 >> val;
            if (type == "V") {
                VCount++;
                circuit.addComponent(new VoltageSource("V" + to_string(VCount), n1, n2, val));
            } else if (type == "R") {
                RCount++;
                circuit.addComponent(new Resistor("R" + to_string(RCount), n1, n2, val));
            } else if (type == "C") {
                CCount++;
                circuit.addComponent(new Capacitor("C" + to_string(CCount), n1, n2, val));
            } else if (type == "L") {
                LCount++;
                circuit.addComponent(new Inductor("L" + to_string(LCount), n1, n2, val));
            } else if (type == "I") {
                ICount++;
                circuit.addComponent(new CurrentSource("I" + to_string(ICount), n1, n2, val));
            } else {
                cout << "Unknown component type: " << type << "\n";
            }
        }
    }

    bool hasDynamic = false;
    for (auto comp : circuit.getComponents()) {
        if (comp->type == CAPACITOR || comp->type == INDUCTOR) {
            hasDynamic = true;
            break;
        }
    }

    if (!hasDynamic) {
        circuit.analyzeDC();
    }else {
        double tStep, tStop;
        cout << "Enter time step: ";
        cin >> tStep;
        cout << "Enter stop time: ";
        cin >> tStop;
        circuit.analyzeTransient(tStep, tStop);
    }


    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("Voltage vs Time Graph",
                                         SDL_WINDOWPOS_CENTERED,
                                         SDL_WINDOWPOS_CENTERED,
                                         800, 600,
                                         SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if (hasDynamic) {
        plotVoltageTimeGraph(renderer, voltages, times, 800, 600);

        int k;
        cin >> k;
        if (k) {
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
        }
    }

    return 0;
}