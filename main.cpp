#include <bits/stdc++.h>

using namespace std;

enum ComponentType {
    RESISTOR,
    CAPACITOR,
    INDUCTOR,
    VOLTAGE_SOURCE,
    CURRENT_SOURCE,
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

    virtual void stamp(vector<vector<double>>& G, vector<vector<double>>& B, vector<vector<double>>& C, vector<vector<double>>& D, vector<double>& J, vector<double>& E, int& nextVariable) = 0;

    virtual void update(double dt) {}
};

class Resistor : public Component {
public:
    Resistor(const string& n, int n1, int n2, double val) : Component(RESISTOR, n, n1, n2, val) {}

    void stamp(vector<vector<double>>& G, vector<vector<double>>& B, vector<vector<double>>& C, vector<vector<double>>& D, vector<double>& J, vector<double>& E, int& nextVariable) override {
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
};

class VoltageSource : public Component {
public:
    VoltageSource(const string& n, int n1, int n2, double val) : Component(VOLTAGE_SOURCE, n, n1, n2, val) {}

    void stamp(vector<vector<double>>& G, vector<vector<double>>& B, vector<vector<double>>& C, vector<vector<double>>& D, vector<double>& J, vector<double>& E, int& nextVariable) override {
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

    void stamp(vector<vector<double>>& G, vector<vector<double>>& B, vector<vector<double>>& C, vector<vector<double>>& D, vector<double>& J, vector<double>& E, int& nextVariable) override {}

    void update(double dt) override {}
};

class Circuit {
    vector<Component*> components;
    int maxNode;

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

        for (auto comp : components) {
            if (comp->type == VOLTAGE_SOURCE) {
                numVSources++;
            }
        }

        int numVars = numNodes + numVSources;

        vector<vector<double>> G(numNodes, vector<double>(numNodes, 0.0));
        vector<vector<double>> B(numNodes, vector<double>(numVSources, 0.0));
        vector<vector<double>> C(numVSources, vector<double>(numNodes, 0.0));
        vector<vector<double>> D(numVSources, vector<double>(numVSources, 0.0));
        vector<double> J(numNodes, 0.0);
        vector<double> E(numVSources, 0.0);

        int vsCount = 0;
        for (auto comp : components) {
            comp->stamp(G, B, C, D, J, E, vsCount);
        }

        vector<vector<double>> A(numVars, vector<double>(numVars, 0.0));
        vector<double> b(numVars, 0.0);

        for (int i = 0; i < numNodes; i++) {
            for (int j = 0; j < numNodes; j++) {
                A[i][j] = G[i][j];
            }
            for (int j = 0; j < numVSources; j++) {
                A[i][numNodes + j] = B[i][j];
            }
        }
        for (int i = 0; i < numVSources; i++) {
            for (int j = 0; j < numNodes; j++) {
                A[numNodes + i][j] = C[i][j];
            }
            for (int j = 0; j < numVSources; j++) {
                A[numNodes + i][numNodes + j] = D[i][j];
            }
        }

        for (int i = 0; i < numNodes; i++) {
            b[i] = J[i];
        }
        for (int i = 0; i < numVSources; i++) {
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
        cout << "\nTransient analysis would be implemented here\n";
    }
};

int main() {
    Circuit circuit;
    string type;
    int n1, n2, VCount = 0, RCount = 0, CCount = 0, LCount = 0, ICount = 0, DCount = 0;
    double val;

    cout << "input format:\nType(V, R, C, L, I, D) - node1 - node2 - value\ntype analyze to analyze! (best tip doesn't exi...)\n";

    while (true) {
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
            }
        }
    }

    circuit.analyzeDC();
 cout << "test"<< endl;
    return 0;
}