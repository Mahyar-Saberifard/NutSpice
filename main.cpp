#include <bits/stdc++.h>
#include <SDL2/SDL.h>

using namespace std;

vector<double> voltages;
vector<double> times;
bool hasDynamic = false;

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

        if (comp->type == CAPACITOR || comp->type == INDUCTOR) {
            hasDynamic = true;
        }
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

        voltages.clear();
        times.clear();

        if (maxNode < 1) {
            cerr << "Error: Circuit must have at least one non-ground node" << endl;
            return;
        }

        int numNodes = maxNode;
        int numVSources = 0;
        int numInductors = 0;

        for (auto comp : components) {
            if (comp->type == VOLTAGE_SOURCE) numVSources++;
            if (comp->type == INDUCTOR) numInductors++;
        }

        int numVars = numNodes + numVSources + numInductors;

        for (auto comp : components) {
            if (comp->type == INDUCTOR) {
                //dynamic_cast<Inductor*>(comp)->current = 0.0; // Start with 0 current
            }
            if (comp->type == CAPACITOR) {
                //dynamic_cast<Capacitor*>(comp)->prevVoltage = 0.0; // Start with 0 voltage
            }
        }

        while (t <= tStop) {
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
                times.push_back(t);
                voltages.push_back(x[i]);
            }

            t += tStep;
        }
    }

    static double getTimeStep() { return currentTimeStep; }
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
        if (value == 0) return 0.0;
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
public:
    Capacitor(const string& n, int n1, int n2, double val) : Component(CAPACITOR, n, n1, n2, val), prevVoltage(0.0) {}

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
    double current;
    double prevVoltage;
public:
    Inductor(const string& n, int n1, int n2, double val)
        : Component(INDUCTOR, n, n1, n2, val), current(0.0), prevVoltage(0.0) {}

    void stamp(vector<vector<double>>& G,
              vector<vector<double>>& B,
              vector<vector<double>>& C,
              vector<vector<double>>& D,
              vector<double>& J,
              vector<double>& E,
              int& nextVariable) override {
        double dt = Circuit::getTimeStep();

        double geq = dt / (2.0 * value);
        double ieq = current + (dt / (2.0 * value)) * prevVoltage;

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

        if (node1 != 0) J[node1-1] -= ieq;
        if (node2 != 0) J[node2-1] += ieq;
    }

    void update(double dt, const vector<double>& nodeVoltages) override {
        double v1 = (node1 == 0) ? 0 : nodeVoltages[node1-1];
        double v2 = (node2 == 0) ? 0 : nodeVoltages[node2-1];
        double voltage = v1 - v2;

        current += (dt / (2.0 * value)) * (voltage + prevVoltage);
        prevVoltage = voltage;
    }

    double getCurrent(const vector<double>& nodeVoltages) const override {
        return current;
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
                         const vector<double>& voltages,
                         const vector<double>& times,
                         int width, int height,
                         int margin = 50) {
    if (voltages.empty() || times.empty() || voltages.size() != times.size()) {
        SDL_Log("Plotting error: No valid data to display");
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        return;
    }

    double minVoltage = *min_element(voltages.begin(), voltages.end());
    double maxVoltage = *max_element(voltages.begin(), voltages.end());
    double minTime = *min_element(times.begin(), times.end());
    double maxTime = *max_element(times.begin(), times.end());

    double voltageRange = maxVoltage - minVoltage;
    double timeRange = maxTime - minTime;
    maxVoltage += voltageRange * 0.1;
    minVoltage -= voltageRange * 0.1;
    maxTime += timeRange * 0.1;
    minTime -= timeRange * 0.1;

    double scaleX = (width - 2 * margin) / (maxTime - minTime);
    double scaleY = (height - 2 * margin) / (maxVoltage - minVoltage);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawLine(renderer, margin, height - margin, width - margin, height - margin);
    SDL_RenderDrawLine(renderer, margin, height - margin, margin, margin);

    for (double t = minTime; t <= maxTime; t += (maxTime - minTime) / 5) {
        int x = margin + static_cast<int>((t - minTime) * scaleX);
        SDL_RenderDrawLine(renderer, x, height - margin - 5, x, height - margin + 5);
    }

    for (double v = minVoltage; v <= maxVoltage; v += (maxVoltage - minVoltage) / 5) {
        int y = height - margin - static_cast<int>((v - minVoltage) * scaleY);
        SDL_RenderDrawLine(renderer, margin - 5, y, margin + 5, y);
    }

    SDL_SetRenderDrawColor(renderer, 0, 255, 100, 255);
    for (size_t i = 0; i < voltages.size(); ++i) {
        int x = margin + static_cast<int>((times[i] - minTime) * scaleX);
        int y = height - margin - static_cast<int>((voltages[i] - minVoltage) * scaleY);

        SDL_RenderDrawLine(renderer, x-2, y, x+2, y);
        SDL_RenderDrawLine(renderer, x, y-2, x, y+2);
    }

    SDL_RenderPresent(renderer);
}

int main(int argc, char* argv[]) {
    Circuit circuit;
    string type;
    int n1, n2, VCount = 0, RCount = 0, CCount = 0, LCount = 0, ICount = 0;
    double val;

    cout << "Circuit Simulator\n";
    cout << "Format: Type(V,R,C,L,I) node1 node2 value\n";
    cout << "Type 'analyze' to run simulation.\n";

    while (true) {
        cin >> type;

        if (type == "analyze") {
            break;
        } else if (type == "V" || type == "R" || type == "C" || type == "L" || type == "I") {
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
            }
        } else {
            cout << "Unknown component type: " << type << "\n";
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }
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

        plotVoltageTimeGraph(renderer, voltages, times, 800, 600);

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