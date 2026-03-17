#include "IHTC_Greedy.hh"

#include <algorithm>
#include <iostream>
#include <limits>
#include <climits>
#include <cassert>
#include <unordered_map>
#include "nlohmann/json.hpp"

namespace {

// Ordina i pazienti per priorità: prima i più urgenti, poi i più "pesanti"
// uso sort con un comparatore personalizzato basato sulle proprietà dei pazienti, basta confrontarne 2.
std::vector<int> sortPatientsByPriority(const IHTC_Input& in) {
    // Crea una lista di indici [0, 1, 2, ..., N-1], uno per paziente
    std::vector<int> p_ids(in.patients.size());
    for (int i = 0; i < (int)in.patients.size(); ++i) p_ids[i] = i;

    // Ordina gli indici confrontando le proprietà dei pazienti
    std::sort(p_ids.begin(), p_ids.end(), [&](int a, int b) {
        const Patient &pa = in.patients[a];
        const Patient &pb = in.patients[b];

        // I pazienti obbligatori vengono prima degli opzionali
        if (pa.mandatory != pb.mandatory) return pa.mandatory > pb.mandatory;

        // Tra obbligatori: prima chi ha la scadenza più vicina
        if (pa.mandatory) {
            if (pa.due_date != pb.due_date) return pa.due_date < pb.due_date;
            // A parità di scadenza, prima chi può entrare prima
            if (pa.release_date != pb.release_date) return pa.release_date < pb.release_date;
            // A parità, prima chi ha la finestra di ammissione più stretta
            int window_a = pa.due_date - pa.release_date;
            int window_b = pb.due_date - pb.release_date;
            if (window_a != window_b) return window_a < window_b;
        }

        // Tra pazienti con stessa urgenza: prima chi resta più a lungo
        if (pa.length_of_stay != pb.length_of_stay) return pa.length_of_stay > pb.length_of_stay;
        // Infine, prima chi ha un intervento più lungo
        return pa.surgery_time > pb.surgery_time;
    });

    return p_ids;
}

double evaluatePlacementCost(const IHTC_Input& in, const IHTC_Output& out, int patient_id, int day, int room_id, int ot_id) {
    double cost = 0.0;
    const Patient& p = in.patients[patient_id];

    // Costo per ritardo di ammissione rispetto alla release date
    cost += (day - p.release_date) * in.w_patient_delay;

    // Costo per apertura di una nuova sala operatoria in quel giorno
    if (ot_id >= 0) {
        int max_cap = (day < (int)in.ots[ot_id].availability.size()) ? in.ots[ot_id].availability[day] : 0;
        // Se la capacità residua è uguale a quella di default, l'OT è intonsa e va aperta
        if (out.getOtAvailability(ot_id, day) == max_cap) {
            cost += in.w_open_operating_theater;
        }
    }

    // Euristica: preferire stanze meno affollate (non è un peso ufficiale)
    int occ = out.getRoomOccupancy(room_id, day);
    cost += occ * 1.0;

    // Euristica: a parità penalizzare interventi lunghi per bilanciarli
    cost += p.surgery_time * 0.1;

    return cost;
}

// Cerca la migliore combinazione (giorno, stanza, sala operatoria) per un paziente
bool schedulePatient(const IHTC_Input& in, IHTC_Output& out, int patient_id) {
    const Patient& p = in.patients[patient_id];

    // Migliore piazzamento trovato finora
    int best_day = -1;
    int best_room = -1;
    int best_ot = -1;
    double min_cost = std::numeric_limits<double>::max();

    // Finestra temporale ammissibile per l'ammissione
    int start_d = p.release_date;
    int end_d = in.D - 1;
    if (p.mandatory) end_d = std::min(end_d, p.due_date);

    // Controlla se il paziente ha bisogno di un intervento chirurgico
    bool needs_ot = p.surgery_time > 0;
    if (needs_ot && in.ots.empty()) return false; // serve una OT ma non ce ne sono

    // Prova tutte le combinazioni giorno × stanza × sala operatoria
    for (int d = start_d; d <= end_d; ++d) {
        for (int r = 0; r < (int)in.rooms.size(); ++r) {
            // Se non serve OT, il ciclo gira una sola volta con ot = -1
            int ot_start = needs_ot ? 0 : -1;
            int ot_end = needs_ot ? (int)in.ots.size() - 1 : -1;
            for (int ot = ot_start; ot <= ot_end; ++ot) {
                // Verifica vincoli hard (compatibilità, capienza, ecc.)
                if (!out.canAssignPatient(patient_id, d, r, ot, in)) continue;

                // Valuta il costo di questo piazzamento
                double current_cost = evaluatePlacementCost(in, out, patient_id, d, r, ot);
                if (current_cost < min_cost) {
                    min_cost = current_cost;
                    best_day = d;
                    best_room = r;
                    best_ot = ot;
                    if (min_cost <= 0.0) break; // costo zero = ottimo, esci subito
                }
            }
            if (min_cost <= 0.0) break;
        }
        if (min_cost <= 0.0) break;
    }

    // Se trovato un piazzamento valido, assegna il paziente
    if (best_day != -1) {
        out.assignPatient(patient_id, best_day, best_room, best_ot, in);
        return true;
    }
    return false; // nessun piazzamento feasible trovato
}

// Conta quante combinazioni (giorno, stanza, sala operatoria) sono valide per un paziente.
// Si ferma in anticipo (early exit) se supera la soglia `stop_after` per risparmiare tempo.
int countFeasiblePlacements(const IHTC_Input& in, const IHTC_Output& out, int patient_id, int stop_after) {
    const Patient& p = in.patients[patient_id];

    // Finestra temporale
    int start_d = std::max(0, p.release_date);
    int end_d = in.D - 1;
    if (p.mandatory) end_d = std::min(end_d, p.due_date);
    if (end_d < start_d) return 0; // nessuna opzione possibile

    // Controllo sala operatoria
    bool needs_ot = p.surgery_time > 0;
    if (needs_ot && in.ots.empty()) return 0; // nessuna opzione se serve OT ma non ci sono

    int feasible = 0;
    
    // Prova tutte le combinazioni possibili
    for (int d = start_d; d <= end_d; ++d) {
        for (int r = 0; r < (int)in.rooms.size(); ++r) {
            int ot_start = needs_ot ? 0 : -1;
            int ot_end = needs_ot ? (int)in.ots.size() - 1 : -1;
            for (int ot = ot_start; ot <= ot_end; ++ot) {
                // Se i vincoli non sono rispettati, scarta l'opzione
                if (!out.canAssignPatient(patient_id, d, r, ot, in)) continue;
                
                // Opzione valida trovata!
                feasible++;
                
                // Se abbiamo già trovato abbastanza opzioni, smetti di cercare
                if (feasible > stop_after) return feasible;
            }
        }
    }
    return feasible;
}

void seedOccupants(const IHTC_Input& in, IHTC_Output& out) {
    for (const auto& f : in.occupants) {
        if (f.room_idx < 0 || f.room_idx >= (int)in.rooms.size()) continue;
        out.seedOccupantStay(f.room_idx, f.length_of_stay, f.sex);
    }
}

} // namespace

