#include "IHTC_Greedy.hh"

#include <algorithm>
#include <iostream>
#include <limits>
#include <climits>
#include <unordered_map>
#include "nlohmann/json.hpp"

namespace {

int horizonDays(const IHTC_Input& in) {
    return in.D > 0 ? in.D : 1;
}

std::vector<int> sortPatientsByPriority(const IHTC_Input& in) {
    std::vector<int> p_ids(in.patients.size());
    for (int i = 0; i < (int)in.patients.size(); ++i) p_ids[i] = i;

    std::sort(p_ids.begin(), p_ids.end(), [&](int a, int b) {
        const Patient& pa = in.patients[a];
        const Patient& pb = in.patients[b];

        if (pa.mandatory != pb.mandatory) return pa.mandatory > pb.mandatory;

        if (pa.mandatory) {
            if (pa.due_date != pb.due_date) return pa.due_date < pb.due_date;
            if (pa.release_date != pb.release_date) return pa.release_date < pb.release_date;
            int window_a = pa.due_date - pa.release_date;
            int window_b = pb.due_date - pb.release_date;
            if (window_a != window_b) return window_a < window_b;
        }

        if (pa.length_of_stay != pb.length_of_stay) return pa.length_of_stay > pb.length_of_stay;
        return pa.surgery_time > pb.surgery_time;
    });

    return p_ids;
}

double evaluatePlacementCost(const IHTC_Input& in, const IHTC_Output& out, int patient_id, int day, int room_id, int ot_id) {
    double cost = 0.0;
    const Patient& p = in.patients[patient_id];

    auto w = [&](const std::string& key, double fallback) -> double {
        auto it = in.weights.find(key);
        if (it != in.weights.end()) return static_cast<double>(it->second);
        return fallback;
    };

    double wS7 = w("S7", 5.0);
    cost += (day - p.release_date) * wS7;

    double wS5 = w("S5", 20.0);
    if (ot_id >= 0 && out.getOtMinutesUsed(ot_id, day) == 0) {
        cost += wS5;
    }

    double wS1 = w("S1", 1.0);
    int occ = out.getRoomOccupancy(room_id, day);
    cost += occ * wS1;

    double wSx = w("Sx", 0.1);
    cost += p.surgery_time * wSx;

    return cost;
}

bool schedulePatient(const IHTC_Input& in, IHTC_Output& out, int patient_id) {
    const Patient& p = in.patients[patient_id];

    int best_day = -1;
    int best_room = -1;
    int best_ot = -1;
    double min_cost = std::numeric_limits<double>::max();

    int start_d = std::max(0, p.release_date);
    int end_d = (in.D > 0) ? (in.D - 1) : start_d;
    if (p.mandatory) end_d = std::min(end_d, p.due_date);
    if (end_d < start_d) return false;

    bool needs_ot = p.surgery_time > 0;
    if (needs_ot && in.ots.empty()) return false;

    for (int d = start_d; d <= end_d; ++d) {
        for (int r = 0; r < (int)in.rooms.size(); ++r) {
            int ot_start = needs_ot ? 0 : -1;
            int ot_end = needs_ot ? (int)in.ots.size() - 1 : -1;
            for (int ot = ot_start; ot <= ot_end; ++ot) {
                if (!out.canAssignPatient(patient_id, d, r, ot, in)) continue;

                double current_cost = evaluatePlacementCost(in, out, patient_id, d, r, ot);
                if (current_cost < min_cost) {
                    min_cost = current_cost;
                    best_day = d;
                    best_room = r;
                    best_ot = ot;
                    if (min_cost <= 0.0) break;
                }
            }
            if (min_cost <= 0.0) break;
        }
        if (min_cost <= 0.0) break;
    }

    if (best_day != -1) {
        out.assignPatient(patient_id, best_day, best_room, best_ot, in);
        return true;
    }
    return false;
}

int countFeasiblePlacements(const IHTC_Input& in, const IHTC_Output& out, int patient_id, int stop_after) {
    const Patient& p = in.patients[patient_id];

    int start_d = std::max(0, p.release_date);
    int end_d = (in.D > 0) ? (in.D - 1) : start_d;
    if (p.mandatory) end_d = std::min(end_d, p.due_date);
    if (end_d < start_d) return 0;

    bool needs_ot = p.surgery_time > 0;
    if (needs_ot && in.ots.empty()) return 0;

    int feasible = 0;
    for (int d = start_d; d <= end_d; ++d) {
        for (int r = 0; r < (int)in.rooms.size(); ++r) {
            int ot_start = needs_ot ? 0 : -1;
            int ot_end = needs_ot ? (int)in.ots.size() - 1 : -1;
            for (int ot = ot_start; ot <= ot_end; ++ot) {
                if (!out.canAssignPatient(patient_id, d, r, ot, in)) continue;
                feasible++;
                if (feasible > stop_after) return feasible;
            }
        }
    }
    return feasible;
}

void seedOccupants(const IHTC_Input& in, IHTC_Output& out) {
    std::unordered_map<std::string, int> room_idx;
    for (int r = 0; r < (int)in.rooms.size(); ++r) room_idx[in.rooms[r].id] = r;

    for (const auto& f : in.occupants) {
        auto it = room_idx.find(f.room_id);
        if (it == room_idx.end()) continue;
        out.seedOccupantStay(it->second, f.admission_day, f.length_of_stay, f.sex);
    }
}

} // namespace

