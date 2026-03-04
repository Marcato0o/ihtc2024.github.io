#include "io.hh"

#include <algorithm>
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

        bool is_admitted = (pid < out.admitted.size() && out.admitted[pid]);
        if (is_admitted) {
            ja["admission_day"] = out.admit_day[pid];

            if (out.room_assigned_idx[pid] >= 0 && out.room_assigned_idx[pid] < (int)in.rooms.size())
                ja["room"] = in.rooms[out.room_assigned_idx[pid]].id;
            else
                ja["room"] = nullptr;

            if (out.ot_assigned_idx[pid] >= 0 && out.ot_assigned_idx[pid] < (int)in.ots.size())
                ja["operating_theater"] = in.ots[out.ot_assigned_idx[pid]].id;
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

    for (const auto &na : out.nurse_assignments) {
        if (na.nurse_idx < 0 || na.nurse_idx >= (int)in.nurses.size()) continue;
        if (na.day < 0 || na.day >= days) continue;
        if (na.shift < 0 || na.shift >= shifts) continue;
        if (na.room_idx < 0 || na.room_idx >= (int)in.rooms.size()) continue;
        nurse_day_shift_rooms[na.nurse_idx][na.day][na.shift].insert(in.rooms[na.room_idx].id);
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

    int age_mix_weighted = out.ComputeCostRoomMixedAge();
    int skill_weighted = out.ComputeCostRoomNurseSkill();
    int continuity_weighted = out.ComputeCostContinuityOfCare();
    int excess_weighted = out.ComputeCostNurseExcessiveWorkload();
    int open_ot_cost = out.ComputeCostOpenOperatingTheater();
    int surgeon_transfer_weighted = out.ComputeCostSurgeonTransfer();
    int delay_cost = out.ComputeCostPatientDelay();
    int unscheduled_cost = out.ComputeCostUnscheduledOptional();
    int total_cost = out.ComputeCostTotal();

    std::ostringstream cost_line;
    cost_line << "Cost: " << total_cost
              << ", Unscheduled: " << unscheduled_cost
              << ",  Delay: " << delay_cost
              << ",  OpenOT: " << open_ot_cost
              << ",  AgeMix: " << age_mix_weighted
              << ",  Skill: " << skill_weighted
              << ",  Excess: " << excess_weighted
              << ",  Continuity: " << continuity_weighted
              << ",  SurgeonTransfer: " << surgeon_transfer_weighted;
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
