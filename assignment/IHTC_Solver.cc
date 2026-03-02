// --- File: IHTC_Solver.cc ---
#include "IHTC_Solver.hh"
#include <algorithm>
#include <limits>
#include <iostream>

using namespace std;

IHTC_Solver::IHTC_Solver(const IHTC_Data& input_data, IHTC_Output& output_data)
    : in(input_data), out(output_data) {
    // initialize output containers based on input sizes
    int days = in.D > 0 ? in.D : 1;
    out.init(in.patients.size(), in.rooms.size(), in.ots.size(), days);
}

void IHTC_Solver::greedySolve() {
    // High-level algorithm:
    // 1) Sort patients by priority (mandatory/urgency/size)
    // 2) For each patient in order, try to find the cheapest valid placement
    //    (day, room, optional OT). If found, assign and update occupancy.
    // 3) After all patients are considered, perform a simple nurse assignment pass.

    cout << "[SOLVER] Starting greedy solver..." << endl;

    // Phase 1: compute patient order
    vector<int> sorted_patients = sortPatientsByPriority();

    // Phase 2: schedule patients one-by-one
    int admitted_count = 0;
    int mandatory_failed = 0;

    for (int patient_id : sorted_patients) {
        bool ok = schedulePatient(patient_id);
        if (ok) admitted_count++;
        else if (in.patients[patient_id].mandatory) mandatory_failed++;
    }

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
    int start_d = p.release_date;
    int end_d = start_d;
    if (in.D > 0) end_d = in.D - 1;
    if (p.due_date > end_d) end_d = p.due_date; // ensure due_date is considered if present

    // Iterate over days, rooms, and OTs (including "no OT" option)
    for (int d = start_d; d <= end_d; ++d) {
        for (int r = 0; r < (int)in.rooms.size(); ++r) {
            // allow ot = -1 (no OT) and every real OT index
            int max_ot_loop = std::max(0, (int)in.ots.size());
            for (int ot_iter = -1; ot_iter < max_ot_loop; ++ot_iter) {
                int ot = ot_iter;
                // Check hard constraints quickly
                if (!out.canAssignPatient(patient_id, d, r, ot, in)) continue;

                // Compute soft-constraint cost for this placement
                double current_cost = evaluatePlacementCost(patient_id, d, r, ot);
                if (current_cost < min_cost) {
                    min_cost = current_cost;
                    best_day = d;
                    best_room = r;
                    best_ot = ot;
                }
            }
        }
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