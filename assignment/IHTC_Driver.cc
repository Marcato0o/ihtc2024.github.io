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

static int shiftIndexFromName(const std::string &name) {
    if (name == "early") return 0;
    if (name == "late") return 1;
    if (name == "night") return 2;
    return 0;
}

static std::string ageGroupKey(const Patient &p) {
    if (p.raw_json.contains("age_group") && !p.raw_json["age_group"].is_null()) {
        if (p.raw_json["age_group"].is_string()) return p.raw_json["age_group"].get<std::string>();
        if (p.raw_json["age_group"].is_number_integer()) return std::to_string(p.raw_json["age_group"].get<int>());
    }
    if (p.age_group >= 0) return std::to_string(p.age_group);
    return "unknown";
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

    // Build occupied rooms by day from admitted patients and precompute room-day-shift demands.
    int days = in.D > 0 ? in.D : 1;
    int shifts = std::max(1, in.shifts_per_day);
    std::vector<std::set<std::string>> occupied_rooms_by_day(days);
    int admitted_count = 0;
    int unscheduled_count = 0;
    int total_delay = 0;
    std::vector<std::set<std::string>> ot_by_day(days);
    std::vector<std::vector<std::vector<int>>> room_shift_load(days, std::vector<std::vector<int>>(shifts, std::vector<int>(in.rooms.size(), 0)));
    std::vector<std::vector<std::vector<int>>> room_shift_skill(days, std::vector<std::vector<int>>(shifts, std::vector<int>(in.rooms.size(), 0)));
    std::vector<std::vector<std::vector<int>>> patients_in_room_day(in.rooms.size(), std::vector<std::vector<int>>(days));

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
                if (d >= 0) {
                    occupied_rooms_by_day[d].insert(in.rooms[room_idx].id);
                    patients_in_room_day[room_idx][d].push_back((int)pid);
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

    // Build costs summary string in the expected textual format.
    // AgeMix: penalize mixed age-groups in same room/day.
    int age_mix_cost = 0;
    for (int r = 0; r < (int)in.rooms.size(); ++r) {
        for (int d = 0; d < days; ++d) {
            const auto &plist = patients_in_room_day[r][d];
            if (plist.size() < 2) continue;
            std::unordered_map<std::string, int> age_counts;
            for (int pid : plist) age_counts[ageGroupKey(in.patients[pid])]++;
            long long n = (long long)plist.size();
            long long total_pairs = (n * (n - 1)) / 2;
            long long same_pairs = 0;
            for (const auto &kv : age_counts) {
                long long c = kv.second;
                same_pairs += (c * (c - 1)) / 2;
            }
            age_mix_cost += (int)std::max(0LL, total_pairs - same_pairs);
        }
    }

    // Skill: if assigned nurse skill is below room required skill for a shift.
    int skill_cost = 0;
    for (int d = 0; d < days; ++d) {
        for (int sh = 0; sh < shifts; ++sh) {
            for (int r = 0; r < (int)in.rooms.size(); ++r) {
                int req = room_shift_skill[d][sh][r];
                if (req <= 0) continue;
                for (int nidx : room_shift_nurses[d][sh][r]) {
                    if (nidx >= 0 && nidx < (int)nurse_level.size() && nurse_level[nidx] < req) {
                        skill_cost += (req - nurse_level[nidx]);
                    }
                }
            }
        }
    }

    // Excess: nurse assigned load above max_load per shift.
    int excess_cost = 0;
    for (int nidx = 0; nidx < nurse_count; ++nidx) {
        int cap = (nidx < (int)nurse_max_load.size()) ? nurse_max_load[nidx] : 9999;
        for (int t = 0; t < days * shifts; ++t) {
            int over = nurse_load_by_shift[nidx][t] - cap;
            if (over > 0) excess_cost += over;
        }
    }

    // Continuity: each patient should be served by fewer distinct nurses across stay.
    int continuity_cost = 0;
    for (size_t pid = 0; pid < in.patients.size(); ++pid) {
        if (!(pid < out_data.admitted.size() && out_data.admitted[pid])) continue;
        int day0 = out_data.admit_day[pid];
        int ridx = out_data.room_assigned_idx[pid];
        if (ridx < 0 || ridx >= (int)in.rooms.size()) continue;
        int los = std::max(1, in.patients[pid].length_of_stay);
        std::set<int> seen_nurses;
        for (int dd = 0; dd < los; ++dd) {
            int d = day0 + dd;
            if (d < 0 || d >= days) continue;
            for (int sh = 0; sh < shifts; ++sh) {
                for (int nidx : room_shift_nurses[d][sh][ridx]) seen_nurses.insert(nidx);
            }
        }
        if (!seen_nurses.empty()) continuity_cost += std::max(0, (int)seen_nurses.size() - 1);
    }

    // SurgeonTransfer: same surgeon working in multiple OTs in one day.
    int surgeon_transfer_cost = 0;
    std::unordered_map<std::string, std::vector<std::set<std::string>>> surgeon_ot_by_day;
    for (size_t pid = 0; pid < in.patients.size(); ++pid) {
        if (!(pid < out_data.admitted.size() && out_data.admitted[pid])) continue;
        int d = out_data.admit_day[pid];
        int ot_idx = out_data.ot_assigned_idx[pid];
        if (d < 0 || d >= days || ot_idx < 0 || ot_idx >= (int)in.ots.size()) continue;
        const std::string &sid = in.patients[pid].surgeon_id;
        if (sid.empty()) continue;
        if (!surgeon_ot_by_day.count(sid)) surgeon_ot_by_day[sid] = std::vector<std::set<std::string>>(days);
        surgeon_ot_by_day[sid][d].insert(in.ots[ot_idx].id);
    }
    for (const auto &kv : surgeon_ot_by_day) {
        for (int d = 0; d < days; ++d) {
            int sz = (int)kv.second[d].size();
            if (sz > 1) surgeon_transfer_cost += (sz - 1);
        }
    }

    auto getWeight = [&](const std::string &named_key, const std::string &short_key, int fallback) -> int {
        auto it_named = in.weights.find(named_key);
        if (it_named != in.weights.end()) return it_named->second;
        auto it_short = in.weights.find(short_key);
        if (it_short != in.weights.end()) return it_short->second;
        return fallback;
    };

    // Soft constraint weights from instance JSON (S1..S8).
    // S1: room_mixed_age
    // S2: room_nurse_skill
    // S3: continuity_of_care
    // S4: nurse_eccessive_workload
    // S5: open_operating_theater
    // S6: surgeon_transfer
    // S7: patient_delay
    // S8: unscheduled_optional
    int wS1 = getWeight("room_mixed_age", "S1", 5);
    int wS2 = getWeight("room_nurse_skill", "S2", 1);
    int wS3 = getWeight("continuity_of_care", "S3", 1);
    int wS4 = getWeight("nurse_eccessive_workload", "S4", 1);
    int wS5 = getWeight("open_operating_theater", "S5", 10);
    int wS6 = getWeight("surgeon_transfer", "S6", 1);
    int wS7 = getWeight("patient_delay", "S7", 5);
    int wS8 = getWeight("unscheduled_optional", "S8", 100);

    int unscheduled_optional_count = 0;
    for (size_t pid = 0; pid < in.patients.size(); ++pid) {
        bool is_admitted = (pid < out_data.admitted.size() && out_data.admitted[pid]);
        if (!is_admitted && !in.patients[pid].mandatory) unscheduled_optional_count++;
    }

    int delay_cost = total_delay * wS7;
    int unscheduled_cost = unscheduled_optional_count * wS8;
    int open_ot_days = 0;
    for (int d = 0; d < days; ++d) {
        if (!ot_by_day[d].empty()) open_ot_days += (int)ot_by_day[d].size();
    }
    int open_ot_cost = open_ot_days * wS5;
    int age_mix_weighted = age_mix_cost * wS1;
    int skill_weighted = skill_cost * wS2;
    int excess_weighted = excess_cost * wS4;
    int continuity_weighted = continuity_cost * wS3;
    int surgeon_transfer_weighted = surgeon_transfer_cost * wS6;

    int total_cost = unscheduled_cost + delay_cost + open_ot_cost + age_mix_weighted + skill_weighted + excess_weighted + continuity_weighted + surgeon_transfer_weighted;

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
