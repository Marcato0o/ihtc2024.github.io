#include "IHTC_Greedy.hh"

#include <algorithm>
#include <iostream>
#include <limits>

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

    auto w = [&](const std::string &key, double fallback) -> double {
        auto it = in.weights.find(key);
        if (it != in.weights.end()) return static_cast<double>(it->second);
        return fallback;
    };

    double wS7 = w("S7", 5.0);
    cost += (day - p.release_date) * wS7;

    double wS5 = w("S5", 20.0);
    if (ot_id >= 0) {
        if (out.getOtMinutesUsed(ot_id, day) == 0) cost += wS5;
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

void scheduleInOrder(const IHTC_Input& in, IHTC_Output& out, const std::vector<int>& order, int& admitted_count, int& mandatory_failed) {
    out.init(in.patients.size(), in.rooms.size(), in.ots.size(), horizonDays(in));

    admitted_count = 0;
    mandatory_failed = 0;
    for (int patient_id : order) {
        bool ok = schedulePatient(in, out, patient_id);
        if (ok) admitted_count++;
        else if (in.patients[patient_id].mandatory) mandatory_failed++;
    }
}

void assignNurses(const IHTC_Input& in, IHTC_Output& out) {
    int days = in.D > 0 ? in.D : 1;
    for (int d = 0; d < days; ++d) {
        for (int shift = 0; shift < in.shifts_per_day; ++shift) {
            std::vector<int> room_load(in.rooms.size(), 0);
            for (size_t pid = 0; pid < in.patients.size(); ++pid) {
                if (!out.admitted[pid]) continue;
                if (out.admit_day[pid] != d) continue;
                int ridx = out.room_assigned_idx[pid];
                if (ridx < 0 || ridx >= (int)room_load.size()) continue;
                if (!in.patients[pid].nurse_load_per_shift.empty()) {
                    int local = in.patients[pid].nurse_load_per_shift[shift % in.patients[pid].nurse_load_per_shift.size()];
                    room_load[ridx] += local;
                } else {
                    room_load[ridx] += 1;
                }
            }

            for (int r = 0; r < (int)in.rooms.size(); ++r) {
                if (room_load[r] <= 0) continue;
                for (int ni = 0; ni < (int)in.nurses.size(); ++ni) {
                    if (in.nurses[ni].max_load >= room_load[r]) {
                        break;
                    }
                }
            }
        }
    }
}

} // namespace

void GreedyIHTCSolver(const IHTC_Input& in, IHTC_Output& out)
{
    std::cout << "[SOLVER] Starting greedy solver..." << std::endl;

    std::vector<int> best_order = sortPatientsByPriority(in);
    int admitted_count = 0;
    int mandatory_failed = 0;
    scheduleInOrder(in, out, best_order, admitted_count, mandatory_failed);

    int best_admitted = admitted_count;
    int best_mandatory_failed = mandatory_failed;
    bool improved = true;
    while (improved) {
        improved = false;

        std::vector<int> unscheduled;
        for (int i = 0; i < (int)out.admitted.size(); ++i) {
            if (!out.admitted[i]) unscheduled.push_back(i);
        }

        for (int pid : unscheduled) {
            std::vector<int> trial_order = best_order;
            auto it = std::find(trial_order.begin(), trial_order.end(), pid);
            if (it == trial_order.end()) continue;

            trial_order.erase(it);
            size_t insert_pos = 0;
            if (!in.patients[pid].mandatory) {
                while (insert_pos < trial_order.size() && in.patients[trial_order[insert_pos]].mandatory) insert_pos++;
            }
            trial_order.insert(trial_order.begin() + static_cast<long long>(insert_pos), pid);

            int trial_admitted = 0;
            int trial_mandatory_failed = 0;
            scheduleInOrder(in, out, trial_order, trial_admitted, trial_mandatory_failed);

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

    scheduleInOrder(in, out, best_order, admitted_count, mandatory_failed);

    std::cout << "[SOLVER] Patients admitted: " << admitted_count << "/" << in.patients.size() << std::endl;
    if (mandatory_failed > 0) {
        std::cerr << "[WARNING] Mandatory patients not admitted: " << mandatory_failed << std::endl;
    }

    assignNurses(in, out);
    std::cout << "[SOLVER] Nurse assignment finished." << std::endl;
}
