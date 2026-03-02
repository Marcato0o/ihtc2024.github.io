// --- File: IHTC_Solver.hh ---
#ifndef _IHTC_SOLVER_HH_
#define _IHTC_SOLVER_HH_

#include "IHTC_Data.hh"
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

    bool canAssignPatient(int patient_id, int day, int room_idx, int ot_idx, const IHTC_Data &in) const {
        if (room_idx < 0 || room_idx >= (int)room_occupancy.size()) return false;
        if (day < 0 || day >= (int)room_occupancy[0].size()) return false;
        const Patient &p = in.patients[patient_id];
        const Room &r = in.rooms[room_idx];
        // room capacity
        if (room_occupancy[room_idx][day] >= r.capacity) return false;
        // incompatible room
        for (auto &bad : p.incompatible_rooms) if (bad == r.id) return false;
        // OT capacity if ot_idx valid
        if (ot_idx >= 0 && ot_idx < (int)ot_minutes_used.size()) {
            int used = ot_minutes_used[ot_idx][day];
            if (used + p.surgery_time > in.ots[ot_idx].daily_capacity) return false;
        }
        return true;
    }

    void assignPatient(int patient_id, int day, int room_idx, int ot_idx, const IHTC_Data &in) {
        admitted[patient_id] = true;
        admit_day[patient_id] = day;
        room_assigned_idx[patient_id] = room_idx;
        ot_assigned_idx[patient_id] = ot_idx;
        // mark occupancy for the length of stay (naive: only mark admit day)
        room_occupancy[room_idx][day] += 1;
        if (ot_idx >=0 && ot_idx < (int)ot_minutes_used.size()) {
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

    // --- Fasi dell'Algoritmo ---
    
    // Fase 1: Ordina i pazienti per priorità (Urgenza e "Difficoltà di incastro")
    std::vector<int> sortPatientsByPriority() const;

    // Fase 2: Assegna Data, Stanza e Sala Operatoria (PAS + SCP)
    bool schedulePatient(int patient_id);

    // Fase 3: Assegna gli Infermieri ai turni delle stanze occupate (NRA)
    void assignNurses();

    // --- Funzioni di Valutazione (Euristiche Locali) ---
    
    // Calcola una stima del costo (Violazione Vincoli Soft) per un potenziale assegnamento
    double evaluatePlacementCost(int patient_id, int day, int room_id, int ot_id) const;
};

#endif