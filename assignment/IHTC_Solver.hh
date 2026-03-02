// --- File: IHTC_Solver.hh ---
#ifndef _IHTC_SOLVER_HH_
#define _IHTC_SOLVER_HH_

#include "IHTC_Data.hh"
#include <algorithm>
#include <vector>
#include <string>

// Minimal output container used by the greedy solver. It stores assignments
// and provides small helper queries used by the solver.
struct IHTC_Output {
    // per-patient assignments
    std::vector<bool> admitted; // per patient
    std::vector<int> admit_day; // -1 if not admitted
    std::vector<int> room_assigned_idx; // index into IHTC_Data::rooms
    std::vector<int> ot_assigned_idx; // index into IHTC_Data::ots

    // occupancy: rooms x days -> count
    std::vector<std::vector<int>> room_occupancy;
    // OT minutes used: ots x days -> minutes
    std::vector<std::vector<int>> ot_minutes_used;

    void init(size_t num_patients, size_t num_rooms, size_t num_ots, int days) {
        admitted.assign(num_patients, false);
        admit_day.assign(num_patients, -1);
        room_assigned_idx.assign(num_patients, -1);
        ot_assigned_idx.assign(num_patients, -1);
        room_occupancy.assign(num_rooms, std::vector<int>(days,0));
        ot_minutes_used.assign(num_ots, std::vector<int>(days,0));
    }

    // Check whether a patient can be assigned to `room_idx` on `day` and (optionally) `ot_idx`.
    // Enforces hard constraints such as room capacity, incompatible rooms and OT capacity.
    bool canAssignPatient(int patient_id, int day, int room_idx, int ot_idx, const IHTC_Data &in) const {
        if (room_idx < 0 || room_idx >= (int)room_occupancy.size()) return false;
        if (day < 0 || day >= (int)room_occupancy[0].size()) return false;
        const Patient &p = in.patients[patient_id];
        const Room &r = in.rooms[room_idx];
        // room capacity over the full length of stay
        int los = std::max(1, p.length_of_stay);
        int days = (int)room_occupancy[0].size();
        if (day + los > days) return false;
        for (int dd = 0; dd < los; ++dd) {
            if (room_occupancy[room_idx][day + dd] >= r.capacity) return false;
        }
        // incompatible room
        for (auto &bad : p.incompatible_rooms) if (bad == r.id) return false;
        // OT capacity if ot_idx valid
        if (ot_idx >= 0 && ot_idx < (int)ot_minutes_used.size()) {
            int used = ot_minutes_used[ot_idx][day];
            if (used + p.surgery_time > in.ots[ot_idx].daily_capacity) return false;
        }
        return true;
    }

    // Assign the patient and update occupancy/OT usage.
    // We conservatively mark occupancy for the admission day and the following
    // `length_of_stay - 1` days so room capacity is respected over the stay.
    void assignPatient(int patient_id, int day, int room_idx, int ot_idx, const IHTC_Data &in) {
        admitted[patient_id] = true;
        admit_day[patient_id] = day;
        room_assigned_idx[patient_id] = room_idx;
        ot_assigned_idx[patient_id] = ot_idx;
        // mark occupancy for the whole length of stay (bounded by horizon)
        int los = std::max(1, in.patients[patient_id].length_of_stay);
        int days = room_occupancy.empty() ? 0 : (int)room_occupancy[0].size();
        for (int dd = 0; dd < los; ++dd) {
            int dd_idx = day + dd;
            if (dd_idx >= 0 && dd_idx < days) room_occupancy[room_idx][dd_idx] += 1;
        }
        if (ot_idx >=0 && ot_idx < (int)ot_minutes_used.size()) {
            int days_ot = (int)ot_minutes_used[0].size();
            if (day >=0 && day < days_ot)
                ot_minutes_used[ot_idx][day] += in.patients[patient_id].surgery_time;
        }
    }

    int getRoomOccupancy(int room_idx, int day) const {
        if (room_idx < 0 || room_idx >= (int)room_occupancy.size()) return 0;
        if (day < 0 || day >= (int)room_occupancy[0].size()) return 0;
        return room_occupancy[room_idx][day];
    }

    int getOtMinutesUsed(int ot_idx, int day) const {
        if (ot_idx < 0 || ot_idx >= (int)ot_minutes_used.size()) return 0;
        if (day < 0 || day >= (int)ot_minutes_used[0].size()) return 0;
        return ot_minutes_used[ot_idx][day];
    }
};


class IHTC_Solver {
public:
    // The solver takes a const reference to input data and a modifiable output container
    IHTC_Solver(const IHTC_Data& input_data, IHTC_Output& output_data);

    // The main method that runs the algorithm
    void greedySolve();

private:
    const IHTC_Data& in;
    IHTC_Output& out;

    // --- Algorithm Phases ---
    
    // Phase 1: Sort patients by priority (urgency and "difficulty to fit")
    std::vector<int> sortPatientsByPriority() const;

    // Phase 2: Assign admission day, room and (optionally) operating theatre
    bool schedulePatient(int patient_id);

    // Phase 3: Assign nurses to shifts for occupied rooms
    void assignNurses();

    // --- Evaluation helpers (local heuristics) ---
    
    // Compute an estimated cost (soft-constraint violations) for a candidate placement
    double evaluatePlacementCost(int patient_id, int day, int room_id, int ot_id) const;
};

#endif