#include "Parser.h"
#include "Circuit.h"
#include "Passives.h"
#include "Sources.h"
#include "DependentSources.h"
#include "Platform.h"
#include "Placement.h" // brings in the global GUI state (nodeMap, getOrCreateNode, ...)

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace nutspice
{

    double parseSpiceValue(const std::string &valStr)
    {
        if (valStr.empty())
            return 0.0;

        bool isNumber = true;
        bool hasDot = false, hasE = false, hasSign = false;
        for (size_t i = 0; i < valStr.size(); i++)
        {
            char c = valStr[i];
            if (c == '.' && !hasDot)
                hasDot = true;
            else if ((c == 'e' || c == 'E') && !hasE)
            {
                hasE = true;
                hasDot = false;
                hasSign = false;
            }
            else if ((c == '+' || c == '-') && (i == 0 || hasE))
                hasSign = true;
            else if (!std::isdigit(static_cast<unsigned char>(c)))
            {
                isNumber = false;
                break;
            }
        }
        if (isNumber)
            return std::stod(valStr);

        size_t suffixPos = 0;
        while (suffixPos < valStr.size() &&
               (std::isdigit(static_cast<unsigned char>(valStr[suffixPos])) ||
                valStr[suffixPos] == '.' || valStr[suffixPos] == '-' ||
                valStr[suffixPos] == '+' || valStr[suffixPos] == 'e' || valStr[suffixPos] == 'E'))
        {
            suffixPos++;
        }
        if (suffixPos == 0)
            return 0.0;

        double num = std::stod(valStr.substr(0, suffixPos));
        std::string suffix = valStr.substr(suffixPos);
        std::transform(suffix.begin(), suffix.end(), suffix.begin(),
                       [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });

        if (suffix == "t")
            return num * 1e12;
        if (suffix == "g")
            return num * 1e9;
        if (suffix == "meg")
            return num * 1e6;
        if (suffix == "k")
            return num * 1e3;
        if (suffix == "m")
            return num * 1e-3;
        if (suffix == "u")
            return num * 1e-6;
        if (suffix == "n")
            return num * 1e-9;
        if (suffix == "p")
            return num * 1e-12;
        if (suffix == "f")
            return num * 1e-15;
        return num;
    }

    void processCircuitFile(const std::string &filename, Circuit &circuit)
    {
        std::string extension = filename.substr(filename.find_last_of('.') + 1);
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });

        if (extension == "cir")
        {
            if (!circuit.loadFromFile(filename))
            {
                std::cerr << "Error: Could not load circuit file '" << filename << "'\n"
                          << "  cwd: " << nutspice::absolutePath(nutspice::currentDirectory()) << std::endl;
            }
            else
            {
                std::cout << "Successfully loaded circuit from: " << filename << std::endl;
                // Sync global state used by the GUI for node lookups.
                nodeMap = circuit.nodeMap;
                reverseNodeMap = circuit.reverseNodeMap;
                nextNodeNumber = circuit.nextNodeNumber;
                nodePositions = circuit.nodePositions;
            }
            return;
        }

        std::ifstream file(filename);
        if (!file.is_open())
        {
            std::string withExtension = filename;
            if (filename.find(".txt") == std::string::npos)
            {
                withExtension += ".txt";
                file.open(withExtension);
            }
            if (!file.is_open())
            {
                std::cerr << "Error: Could not open file '" << filename << "'\n"
                          << "  cwd: " << nutspice::currentDirectory() << std::endl;
                return;
            }
        }

        std::string line;
        while (std::getline(file, line))
        {
            if (line.empty() || line[0] == '*')
                continue;

            std::istringstream iss(line);
            std::string type, name, node1, node2;
            iss >> type >> name >> node1 >> node2;
            if (type.empty())
                continue;

            try
            {
                char typeChar = static_cast<char>(std::toupper(static_cast<unsigned char>(type[0])));

                if (typeChar == 'G' && type.size() >= 3 &&
                    std::toupper(static_cast<unsigned char>(type[1])) == 'N' &&
                    std::toupper(static_cast<unsigned char>(type[2])) == 'D')
                {
                    circuit.addComponent(new Ground("GND", getOrCreateNode(node1)));
                    continue;
                }

                int n1 = getOrCreateNode(node1);
                int n2 = getOrCreateNode(node2);

                switch (typeChar)
                {
                case 'R':
                case 'C':
                case 'L':
                {
                    std::string valStr;
                    if (!(iss >> valStr))
                    {
                        std::cerr << "Error: Missing value for component " << name << "\n";
                        continue;
                    }
                    double value = parseSpiceValue(valStr);
                    if (value <= 0)
                    {
                        std::cerr << "Error: Value must be positive for " << name << "\n";
                        continue;
                    }
                    if (typeChar == 'R')
                        circuit.addComponent(new Resistor(name, n1, n2, value));
                    else if (typeChar == 'C')
                        circuit.addComponent(new Capacitor(name, n1, n2, value));
                    else
                        circuit.addComponent(new Inductor(name, n1, n2, value));
                    break;
                }
                case 'V':
                {
                    if (type.size() > 1 && std::toupper(static_cast<unsigned char>(type[1])) == 'S')
                    {
                        std::string dcStr, ampStr, freqStr, phaseStr = "0";
                        if (!(iss >> dcStr >> ampStr >> freqStr))
                        {
                            std::cerr << "Error: Invalid sinusoidal source parameters\n";
                            continue;
                        }
                        iss >> phaseStr;
                        circuit.addComponent(new SinVoltageSource(
                            name, n1, n2,
                            parseSpiceValue(ampStr),
                            parseSpiceValue(freqStr),
                            parseSpiceValue(phaseStr),
                            parseSpiceValue(dcStr)));
                    }
                    else if (type.size() > 1 && std::toupper(static_cast<unsigned char>(type[1])) == 'P')
                    {
                        std::string v1Str, v2Str, tdStr, trStr, tfStr, pwStr, perStr;
                        if (!(iss >> v1Str >> v2Str >> tdStr >> trStr >> tfStr >> pwStr >> perStr))
                        {
                            std::cerr << "Error: Invalid pulse source parameters\n";
                            continue;
                        }
                        circuit.addComponent(new PulseVoltageSource(
                            name, n1, n2,
                            parseSpiceValue(v1Str), parseSpiceValue(v2Str),
                            parseSpiceValue(tdStr), parseSpiceValue(trStr),
                            parseSpiceValue(tfStr), parseSpiceValue(pwStr),
                            parseSpiceValue(perStr)));
                    }
                    else
                    {
                        std::string valStr;
                        if (!(iss >> valStr))
                        {
                            std::cerr << "Error: Missing value for voltage source " << name << "\n";
                            continue;
                        }
                        circuit.addComponent(new VoltageSource(name, n1, n2, parseSpiceValue(valStr)));
                    }
                    break;
                }
                case 'I':
                {
                    if (type.size() > 1 && std::toupper(static_cast<unsigned char>(type[1])) == 'S')
                    {
                        std::string dcStr, ampStr, freqStr, phaseStr = "0";
                        if (!(iss >> dcStr >> ampStr >> freqStr))
                        {
                            std::cerr << "Error: Invalid sinusoidal source parameters\n";
                            continue;
                        }
                        iss >> phaseStr;
                        circuit.addComponent(new SinCurrentSource(
                            name, n1, n2,
                            parseSpiceValue(ampStr),
                            parseSpiceValue(freqStr),
                            parseSpiceValue(phaseStr),
                            parseSpiceValue(dcStr)));
                    }
                    else if (type.size() > 1 && std::toupper(static_cast<unsigned char>(type[1])) == 'P')
                    {
                        std::string i1Str, i2Str, tdStr, trStr, tfStr, pwStr, perStr;
                        if (!(iss >> i1Str >> i2Str >> tdStr >> trStr >> tfStr >> pwStr >> perStr))
                        {
                            std::cerr << "Error: Invalid pulse source parameters\n";
                            continue;
                        }
                        circuit.addComponent(new PulseCurrentSource(
                            name, n1, n2,
                            parseSpiceValue(i1Str), parseSpiceValue(i2Str),
                            parseSpiceValue(tdStr), parseSpiceValue(trStr),
                            parseSpiceValue(tfStr), parseSpiceValue(pwStr),
                            parseSpiceValue(perStr)));
                    }
                    else
                    {
                        std::string valStr;
                        if (!(iss >> valStr))
                        {
                            std::cerr << "Error: Missing value for current source " << name << "\n";
                            continue;
                        }
                        circuit.addComponent(new CurrentSource(name, n1, n2, parseSpiceValue(valStr)));
                    }
                    break;
                }
                case 'D':
                    circuit.addComponent(new Diode(name, n1, n2));
                    break;

                case 'E':
                { // VCVS
                    std::string cn1, cn2, gainStr;
                    if (!(iss >> cn1 >> cn2 >> gainStr))
                    {
                        std::cerr << "Error: Invalid VCVS parameters\n";
                        continue;
                    }
                    circuit.addComponent(new VCVS(name, n1, n2,
                                                  getOrCreateNode(cn1),
                                                  getOrCreateNode(cn2),
                                                  parseSpiceValue(gainStr)));
                    break;
                }
                case 'G':
                { // VCCS
                    std::string cn1, cn2, gmStr;
                    if (!(iss >> cn1 >> cn2 >> gmStr))
                    {
                        std::cerr << "Error: Invalid VCCS parameters\n";
                        continue;
                    }
                    circuit.addComponent(new VCCS(name, n1, n2,
                                                  getOrCreateNode(cn1),
                                                  getOrCreateNode(cn2),
                                                  parseSpiceValue(gmStr)));
                    break;
                }
                case 'H':
                { // CCVS
                    std::string vsName, gainStr;
                    if (!(iss >> vsName >> gainStr))
                    {
                        std::cerr << "Error: Invalid CCVS parameters\n";
                        continue;
                    }
                    circuit.addComponent(new CCVS(name, n1, n2, vsName, parseSpiceValue(gainStr)));
                    break;
                }
                case 'F':
                { // CCCS
                    std::string vsName, gainStr;
                    if (!(iss >> vsName >> gainStr))
                    {
                        std::cerr << "Error: Invalid CCCS parameters\n";
                        continue;
                    }
                    circuit.addComponent(new CCCS(name, n1, n2, vsName, parseSpiceValue(gainStr)));
                    break;
                }
                default:
                    std::cerr << "Error: Unknown component type " << type << "\n";
                }
                std::cout << "Added component: " << line << std::endl;
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error processing line: " << line << " - " << e.what() << std::endl;
            }
        }
        file.close();
    }

    bool saveCircuitToFile(const Circuit &circuit, const std::string &filename)
    {
        std::string actualFilename = filename;
        if (actualFilename.find(".txt") == std::string::npos)
            actualFilename += ".txt";

        std::ofstream file(actualFilename);
        if (!file.is_open())
        {
            std::cerr << "Error: Could not open file '" << actualFilename << "' for writing." << std::endl;
            return false;
        }

        for (const auto &info : circuit.listComponents())
        {
            file << info << "\n";
        }
        file.close();

        std::cout << "SUCCESS: Circuit saved to " << nutspice::absolutePath(actualFilename) << std::endl;
        return true;
    }

} // namespace nutspice
