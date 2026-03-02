// --- File: IHTC_Solver.cc ---
#include "IHTC_Solver.hh"
#include <algorithm>
#include <limits>
#include <iostream>

using namespace std;

IHTC_Solver::IHTC_Solver(const IHTC_Data& input_data, IHTC_Output& output_data)
    : in(input_data), out(output_data) {
    // initialize output containers based on input sizes
    out.init(in.patients.size(), in.rooms.size(), in.ots.size(), horizonDays());
}

int IHTC_Solver::horizonDays() const {
    return in.D > 0 ? in.D : 1;
}

void IHTC_Solver::scheduleInOrder(const std::vector<int>& order, int& admitted_count, int& mandatory_failed) {
    // Start from a clean output state for each full scheduling attempt.
    out.init(in.patients.size(), in.rooms.size(), in.ots.size(), horizonDays());

    admitted_count = 0;
    mandatory_failed = 0;
    for (int patient_id : order) {
        bool ok = schedulePatient(patient_id);
        if (ok) admitted_count++;
        else if (in.patients[patient_id].mandatory) mandatory_failed++;
    }
}

void IHTC_Solver::greedySolve() {
    // High-level algorithm:
    // 1) Sort patients by priority (mandatory/urgency/size)
    // 2) For each patient in order, try to find the cheapest valid placement
    //    (day, room, optional OT). If found, assign and update occupancy.
    // 3) After all patients are considered, perform a simple nurse assignment pass.

    cout << "[SOLVER] Starting greedy solver..." << endl;

    // Phase 1: compute patient order and run baseline greedy schedule.
    vector<int> best_order = sortPatientsByPriority();
    int admitted_count = 0;
    int mandatory_failed = 0;
    scheduleInOrder(best_order, admitted_count, mandatory_failed);

    // Phase 2: lightweight repair/improvement pass.
    // Strategy: for each currently unscheduled patient, move it earlier in the order
    // and rerun greedy; keep the change if it improves admitted count.
    int best_admitted = admitted_count;
    int best_mandatory_failed = mandatory_failed;
    bool improved = true;
    while (improved) {
        improved = false;

        vector<int> unscheduled;
        for (int i = 0; i < (int)out.admitted.size(); ++i) {
            if (!out.admitted[i]) unscheduled.push_back(i);
        }

        for (int pid : unscheduled) {
            vector<int> trial_order = best_order;
            auto it = std::find(trial_order.begin(), trial_order.end(), pid);
            if (it == trial_order.end()) continue;

            // Remove patient from current position and reinsert near the front.
            trial_order.erase(it);
            // Keep mandatory-first idea: place before first optional if possible.
            size_t insert_pos = 0;
            if (!in.patients[pid].mandatory) {
                while (insert_pos < trial_order.size() && in.patients[trial_order[insert_pos]].mandatory) insert_pos++;
            }
            trial_order.insert(trial_order.begin() + static_cast<long long>(insert_pos), pid);

            int trial_admitted = 0;
            int trial_mandatory_failed = 0;
            scheduleInOrder(trial_order, trial_admitted, trial_mandatory_failed);

            bool better = false;
            if (trial_admitted > best_admitted) better = true;
            else if (trial_admitted == best_admitted && trial_mandatory_failed < best_mandatory_failed) better = true;

            if (better) {
                best_order = std::move(trial_order);
                best_admitted = trial_admitted;
                best_mandatory_failed = trial_mandatory_failed;
                improved = true;
            }
        }
    }

    // Ensure output corresponds to the best order found.
    scheduleInOrder(best_order, admitted_count, mandatory_failed);

    cout << "[SOLVER] Patients admitted: " << admitted_count << "/" << in.patients.size() << endl;
    if (mandatory_failed > 0) {
        cerr << "[WARNING] Mandatory patients not admitted: " << mandatory_failed << endl;
    }

    // Phase 3: assign nurses (simple heuristic)
    assignNurses();
    cout << "[SOLVER] Nurse assignment finished." << endl;
}

std::vector<int> IHTC_Solver::sortPatientsByPriority() const {
    // Build index list
    vector<int> p_ids(in.patients.size());
    for (int i = 0; i < (int)in.patients.size(); ++i) p_ids[i] = i;

    // Heuristic ordering:
    // - mandatory patients first
    // - among mandatory, smaller time window (due - release) first
    // - longer stays and longer surgeries earlier (harder to fit)
    std::sort(p_ids.begin(), p_ids.end(), [&](int a, int b) {
        const Patient& pa = in.patients[a];
        const Patient& pb = in.patients[b];

        if (pa.mandatory != pb.mandatory) return pa.mandatory > pb.mandatory;

        if (pa.mandatory) {
            int window_a = pa.due_date - pa.release_date;
            int window_b = pb.due_date - pb.release_date;
            if (window_a != window_b) return window_a < window_b;
        }

        if (pa.length_of_stay != pb.length_of_stay) return pa.length_of_stay > pb.length_of_stay;
        return pa.surgery_time > pb.surgery_time;
    });

    return p_ids;
}

