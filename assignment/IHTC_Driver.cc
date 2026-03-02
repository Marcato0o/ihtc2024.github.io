#include "IHTC_Data.hh"
#include "IHTC_Solver.hh"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

using nlohmann::json;

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
    // Print parsing summary
    std::cout << "Parsed instance summary:\n";
    std::cout << "  patients: " << data.patients.size() << "\n";
    std::cout << "  rooms:    " << data.rooms.size() << "\n";
    std::cout << "  nurses:   " << data.nurses.size() << "\n";
    std::cout << "  surgeons:" << data.surgeons.size() << "\n";
    std::cout << "  OTs:      " << data.ots.size() << "\n";
    std::cout << "  days D:   " << data.D << "\n";
    std::cout << "  shifts/d: " << data.shifts_per_day << "\n";

    // Run solver
    IHTC_Output out_data;
    IHTC_Solver solver(data, out_data);
    solver.greedySolve();

    // Build solution JSON from solver output
    json sol;
    sol["admitted"] = json::array();
    for (size_t pid = 0; pid < data.patients.size(); ++pid) {
        if (pid < out_data.admitted.size() && out_data.admitted[pid]) {
            json ja;
            ja["id"] = data.patients[pid].id;
            ja["admission_day"] = out_data.admit_day[pid];
            if (out_data.room_assigned_idx[pid] >= 0 && out_data.room_assigned_idx[pid] < (int)data.rooms.size())
                ja["room"] = data.rooms[out_data.room_assigned_idx[pid]].id;
            else
                ja["room"] = nullptr;
            if (out_data.ot_assigned_idx[pid] >= 0 && out_data.ot_assigned_idx[pid] < (int)data.ots.size())
                ja["ot"] = data.ots[out_data.ot_assigned_idx[pid]].id;
            else
                ja["ot"] = nullptr;
            sol["admitted"].push_back(ja);
        }
    }

    // write to file
    std::ofstream fout(out);
    if (!fout) {
        std::cerr << "Error opening output file: " << out << "\n";
        return 3;
    }
    fout << sol.dump(2) << std::endl;
    std::cout << "Wrote solution to " << out << "\n";
    return 0;
}
