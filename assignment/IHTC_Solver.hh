// --- File: IHTC_Solver.hh ---
#ifndef _IHTC_SOLVER_HH_
#define _IHTC_SOLVER_HH_

#include "IHTC_Data.hh"
#include <vector>

class IHTC_Solver {
public:
    // Il Solver prende l'Input (read-only) e l'Output (da costruire)
    IHTC_Solver(const IHTC_Input& input_data, IHTC_Output& output_data);

    // Il metodo principale che lancia le fasi dell'algoritmo
    void greedySolve();

private:
    const IHTC_Input& in;
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