// --- File: IHTC_Solver.cc ---
#include "IHTC_Solver.hh"
#include <algorithm>
#include <limits>
#include <iostream>

using namespace std;

IHTC_Solver::IHTC_Solver(const IHTC_Input& input_data, IHTC_Output& output_data)
    : in(input_data), out(output_data) {}

void IHTC_Solver::greedySolve() {
    cout << "[SOLVER] Avvio Algoritmo Greedy..." << endl;

    // FASE 1: Ordinamento Strategico
    vector<int> sorted_patients = sortPatientsByPriority();

    // FASE 2: Assegnamento Pazienti (Routing e Scheduling)
    int admitted_count = 0;
    int mandatory_failed = 0;

    for (int patient_id : sorted_patients) {
        if (schedulePatient(patient_id)) {
            admitted_count++;
        } else if (in.patients[patient_id].is_mandatory) {
            // GRAVE: Un paziente obbligatorio non ha trovato posto.
            // In un greedy puro questo è un "game over" per la validità (H5 violato).
            mandatory_failed++;
        }
    }
    cout << "[SOLVER] Pazienti ammessi: " << admitted_count << "/" << in.NumPatients() << endl;
    if (mandatory_failed > 0) {
        cerr << "[WARNING] Violato H5! " << mandatory_failed << " pazienti obbligatori non ammessi." << endl;
    }

    // FASE 3: Assegnamento Infermieri (NRA)
    assignNurses();
    cout << "[SOLVER] Assegnamento infermieri completato." << endl;
}

std::vector<int> IHTC_Solver::sortPatientsByPriority() const {
    vector<int> p_ids(in.NumPatients());
    for (int i = 0; i < in.NumPatients(); ++i) p_ids[i] = i;

    // Implementiamo un'euristica simile al "First-Fit Decreasing".
    // Diamo la priorità a chi è più "difficile" da inserire.
    std::sort(p_ids.begin(), p_ids.end(), [&](int a, int b) {
        const Patient& pa = in.patients[a];
        const Patient& pb = in.patients[b];

        // 1. Mandatory vince sempre su Optional
        if (pa.is_mandatory != pb.is_mandatory) {
            return pa.is_mandatory > pb.is_mandatory; 
        }

        // 2. Se entrambi Mandatory, ordiniamo per "Finestra di tempo utile" (Urgenza)
        // Finestra = due_day - release_day. Più è piccola, prima va schedulato.
        if (pa.is_mandatory) {
            int window_a = pa.due_day - pa.release_day;
            int window_b = pb.due_day - pb.release_day;
            if (window_a != window_b) return window_a < window_b;
        }

        // 3. A parità, scheduliamo prima chi occupa più risorse (lunga degenza o chirurgia lunga)
        if (pa.length_of_stay != pb.length_of_stay) {
            return pa.length_of_stay > pb.length_of_stay;
        }
        
        return pa.surgery_duration > pb.surgery_duration;
    });

    return p_ids;
}

bool IHTC_Solver::schedulePatient(int patient_id) {
    const Patient& p = in.patients[patient_id];
    
    int best_day = -1;
    int best_room = -1;
    int best_ot = -1;
    double min_cost = numeric_limits<double>::max(); // Infinito

    // Per un paziente opzionale l'orizzonte è l'intero periodo.
    // Per un mandatory è tra release_day e due_day.
    int start_d = p.release_day;
    int end_d = p.is_mandatory ? p.due_day : in.Days() - 1;

    // Ricerca Esaustiva Locale: Proviamo TUTTE le combinazioni possibili per questo singolo paziente
    for (int d = start_d; d <= end_d; ++d) {
        for (int r = 0; r < in.NumRooms(); ++r) {
            for (int ot = 0; ot < in.ots.size(); ++ot) {
                
                // Controllo Vincoli Hard (H1, H2, H3, H4, H6, H7) in O(1)
                if (out.canAssignPatient(patient_id, d, r, ot)) {
                    
                    // Se valido, calcoliamo i Vincoli Soft (S1, S5, S7, ecc.)
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
    }

    // Se abbiamo trovato almeno un posto valido, registriamo l'assegnamento
    if (best_day != -1) {
        out.assignPatient(patient_id, best_day, best_room, best_ot);
        return true;
    }

    return false; // Paziente non ammesso (Nessuna combinazione valida trovata)
}

double IHTC_Solver::evaluatePlacementCost(int patient_id, int day, int room_id, int ot_id) const {
    double cost = 0.0;
    const Patient& p = in.patients[patient_id];

    // Euristica per S7: Penalità per il ritardo di ammissione
    // Costo = (giorno_scelto - release_day) * Peso_S7
    cost += (day - p.release_day) * 5.0; // Sostituire 5.0 con il peso reale dal JSON

    // Euristica per S5: Minimizzare le Sale Operatorie aperte
    // Se la sala 'ot_id' in quel giorno è vuota, usarla comporta "aprirla" (Costo alto)
    // Se ha già minuti occupati, usarla costa zero (stiamo ottimizzando lo spazio)
    if (out.getOtMinutesUsed(ot_id, day) == 0) {
        cost += 20.0; // Penalità per apertura nuova sala (Peso_S5)
    }

    // Aggiungeremo le altre euristiche qui (es. S1 mix di età nella stanza)
    
    return cost;
}

void IHTC_Solver::assignNurses() {
    // Itera su tutti i giorni, su tutti e 3 i turni
    for (int d = 0; d < in.Days(); ++d) {
        for (int shift = 0; shift < 3; ++shift) {
            int global_shift_id = (d * 3) + shift;

            // Per ogni stanza
            for (int r = 0; r < in.NumRooms(); ++r) {
                // H8: Se la stanza è occupata in questo giorno, SERVE un infermiere
                if (out.getRoomOccupancy(r, d) > 0) {
                    
                    int best_nurse = -1;
                    // ... qui implementeremo la ricerca dell'infermiere migliore ...
                    // minimizzando S2 (skill) e S4 (carico di lavoro)
                    
                    // out.assignNurseToRoom(best_nurse, r, global_shift_id);
                }
            }
        }
    }
}