namespace GreedySolver {

void solvePASandSCP(const IHTC_Input& in, IHTC_Output& out) {
    out.init(in);
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

            // Valuta se il paziente corrente (pid) è "migliore" (più urgente di posizionare) 
            // rispetto al miglior paziente trovato finora in questo step (chosen_pid)
            bool better = false;
            
            // 1. Se non abbiamo ancora scelto nessuno, prendi il primo che capita
            if (chosen_pid == -1) better = true;
            
            // 2. Priorità assoluta: se i due pazienti hanno stato mandatory diverso, 
            // vince quello mandatory (is_mandatory == true). 
            // Se sono uguali (entrambi mandatory o entrambi optional), si passa al criterio successivo.
            else if (is_mandatory != chosen_mandatory) better = is_mandatory;
            
            // 3. Minimum Feasible Options (Tie-breaker n.1):
            // Tra due pazienti con lo stesso stato mandatory, diamo la precedenza 
            // a chi ha "meno opzioni" rimaste per l'inserimento. È più difficile da piazzare, 
            // quindi va inserito prima che lo spazio finisca.
            else if (feasible != chosen_feasible) better = (feasible < chosen_feasible);
            
            // 4. Ordine base (Tie-breaker n.2):
            // Se hanno lo stesso numero di opzioni rimaste (e lo stesso stato mandatory),
            // usiamo l'ordine calcolato da `sortPatientsByPriority` all'inizio (rank minore = priorità maggiore).
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
    int days = in.D;
    int shifts = in.shifts_per_day;
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

    for (const auto& o : in.occupants) {
        int ridx = o.room_idx;
        if (ridx < 0 || ridx >= room_count) continue;
        int los = std::max(1, o.length_of_stay);
        for (int d = 0; d < los && d < days; ++d) {
            room_occupied[d][ridx] = true;
            for (int s = 0; s < shifts; ++s) {
                int shift_idx = d * shifts + s;
                int load = 1;
                int req_skill = 0;
                if (!o.nurse_load_per_shift.empty()) {
                    if (shift_idx < (int)o.nurse_load_per_shift.size()) load = o.nurse_load_per_shift[shift_idx];
                    else load = o.nurse_load_per_shift.back();
                }
                if (!o.skill_level_required_per_shift.empty()) {
                    if (shift_idx < (int)o.skill_level_required_per_shift.size()) req_skill = o.skill_level_required_per_shift[shift_idx];
                    else req_skill = o.skill_level_required_per_shift.back();
                }
                room_shift_load[d][s][ridx] += load;
                room_shift_skill[d][s][ridx] = std::max(room_shift_skill[d][s][ridx], req_skill);
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
                int req_skill = 0;
                if (!in.patients[pid].nurse_load_per_shift.empty()) {
                    if (idx < (int)in.patients[pid].nurse_load_per_shift.size()) load = in.patients[pid].nurse_load_per_shift[idx];
                    else load = in.patients[pid].nurse_load_per_shift.back();
                }
                if (!in.patients[pid].skill_level_required_per_shift.empty()) {
                    if (idx < (int)in.patients[pid].skill_level_required_per_shift.size()) req_skill = in.patients[pid].skill_level_required_per_shift[idx];
                    else req_skill = in.patients[pid].skill_level_required_per_shift.back();
                }
                room_shift_load[d][s][ridx] += load;
                room_shift_skill[d][s][ridx] = std::max(room_shift_skill[d][s][ridx], req_skill);
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
