#include "IHTC_Data.hh"
#include "IHTC_Greedy.hh"

#include <iostream>

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file>" << std::endl;
        return 1;
    }

    IHTC_Input in(argv[1]);
    if (in.patients.empty() && in.rooms.empty() && in.ots.empty() && in.nurses.empty() && in.surgeons.empty() && in.occupants.empty()) {
        std::cerr << "Error loading instance." << std::endl;
        return 2;
    }

    std::cout << "Parsed instance summary:\n";
    std::cout << "  patients: " << in.patients.size() << "\n";
    std::cout << "  rooms:    " << in.rooms.size() << "\n";
    std::cout << "  days D:   " << in.D << "\n";

    IHTC_Output out(in);
    GreedySolver::runFullSolver(in, out);

    out.printCosts();
    out.writeJSON("solution.json");

    return 0;
}
