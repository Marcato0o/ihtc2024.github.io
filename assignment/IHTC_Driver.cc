#include "IHTC_Data.hh"
#include "IHTC_Greedy.hh"
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <set>
#include <sstream>
#include <nlohmann/json.hpp>


static std::string shiftNameFromIndex(int idx) {
    if (idx == 0) return "early";
    if (idx == 1) return "late";
    return "night";
}

int main(int argc, char **argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " instance.json solution.json\n";
        return 1;
    }
    const std::string inst = argv[1];
    const std::string out = argv[2];

    IHTC_Input in;
    if (!in.loadInstance(inst)) {
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
    IHTC_Output out_data;
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

    // Build occupied rooms by day from admitted patients.
    int days = in.D > 0 ? in.D : 1;
    std::vector<std::set<std::string>> occupied_rooms_by_day(days);
    int admitted_count = 0;
    int unscheduled_count = 0;
    int total_delay = 0;
    std::vector<std::set<std::string>> ot_by_day(days);

    for (size_t pid = 0; pid < in.patients.size(); ++pid) {
        bool is_admitted = (pid < out_data.admitted.size() && out_data.admitted[pid]);
        if (!is_admitted) {
            unscheduled_count++;
            continue;
        }

        admitted_count++;
        int admit_day = out_data.admit_day[pid];
        const auto &p = in.patients[pid];
        int los = std::max(1, p.length_of_stay);
        int room_idx = out_data.room_assigned_idx[pid];
        if (room_idx >= 0 && room_idx < (int)in.rooms.size()) {
            for (int d = admit_day; d < admit_day + los && d < days; ++d) {
                if (d >= 0) occupied_rooms_by_day[d].insert(in.rooms[room_idx].id);
            }
        }

        if (admit_day >= 0) total_delay += std::max(0, admit_day - p.release_date);

        int ot_idx = out_data.ot_assigned_idx[pid];
        if (admit_day >= 0 && admit_day < days && ot_idx >= 0 && ot_idx < (int)in.ots.size()) {
            ot_by_day[admit_day].insert(in.ots[ot_idx].id);
        }
    }

    // Build nurse assignments from original instance JSON when available.
    json raw;
    try {
        raw = json::parse(in.raw_json_text);
    } catch (...) {
        raw = json::object();
    }

    std::unordered_map<std::string, int> room_name_to_idx;
    for (int i = 0; i < (int)in.rooms.size(); ++i) room_name_to_idx[in.rooms[i].id] = i;

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

                    if (day >= 0 && day < days && !occupied_rooms_by_day[day].empty()) {
                        // Lightweight deterministic assignment: one room per nurse-shift, round-robin.
                        std::vector<std::string> room_names(occupied_rooms_by_day[day].begin(), occupied_rooms_by_day[day].end());
                        std::string chosen = room_names[(nurse_idx + day) % room_names.size()];
                        asg["rooms"].push_back(chosen);
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
                            std::string chosen = room_names[(nurse_idx + day + sh) % room_names.size()];
                            asg["rooms"].push_back(chosen);
                        }
                        nurse_out["assignments"].push_back(asg);
                    }
                }
            }

            sol["nurses"].push_back(nurse_out);
            nurse_idx++;
        }
    }

    // Build costs summary string in the expected textual format.
    int delay_cost = total_delay * 10;
    int unscheduled_cost = unscheduled_count * 100;
    int open_ot_days = 0;
    for (int d = 0; d < days; ++d) {
        if (!ot_by_day[d].empty()) open_ot_days += (int)ot_by_day[d].size();
    }
    int open_ot_cost = open_ot_days * 10;
    int age_mix_cost = 0;
    int skill_cost = 0;
    int excess_cost = 0;
    int continuity_cost = 0;
    int surgeon_transfer_cost = 0;
    int total_cost = unscheduled_cost + delay_cost + open_ot_cost + age_mix_cost + skill_cost + excess_cost + continuity_cost + surgeon_transfer_cost;

    std::ostringstream cost_line;
    cost_line << "Cost: " << total_cost
              << ", Unscheduled: " << unscheduled_cost
              << ",  Delay: " << delay_cost
              << ",  OpenOT: " << open_ot_cost
              << ",  AgeMix: " << age_mix_cost
              << ",  Skill: " << skill_cost
              << ",  Excess: " << excess_cost
              << ",  Continuity: " << continuity_cost
              << ",  SurgeonTransfer: " << surgeon_transfer_cost;

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
