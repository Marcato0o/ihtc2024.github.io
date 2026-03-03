#include "IHTC_Data.hh"
#include "IHTC_Greedy.hh"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <unordered_map>
#include <set>
#include <sstream>
#include <nlohmann/json.hpp>


static std::string shiftNameFromIndex(int idx) {
    if (idx == 0) return "early";
    if (idx == 1) return "late";
    return "night";
}

static int shiftIndexFromName(const std::string &name) {
    if (name == "early") return 0;
    if (name == "late") return 1;
    if (name == "night") return 2;
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file>" << std::endl;
        std::exit(1);
    }
    const std::string out = "solution.json";

    IHTC_Input in(argv[1]);
    if (in.patients.empty() && in.rooms.empty() && in.ots.empty() && in.nurses.empty() && in.surgeons.empty() && in.occupants.empty()) {
        std::cerr << "Error loading instance.\n";
        return 2;
    }
    // Print parsing summary
    std::cout << "Parsed instance summary:\n";
    std::cout << "  patients: " << in.patients.size() << "\n";
    std::cout << "  rooms:    " << in.rooms.size() << "\n";
    std::cout << "  nurses:   " << in.nurses.size() << "\n";
    std::cout << "  surgeons:" << in.surgeons.size() << "\n";
    std::cout << "  OTs:      " << in.ots.size() << "\n";
    std::cout << "  days D:   " << in.D << "\n";
    std::cout << "  shifts/d: " << in.shifts_per_day << "\n";

    // Run greedy solver via WL-style free function
    IHTC_Output out_data(in);
    GreedyIHTCSolver(in, out_data);

    // Build solution JSON in the expected structure:
    // {
    //   "patients": [ ... ],
    //   "nurses": [ ... ],
    //   "costs": [ ... ]
    // }
    nlohmann::ordered_json sol = nlohmann::ordered_json::object();
    // Keep exact key order requested by the output schema.
    sol["patients"] = nlohmann::ordered_json::array();
    sol["nurses"] = nlohmann::ordered_json::array();
    sol["costs"] = nlohmann::ordered_json::array();
    for (size_t pid = 0; pid < in.patients.size(); ++pid) {
        nlohmann::ordered_json ja = nlohmann::ordered_json::object();
        ja["id"] = in.patients[pid].id;

        bool is_admitted = (pid < out_data.admitted.size() && out_data.admitted[pid]);
        if (is_admitted) {
            ja["admission_day"] = out_data.admit_day[pid];

            if (out_data.room_assigned_idx[pid] >= 0 && out_data.room_assigned_idx[pid] < (int)in.rooms.size())
                ja["room"] = in.rooms[out_data.room_assigned_idx[pid]].id;
            else
                ja["room"] = nullptr;

            if (out_data.ot_assigned_idx[pid] >= 0 && out_data.ot_assigned_idx[pid] < (int)in.ots.size())
                ja["operating_theater"] = in.ots[out_data.ot_assigned_idx[pid]].id;
            else
                ja["operating_theater"] = nullptr;
        } else {
            ja["admission_day"] = "none";
        }

        sol["patients"].push_back(ja);
    }

    // Build occupied rooms by day from admitted patients and precompute room-day-shift demands.
    int days = in.D > 0 ? in.D : 1;
    int shifts = std::max(1, in.shifts_per_day);
    std::vector<std::set<std::string>> occupied_rooms_by_day(days);
    std::vector<std::vector<std::vector<int>>> room_shift_load(days, std::vector<std::vector<int>>(shifts, std::vector<int>(in.rooms.size(), 0)));
    std::vector<std::vector<std::vector<int>>> room_shift_skill(days, std::vector<std::vector<int>>(shifts, std::vector<int>(in.rooms.size(), 0)));

    // Include pre-existing occupants into occupancy and nurse-demand tensors.
    std::unordered_map<std::string, int> room_name_to_idx;
    for (int i = 0; i < (int)in.rooms.size(); ++i) room_name_to_idx[in.rooms[i].id] = i;
    for (const auto &f : in.occupants) {
        auto it = room_name_to_idx.find(f.room_id);
        if (it == room_name_to_idx.end()) continue;
        int ridx = it->second;
        int start = std::max(0, f.admission_day);
        int los = std::max(1, f.length_of_stay);
        for (int dd = 0; dd < los; ++dd) {
            int d = start + dd;
            if (d < 0 || d >= days) continue;
            occupied_rooms_by_day[d].insert(in.rooms[ridx].id);
            for (int sh = 0; sh < shifts; ++sh) {
                int idx = dd * shifts + sh;
                int load = 1;
                if (!f.nurse_load_per_shift.empty()) {
                    if (idx < (int)f.nurse_load_per_shift.size()) load = f.nurse_load_per_shift[idx];
                    else load = f.nurse_load_per_shift.back();
                }
                room_shift_load[d][sh][ridx] += load;
                room_shift_skill[d][sh][ridx] = std::max(room_shift_skill[d][sh][ridx], f.min_nurse_level);
            }
        }
    }

    for (size_t pid = 0; pid < in.patients.size(); ++pid) {
        bool is_admitted = (pid < out_data.admitted.size() && out_data.admitted[pid]);
        if (!is_admitted) {
            continue;
        }

        int admit_day = out_data.admit_day[pid];
        const auto &p = in.patients[pid];
        int los = std::max(1, p.length_of_stay);
        int room_idx = out_data.room_assigned_idx[pid];
        if (room_idx >= 0 && room_idx < (int)in.rooms.size()) {
            for (int d = admit_day; d < admit_day + los && d < days; ++d) {
                if (d >= 0) {
                    occupied_rooms_by_day[d].insert(in.rooms[room_idx].id);
                }
            }

            for (int dd = 0; dd < los; ++dd) {
                int d = admit_day + dd;
                if (d < 0 || d >= days) continue;
                for (int sh = 0; sh < shifts; ++sh) {
                    int idx = dd * shifts + sh;
                    int load = 1;
                    if (!p.nurse_load_per_shift.empty()) {
                        if (idx < (int)p.nurse_load_per_shift.size()) load = p.nurse_load_per_shift[idx];
                        else load = p.nurse_load_per_shift.back();
                    }
                    room_shift_load[d][sh][room_idx] += load;
                    room_shift_skill[d][sh][room_idx] = std::max(room_shift_skill[d][sh][room_idx], p.min_nurse_level);
                }
            }
        }
    }

    // Build nurse assignments from original instance JSON when available.
    json raw;
    try {
        raw = json::parse(in.raw_json_text);
    } catch (...) {
        raw = json::object();
    }

    int nurse_count = 0;
    if (raw.contains("nurses") && raw["nurses"].is_array()) nurse_count = (int)raw["nurses"].size();
    std::vector<int> nurse_level(nurse_count, 0), nurse_max_load(nurse_count, 9999);
    for (int i = 0; i < nurse_count && i < (int)in.nurses.size(); ++i) {
        nurse_level[i] = in.nurses[i].level;
        nurse_max_load[i] = in.nurses[i].max_load;
    }

    std::vector<std::vector<int>> nurse_load_by_shift(nurse_count, std::vector<int>(days * shifts, 0));
    std::vector<std::vector<std::vector<std::vector<int>>>> room_shift_nurses(days, std::vector<std::vector<std::vector<int>>>(shifts, std::vector<std::vector<int>>(in.rooms.size())));

    if (raw.contains("nurses") && raw["nurses"].is_array()) {
        int nurse_idx = 0;
        for (const auto &jn : raw["nurses"]) {
            nlohmann::ordered_json nurse_out = nlohmann::ordered_json::object();
            std::string nid = jn.contains("id") && jn["id"].is_string() ? jn["id"].get<std::string>() : ("n" + std::to_string(nurse_idx));
            // Keep nurse key order: id first, then assignments.
            nurse_out["id"] = nid;
            nurse_out["assignments"] = nlohmann::ordered_json::array();

            // Preferred schema: working_shifts = [{day, shift}, ...]
            if (jn.contains("working_shifts") && jn["working_shifts"].is_array()) {
                for (const auto &ws : jn["working_shifts"]) {
                    int day = (ws.contains("day") && ws["day"].is_number_integer()) ? ws["day"].get<int>() : -1;
                    std::string shift = (ws.contains("shift") && ws["shift"].is_string()) ? ws["shift"].get<std::string>() : "early";

                    nlohmann::ordered_json asg = nlohmann::ordered_json::object();
                    asg["day"] = day;
                    asg["shift"] = shift;
                    asg["rooms"] = nlohmann::ordered_json::array();
                    int sh_idx = shiftIndexFromName(shift);

                    if (day >= 0 && day < days && !occupied_rooms_by_day[day].empty()) {
                        // Cover all occupied rooms for this shift/day to satisfy H8.
                        std::vector<std::string> room_names(occupied_rooms_by_day[day].begin(), occupied_rooms_by_day[day].end());
                        for (const auto &chosen : room_names) {
                            asg["rooms"].push_back(chosen);
                            auto it = room_name_to_idx.find(chosen);
                            if (it != room_name_to_idx.end() && sh_idx >= 0 && sh_idx < shifts) {
                                int ridx = it->second;
                                room_shift_nurses[day][sh_idx][ridx].push_back(nurse_idx);
                                nurse_load_by_shift[nurse_idx][day * shifts + sh_idx] += room_shift_load[day][sh_idx][ridx];
                            }
                        }
                    }
                    nurse_out["assignments"].push_back(asg);
                }
            } else {
                // Fallback schema: build from parsed roster if present in data.nurses.
                if (nurse_idx < (int)in.nurses.size() && !in.nurses[nurse_idx].roster.empty()) {
                    const auto &roster = in.nurses[nurse_idx].roster;
                    for (int g = 0; g < (int)roster.size(); ++g) {
                        if (roster[g] <= 0) continue;
                        int day = g / std::max(1, in.shifts_per_day);
                        int sh = g % std::max(1, in.shifts_per_day);
                        nlohmann::ordered_json asg = nlohmann::ordered_json::object();
                        asg["day"] = day;
                        asg["shift"] = shiftNameFromIndex(sh);
                        asg["rooms"] = nlohmann::ordered_json::array();
                        if (day >= 0 && day < days && !occupied_rooms_by_day[day].empty()) {
                            std::vector<std::string> room_names(occupied_rooms_by_day[day].begin(), occupied_rooms_by_day[day].end());
                            for (const auto &chosen : room_names) {
                                asg["rooms"].push_back(chosen);
                                auto it = room_name_to_idx.find(chosen);
                                if (it != room_name_to_idx.end() && sh >= 0 && sh < shifts) {
                                    int ridx = it->second;
                                    room_shift_nurses[day][sh][ridx].push_back(nurse_idx);
                                    nurse_load_by_shift[nurse_idx][day * shifts + sh] += room_shift_load[day][sh][ridx];
                                }
                            }
                        }
                        nurse_out["assignments"].push_back(asg);
                    }
                }
            }

            sol["nurses"].push_back(nurse_out);
            nurse_idx++;
        }
    }

    int age_mix_weighted = out_data.ComputeCostRoomMixedAge();
    int skill_weighted = out_data.ComputeCostRoomNurseSkill();
    int continuity_weighted = out_data.ComputeCostContinuityOfCare();
    int excess_weighted = out_data.ComputeCostNurseExcessiveWorkload();
    int open_ot_cost = out_data.ComputeCostOpenOperatingTheater();
    int surgeon_transfer_weighted = out_data.ComputeCostSurgeonTransfer();
    int delay_cost = out_data.ComputeCostPatientDelay();
    int unscheduled_cost = out_data.ComputeCostUnscheduledOptional();
    int total_cost = out_data.ComputeCostTotal();

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
