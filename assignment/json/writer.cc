#include "io.hh"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include "../nlohmann/json.hpp"

namespace { // internal helpers

std::string shift_name_from_index(int idx) {
    if (idx == 0) return "early";
    if (idx == 1) return "late";
    return "night";
}

} // namespace

namespace jsonio {

void write_solution(const IHTC_Input &in, const IHTC_Output &out, const std::string &filename) {
    nlohmann::json sol = nlohmann::json::object();
    sol["patients"] = nlohmann::json::array();
    sol["nurses"] = nlohmann::json::array();
    sol["costs"] = nlohmann::json::array();

    for (size_t pid = 0; pid < in.patients.size(); ++pid) {
        nlohmann::json ja = nlohmann::json::object();
        ja["id"] = in.patients[pid].id;
        if (out.isAdmitted((int)pid)) {
            ja["admission_day"] = out.getAdmitDay((int)pid);
            ja["room"] = in.rooms[out.getRoomAssignedIdx((int)pid)].id;
            ja["operating_theater"] = in.ots[out.getOtAssignedIdx((int)pid)].id;
        } else {
            ja["admission_day"] = "none";
        }
        sol["patients"].push_back(ja);
    }

    int days = in.D;
    int shifts = in.shifts_per_day;
    std::vector<std::vector<std::vector<std::set<std::string>>>> nurse_day_shift_rooms(
        in.nurses.size(), std::vector<std::vector<std::set<std::string>>>(days, std::vector<std::set<std::string>>(shifts)));
    std::vector<std::tuple<int, int, int, int>> nurse_assignments = out.getNurseAssignmentTuples();
    for (const auto &na : nurse_assignments) {
        int nurse_idx = std::get<0>(na);
        int day = std::get<1>(na);
        int shift = std::get<2>(na);
        int room_idx = std::get<3>(na);
        nurse_day_shift_rooms[nurse_idx][day][shift].insert(in.rooms[room_idx].id);
    }
    for (int nurse_idx = 0; nurse_idx < (int)in.nurses.size(); ++nurse_idx) {
        nlohmann::json nurse_out = nlohmann::json::object();
        nurse_out["id"] = in.nurses[nurse_idx].id;
        nurse_out["assignments"] = nlohmann::json::array();
        for (int d = 0; d < days; ++d) {
            for (int sh = 0; sh < shifts; ++sh) {
                const auto &rooms_set = nurse_day_shift_rooms[nurse_idx][d][sh];
                if (rooms_set.empty()) continue;
                nlohmann::json asg = nlohmann::json::object();
                asg["day"] = d;
                asg["shift"] = shift_name_from_index(sh);
                asg["rooms"] = nlohmann::json::array();
                for (const auto &rid : rooms_set) asg["rooms"].push_back(rid);
                nurse_out["assignments"].push_back(asg);
            }
        }
        sol["nurses"].push_back(nurse_out);
    }
    IHTC_Output::CostBreakdown cb = out.computeAllCosts();
    std::ostringstream cost_line;
    cost_line << "Cost: " << cb.total
              << ", Unscheduled: " << cb.unscheduled
              << ",  Delay: " << cb.delay
              << ",  OpenOT: " << cb.open_ot
              << ",  AgeMix: " << cb.age_mix
              << ",  Skill: " << cb.skill
              << ",  Excess: " << cb.excess
              << ",  Continuity: " << cb.continuity
              << ",  SurgeonTransfer: " << cb.surgeon_transfer;
    sol["costs"].push_back(cost_line.str());
    std::ofstream fout(filename);
    fout << sol.dump(2) << std::endl;
    std::cout << "Wrote solution to " << filename << "\n";
}

} // namespace jsonio
