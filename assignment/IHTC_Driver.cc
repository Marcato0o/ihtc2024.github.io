#include "IHTC_Data.hh"
#include <iostream>

int main(int argc, char **argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " instance.json solution.json\n";
        return 1;
    }
    const std::string inst = argv[1];
    const std::string out = argv[2];

    IHTC_Data data;
    if (!data.loadInstance(inst)) {
        std::cerr << "Error loading instance.\n";
        return 2;
    }

    // TODO: call greedy solver implementation to populate solution
    if (!data.runGreedyTodo()) {
        std::cerr << "Greedy todo failed.\n";
        return 4;
    }
    if (!data.writeSolution(out)) {
        std::cerr << "Error writing solution.\n";
        return 3;
    }

    std::cout << "Wrote placeholder solution to " << out << "\n";
    return 0;
}
