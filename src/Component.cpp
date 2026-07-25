#include "Component.h"
#include "Theme.h"

#include <map>
#include <string>

namespace nutspice
{
    std::string getNodeName(int nodeNumber); // implemented in Placement.cpp
}

namespace nutspice
{

    std::string componentTypeLabel(ComponentType t)
    {
        switch (t)
        {
        case RESISTOR:
            return "Resistor";
        case CAPACITOR:
            return "Capacitor";
        case INDUCTOR:
            return "Inductor";
        case VOLTAGE_SOURCE:
            return "Voltage Source";
        case CURRENT_SOURCE:
            return "Current Source";
        case DIODE:
            return "Diode";
        case GROUND:
            return "Ground";
        case SIN_VOLTAGE_SOURCE:
            return "Sinusoidal Voltage Source";
        case PULSE_VOLTAGE_SOURCE:
            return "Pulse Voltage Source";
        case SIN_CURRENT_SOURCE:
            return "Sinusoid Current Source";
        case PULSE_CURRENT_SOURCE:
            return "Pulse Current Source";
        case VCVS_SOURCE:
            return "VCVS Source";
        case VCCS_SOURCE:
            return "VCCS Source";
        case CCVS_SOURCE:
            return "CCVS Source";
        case CCCS_SOURCE:
            return "CCCS Source";
        }
        return "Unknown";
    }

    std::string Component::getInfo() const
    {
        return componentTypeLabel(type) + " " + name + " " +
               getNodeName(node1) + " " + getNodeName(node2) + " " +
               std::to_string(value);
    }

} // namespace nutspice