bool IHTC_Solver::schedulePatient(int patient_id) {
    // Try to place a single patient by exploring candidate days, rooms and OTs.
    const Patient& p = in.patients[patient_id];

    int best_day = -1;
    int best_room = -1;
    int best_ot = -1; // -1 means no OT assigned
    double min_cost = numeric_limits<double>::max();

    // define search window
    int start_d = std::max(0, p.release_date);
    int end_d = (in.D > 0) ? (in.D - 1) : start_d;
    if (p.mandatory) {
        // Mandatory patients must be scheduled no later than due_date.
        end_d = std::min(end_d, p.due_date);
    }
    if (end_d < start_d) return false;

    // Iterate over days, rooms, and OTs (including "no OT" option).
    // We keep this exhaustive local search but reduce wasted OT trials:
    // - If surgery time is 0, only test ot = -1
    // - If surgery time > 0 and OTs exist, only test real OTs
    // - If surgery time > 0 and no OT exists, patient cannot be scheduled
    bool needs_ot = p.surgery_time > 0;
    if (needs_ot && in.ots.empty()) return false;

    for (int d = start_d; d <= end_d; ++d) {
        for (int r = 0; r < (int)in.rooms.size(); ++r) {
            int ot_start = needs_ot ? 0 : -1;
            int ot_end = needs_ot ? (int)in.ots.size() - 1 : -1;
            for (int ot = ot_start; ot <= ot_end; ++ot) {
                // Check hard constraints quickly
                if (!out.canAssignPatient(patient_id, d, r, ot, in)) continue;

                // Compute soft-constraint cost for this placement
                double current_cost = evaluatePlacementCost(patient_id, d, r, ot);
                if (current_cost < min_cost) {
                    min_cost = current_cost;
                    best_day = d;
                    best_room = r;
                    best_ot = ot;
                    // best possible cost is 0: early stop
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

double IHTC_Solver::evaluatePlacementCost(int patient_id, int day, int room_id, int ot_id) const {
    // Simple, explainable cost function using weights provided in the instance.
    // The function returns a lower score for better placements.
    double cost = 0.0;
    const Patient& p = in.patients[patient_id];

    // helper to read weight keys, default to given fallback
    auto w = [&](const std::string &key, double fallback) -> double {
        auto it = in.weights.find(key);
        if (it != in.weights.end()) return static_cast<double>(it->second);
        return fallback;
    };

    // Penalize delayed admission: (day - release_date) * weight_S7
    double wS7 = w("S7", 5.0);
    cost += (day - p.release_date) * wS7;

    // Penalize opening a new OT at that day (prefer re-using open OTs)
    double wS5 = w("S5", 20.0);
    if (ot_id >= 0) {
        if (out.getOtMinutesUsed(ot_id, day) == 0) cost += wS5;
    }

    // Prefer rooms with lower occupancy (soft), weight S1
    double wS1 = w("S1", 1.0);
    int occ = out.getRoomOccupancy(room_id, day);
    cost += occ * wS1;

    // Prefer shorter surgery time earlier (small penalty for large surgery)
    double wSx = w("Sx", 0.1);
    cost += p.surgery_time * wSx;

    return cost;
}

void IHTC_Solver::assignNurses() {
    // Very simple nurse assignment heuristic:
    // For each day and shift, compute required nurse load per room and try to
    // assign nurses greedily by available capacity and skill level.

    int days = in.D > 0 ? in.D : 1;
    for (int d = 0; d < days; ++d) {
        for (int shift = 0; shift < in.shifts_per_day; ++shift) {
            // compute required load per room for this shift
            std::vector<int> room_load(in.rooms.size(), 0);
            for (size_t pid = 0; pid < in.patients.size(); ++pid) {
                if (!out.admitted[pid]) continue;
                if (out.admit_day[pid] != d) continue;
                int ridx = out.room_assigned_idx[pid];
                if (ridx < 0 || ridx >= (int)room_load.size()) continue;
                // patient nurse_load_per_shift may be shorter than shifts_per_day
                if (!in.patients[pid].nurse_load_per_shift.empty()) {
                    int local = in.patients[pid].nurse_load_per_shift[shift % in.patients[pid].nurse_load_per_shift.size()];
                    room_load[ridx] += local;
                } else {
                    // fallback: each patient costs 1 unit of nurse load
                    room_load[ridx] += 1;
                }
            }

            // simple greedy: for each room with positive load, pick a nurse with enough remaining capacity
            // We do not persist assignments in output yet; this is a placeholder for a real assignment structure.
            for (int r = 0; r < (int)in.rooms.size(); ++r) {
                if (room_load[r] <= 0) continue;
                // find any nurse with sufficient max_load
                for (int ni = 0; ni < (int)in.nurses.size(); ++ni) {
                    if (in.nurses[ni].max_load >= room_load[r]) {
                        // assign (conceptually) and reduce nurse availability
                        // In a full implementation we'd record this and reduce nurse max_load for subsequent rooms
                        break;
                    }
                }
            }
        }
    }
}