namespace GreedySolver {

void solvePASandSCP(const IHTC_Input& in, IHTC_Output& out) {
    out.init(in.patients.size(), in.rooms.size(), in.ots.size(), horizonDays(in));
    seedOccupants(in, out);

    std::vector<int> order = sortPatientsByPriority(in);
    std::vector<int> base_rank(in.patients.size(), 0);
    for (int i = 0; i < (int)order.size(); ++i) base_rank[order[i]] = i;
    std::vector<bool> done(in.patients.size(), false);

    int admitted_count = 0;
    int mandatory_failed = 0;

    for (int step = 0; step < (int)in.patients.size(); ++step) {
        int chosen_pid = -1;
        bool chosen_mandatory = false;
        int chosen_feasible = INT_MAX;
        int chosen_rank = INT_MAX;

        for (int pid : order) {
            if (done[pid]) continue;
            bool is_mandatory = in.patients[pid].mandatory;
            int current_stop = (chosen_feasible == INT_MAX) ? INT_MAX : chosen_feasible;
            int feasible = countFeasiblePlacements(in, out, pid, current_stop);
            int rank = base_rank[pid];

            bool better = false;
            if (chosen_pid == -1) better = true;
            else if (is_mandatory != chosen_mandatory) better = is_mandatory;
            else if (feasible != chosen_feasible) better = (feasible < chosen_feasible);
            else if (rank < chosen_rank) better = true;

            if (better) {
                chosen_pid = pid;
                chosen_mandatory = is_mandatory;
                chosen_feasible = feasible;
                chosen_rank = rank;
            }
        }

        if (chosen_pid == -1) break;
        done[chosen_pid] = true;

        int patient_id = chosen_pid;
        bool ok = schedulePatient(in, out, patient_id);
        if (ok) admitted_count++;
        else if (in.patients[patient_id].mandatory) mandatory_failed++;
    }

    std::cout << "[SOLVER] Patients admitted: " << admitted_count << "/" << in.patients.size() << std::endl;
    if (mandatory_failed > 0) {
        std::cerr << "[WARNING] Mandatory patients not admitted: " << mandatory_failed << std::endl;
    }
}

void solveNRA(const IHTC_Input& in, IHTC_Output& out) {
    int days = in.D > 0 ? in.D : 1;
    int shifts = std::max(1, in.shifts_per_day);
    int room_count = (int)in.rooms.size();
    int nurse_count = (int)in.nurses.size();

    out.clearNurseAssignments();
    if (room_count == 0 || nurse_count == 0 || days <= 0) return;

    std::vector<std::vector<std::vector<bool>>> nurse_available(
        nurse_count,
        std::vector<std::vector<bool>>(days, std::vector<bool>(shifts, false))
    );
    // per-nurse per-day per-shift max load (0 = not set, use fallback)
    std::vector<std::vector<std::vector<int>>> nurse_shift_cap(
        nurse_count,
        std::vector<std::vector<int>>(days, std::vector<int>(shifts, 0))
    );

    for (int n = 0; n < nurse_count; ++n) {
        if (in.nurses[n].working_shifts.empty()) {
            // no availability info: assume available everywhere
            for (int d = 0; d < days; ++d)
                for (int s = 0; s < shifts; ++s) nurse_available[n][d][s] = true;
        } else {
            for (const auto& ws : in.nurses[n].working_shifts) {
                int d = ws.day, s = ws.shift;
                if (d >= 0 && d < days && s >= 0 && s < shifts) {
                    nurse_available[n][d][s] = true;
                    nurse_shift_cap[n][d][s] = ws.max_load;
                }
            }
        }
    }

    std::vector<std::vector<bool>> room_occupied(days, std::vector<bool>(room_count, false));
    std::vector<std::vector<std::vector<int>>> room_shift_load(days, std::vector<std::vector<int>>(shifts, std::vector<int>(room_count, 0)));
    std::vector<std::vector<std::vector<int>>> room_shift_skill(days, std::vector<std::vector<int>>(shifts, std::vector<int>(room_count, 0)));

    std::unordered_map<std::string, int> room_idx;
    for (int r = 0; r < room_count; ++r) room_idx[in.rooms[r].id] = r;

    for (const auto& f : in.occupants) {
        auto it = room_idx.find(f.room_id);
        if (it == room_idx.end()) continue;
        int ridx = it->second;
        int start = std::max(0, f.admission_day);
        int los = std::max(1, f.length_of_stay);
        for (int dd = 0; dd < los; ++dd) {
            int d = start + dd;
            if (d < 0 || d >= days) continue;
            room_occupied[d][ridx] = true;
            for (int s = 0; s < shifts; ++s) {
                int idx = dd * shifts + s;
                int load = 1;
                if (!f.nurse_load_per_shift.empty()) {
                    if (idx < (int)f.nurse_load_per_shift.size()) load = f.nurse_load_per_shift[idx];
                    else load = f.nurse_load_per_shift.back();
                }
                room_shift_load[d][s][ridx] += load;
                room_shift_skill[d][s][ridx] = std::max(room_shift_skill[d][s][ridx], f.min_nurse_level);
            }
        }
    }

    for (int pid = 0; pid < (int)in.patients.size(); ++pid) {
        if (!out.isAdmitted(pid)) continue;
        int ridx = out.getRoomAssignedIdx(pid);
        if (ridx < 0 || ridx >= room_count) continue;
        int ad = out.getAdmitDay(pid);
        int los = std::max(1, in.patients[pid].length_of_stay);
        for (int dd = 0; dd < los; ++dd) {
            int d = ad + dd;
            if (d < 0 || d >= days) continue;
            room_occupied[d][ridx] = true;
            for (int s = 0; s < shifts; ++s) {
                int idx = dd * shifts + s;
                int load = 1;
                if (!in.patients[pid].nurse_load_per_shift.empty()) {
                    if (idx < (int)in.patients[pid].nurse_load_per_shift.size()) load = in.patients[pid].nurse_load_per_shift[idx];
                    else load = in.patients[pid].nurse_load_per_shift.back();
                }
                room_shift_load[d][s][ridx] += load;
                room_shift_skill[d][s][ridx] = std::max(room_shift_skill[d][s][ridx], in.patients[pid].min_nurse_level);
            }
        }
    }

    std::vector<std::vector<std::vector<int>>> nurse_load(days, std::vector<std::vector<int>>(shifts, std::vector<int>(nurse_count, 0)));

    for (int d = 0; d < days; ++d) {
        for (int s = 0; s < shifts; ++s) {
            for (int r = 0; r < room_count; ++r) {
                if (!room_occupied[d][r]) continue;

                int demand = room_shift_load[d][s][r];
                int req_skill = room_shift_skill[d][s][r];

                int best_nurse = -1;
                long long best_score = std::numeric_limits<long long>::max();

                for (int n = 0; n < nurse_count; ++n) {
                    if (!nurse_available[n][d][s]) continue;

                    int cur = nurse_load[d][s][n];
                    int projected = cur + demand;
                    int cap = nurse_shift_cap[n][d][s] > 0 ? nurse_shift_cap[n][d][s] : 9999;
                    int overload = std::max(0, projected - cap);
                    int skill_gap = std::max(0, req_skill - in.nurses[n].level);

                    long long score = 0;
                    score += 1000000LL * skill_gap;
                    score += 10000LL * overload;
                    score += 10LL * projected;
                    score += n;

                    if (score < best_score) {
                        best_score = score;
                        best_nurse = n;
                    }
                }

                if (best_nurse >= 0) {
                    nurse_load[d][s][best_nurse] += demand;
                    out.addNurseAssignment(best_nurse, d, s, r);
                }
            }
        }
    }
}

void runFullSolver(const IHTC_Input& in, IHTC_Output& out) {
    std::cout << "[SOLVER] Starting greedy solver..." << std::endl;
    solvePASandSCP(in, out);
    solveNRA(in, out);
    std::cout << "[SOLVER] Greedy pass finished." << std::endl;
}

} // namespace GreedySolver
