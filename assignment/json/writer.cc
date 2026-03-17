#include "io.hh"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <unordered_map>
#include "../nlohmann/json.hpp"

namespace {

std::string shift_name_from_index(int idx) {
    if (idx == 0) return "early";
    if (idx == 1) return "late";
    return "night";
}

} // namespace

namespace jsonio {

void write_solution(const IHTC_Input &in, const IHTC_Output &out, const std::string &filename) {
    nlohmann::ordered_json sol = nlohmann::ordered_json::object();
    sol["patients"] = nlohmann::ordered_json::array();
    sol["nurses"] = nlohmann::ordered_json::array();
    sol["costs"] = nlohmann::ordered_json::array();

    for (size_t pid = 0; pid < in.patients.size(); ++pid) {
        nlohmann::ordered_json ja = nlohmann::ordered_json::object();
        ja["id"] = in.patients[pid].id;

        bool is_admitted = out.isAdmitted((int)pid);
        if (is_admitted) {
            ja["admission_day"] = out.getAdmitDay((int)pid);

            int room_idx = out.getRoomAssignedIdx((int)pid);
            if (room_idx >= 0 && room_idx < (int)in.rooms.size())
                ja["room"] = in.rooms[room_idx].id;
            else
                ja["room"] = nullptr;

            int ot_idx = out.getOtAssignedIdx((int)pid);
            if (ot_idx >= 0 && ot_idx < (int)in.ots.size())
                ja["operating_theater"] = in.ots[ot_idx].id;
            else
                ja["operating_theater"] = nullptr;
        } else {
            ja["admission_day"] = "none";
        }

        sol["patients"].push_back(ja);
    }

    int days = in.D > 0 ? in.D : 1;
    int shifts = std::max(1, in.shifts_per_day);

    std::vector<std::vector<std::vector<std::set<std::string>>>> nurse_day_shift_rooms(
        in.nurses.size(), std::vector<std::vector<std::set<std::string>>>(days, std::vector<std::set<std::string>>(shifts)));

    std::vector<std::tuple<int, int, int, int>> nurse_assignments = out.getNurseAssignmentTuples();
    for (const auto &na : nurse_assignments) {
        int nurse_idx = std::get<0>(na);
        int day = std::get<1>(na);
        int shift = std::get<2>(na);
        int room_idx = std::get<3>(na);
        if (nurse_idx < 0 || nurse_idx >= (int)in.nurses.size()) continue;
        if (day < 0 || day >= days) continue;
        if (shift < 0 || shift >= shifts) continue;
        if (room_idx < 0 || room_idx >= (int)in.rooms.size()) continue;

        nurse_day_shift_rooms[nurse_idx][day][shift].insert(in.rooms[room_idx].id);
    }

    for (int nurse_idx = 0; nurse_idx < (int)in.nurses.size(); ++nurse_idx) {
        nlohmann::ordered_json nurse_out = nlohmann::ordered_json::object();
        nurse_out["id"] = in.nurses[nurse_idx].id;
        nurse_out["assignments"] = nlohmann::ordered_json::array();

        for (int d = 0; d < days; ++d) {
            for (int sh = 0; sh < shifts; ++sh) {
                const auto &rooms_set = nurse_day_shift_rooms[nurse_idx][d][sh];
                if (rooms_set.empty()) continue;
                nlohmann::ordered_json asg = nlohmann::ordered_json::object();
                asg["day"] = d;
                asg["shift"] = shift_name_from_index(sh);
                asg["rooms"] = nlohmann::ordered_json::array();
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
    if (!fout) {
        std::cerr << "Error opening output file: " << filename << "\n";
        return;
    }
    fout << sol.dump(2) << std::endl;
    std::cout << "Wrote solution to " << filename << "\n";
}

} // namespace jsonio
