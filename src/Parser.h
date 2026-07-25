#ifndef NUTSPICE_PARSER_H
#define NUTSPICE_PARSER_H

#include <string>

namespace nutspice
{

    class Circuit;

    // Parses a SPICE-style numeric string with optional engineering suffix
    // (k, meg, m, u, n, p, f, etc.).  Returns 0.0 for unparseable input.
    double parseSpiceValue(const std::string &valStr);

    // Reads a SPICE text file and populates `circuit` with components.
    // Recognised line types:
    //   R/C/L <name> <node1> <node2> <value>
    //   V <name> <node1> <node2> <value>
    //   VS <name> <node1> <node2> <dc> <amp> <freq> [phase]
    //   VP <name> <node1> <node2> <v1> <v2> <td> <tr> <tf> <pw> <per>
    //   I/IS/IP analogous
    //   D <name> <node1> <node2>
    //   GND <name> <node>
    //   E <name> <n1> <n2> <cn1> <cn2> <gain>      (VCVS)
    //   G <name> <n1> <n2> <cn1> <cn2> <gm>        (VCCS)
    //   H <name> <n1> <n2> <vsource_name> <gain>   (CCVS)
    //   F <name> <n1> <n2> <vsource_name> <gain>   (CCCS)
    void processCircuitFile(const std::string &filename, Circuit &circuit);

    // Writes a circuit's textual representation back out to a .txt file.
    bool saveCircuitToFile(const Circuit &circuit, const std::string &filename);

} // namespace nutspice

#endif // NUTSPICE_PARSER_H
