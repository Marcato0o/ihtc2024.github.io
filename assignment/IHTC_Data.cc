#include "IHTC_Data.hh"
#include "json/io.hh"
#include <algorithm>
#include <iostream>
#include <set>
#include <unordered_map>
#include <cassert>

using namespace std;

IHTC_Input::IHTC_Input() = default;

IHTC_Input::IHTC_Input(const std::string &file_name) {
    loadInstance(file_name);
}

const std::string &IHTC_Input::getRawJsonText() const {
    return raw_json_text;
}

IHTC_Output::IHTC_Output(const IHTC_Input &in) {
    bound_input = &in;
    init(in);
}

void IHTC_Output::init(const IHTC_Input &in) {
    int days = in.D > 0 ? in.D : 1;
    admitted.assign(in.patients.size(), false);
    admit_day.assign(in.patients.size(), -1);
    room_assigned_idx.assign(in.patients.size(), -1);
    ot_assigned_idx.assign(in.patients.size(), -1);
    nurse_assignments.clear();
    room_occupancy.assign(in.rooms.size(), std::vector<int>(days, 0));
    
    // Inizializziamo ot_availability con le capacità massime fornite dall'input
    ot_availability.resize(in.ots.size());
    for (size_t i = 0; i < in.ots.size(); ++i) {
        ot_availability[i] = in.ots[i].availability; // copiamo l'array dal JSON
        ot_availability[i].resize(days, 0); // assicuriamo che la lunghezza copra tutti i giorni
    }

    // Inizializziamo surgeon_availability con le capacità massime fornite dall'input
    surgeon_availability.resize(in.surgeons.size());
    for (size_t i = 0; i < in.surgeons.size(); ++i) {
        surgeon_availability[i] = in.surgeons[i].max_surgery_time; // copiato dal JSON
        surgeon_availability[i].resize(days, 0); // assicuriamo che copra l'orizzonte
    }

    room_gender.assign(in.rooms.size(), std::vector<Gender>(days, Gender::NONE));
}

bool IHTC_Output::canAssignPatient(int patient_id, int day, int room_idx, int ot_idx, const IHTC_Input &in) const {
    // 0. Sicurezza: Asserzioni in dev, bypassate in release per velocità massima
    assert(room_idx >= 0 && room_idx < (int)room_occupancy.size());
    assert(day >= 0 && day < (int)room_occupancy[0].size());
    assert(patient_id >= 0 && patient_id < (int)in.patients.size());
    
    const Patient &p = in.patients[patient_id];
    const Room &r = in.rooms[room_idx];

    // 1. Finestre Temporali
    // Il paziente non può essere ammesso prima della sua "release date"
    if (day < p.release_date) return false;
    // Se è "mandatory", scade e NON PUÒ essere ammesso dopo la sua "due date"
    if (p.mandatory && day > p.due_date) return false;

    int los = std::max(1, p.length_of_stay);
    int days = in.D;

    // 2. Vincoli della Stanza (Capienza e Sesso)
    // Dobbiamo verificare che per TUTTI i giorni del ricovero ci stia.
    for (int dd = 0; dd < los; ++dd) {
        int d_idx = day + dd;
        if (d_idx >= days) break; // Sfiora l'orizzonte (OK, l'ospedale non esplode)
        
        // C'è almeno un letto libero per la notte in esame?
        if (room_occupancy[room_idx][d_idx] >= r.capacity) return false;
        
        // Politica del sesso: in una stanza non ci possono stare sessi diversi
        if (p.sex != Gender::NONE) {
            Gender g = room_gender[room_idx][d_idx];
            if (g != Gender::NONE && g != p.sex) return false;
        }
    }

    // 3. Stanze vietate per questo paziente (es. un bambino non può stare in geriatria)
    for (int bad_idx : p.incompatible_room_idxs) {
        if (bad_idx == room_idx) return false;
    }

    // 4. Vincoli della Sala Operatoria (Solo se gli serve operarsi, ot_idx >= 0)
    if (ot_idx >= 0) {
        // Ipotizzando ot_idx sensato se >= 0
        assert(ot_idx < (int)ot_availability.size());
        if (day >= (int)ot_availability[ot_idx].size() || ot_availability[ot_idx][day] < p.surgery_time) {
            return false;
        }
    }

    // 5. Vincoli del Chirurgo (Ha un limite di ore di lavoro al giorno)
    if (p.surgeon_idx >= 0) {
        assert(p.surgeon_idx < (int)surgeon_availability.size());
        if (day >= (int)surgeon_availability[p.surgeon_idx].size()
            || surgeon_availability[p.surgeon_idx][day] < p.surgery_time) {
            return false;
        }
    }

    // Se sopravvive a tutti i controlli, l'assegnazione è VALIDA
    return true;
}

void IHTC_Output::assignPatient(int patient_id, int day, int room_idx, int ot_idx, const IHTC_Input &in) {
    assert(patient_id >= 0 && patient_id < (int)in.patients.size());
    assert(room_idx >= 0 && room_idx < (int)room_occupancy.size());

    // 1. Dati base dell'assegnazione: Salviamo quando, dove e se è stato ammesso
    admitted[patient_id] = true;
    admit_day[patient_id] = day;
    room_assigned_idx[patient_id] = room_idx;
    ot_assigned_idx[patient_id] = ot_idx;
    
    int los = std::max(1, in.patients[patient_id].length_of_stay);
    int days = in.D; 
    
    // 2. Aggiornamento Stanza (Posti letto e Sesso)
    // Per ogni giorno di permanenza del paziente, occupiamo un posto letto
    for (int dd = 0; dd < los; ++dd) {
        int dd_idx = day + dd;
        if (dd_idx >= 0 && dd_idx < days) {
            room_occupancy[room_idx][dd_idx] += 1;
            // Se la stanza era vuota (senza genere), le assegniamo il sesso del paziente
            if (in.patients[patient_id].sex != Gender::NONE && room_gender[room_idx][dd_idx] == Gender::NONE) {
                room_gender[room_idx][dd_idx] = in.patients[patient_id].sex;
            }
        }
    }
    
    // 3. Aggiornamento Sala Operatoria
    // Se ha bisogno della sala operatoria, scaliamo i minuti dell'intervento dalla disponibilità residua
    if (ot_idx >= 0) {
        assert(ot_idx < (int)ot_availability.size());
        if (day >= 0 && day < days) {
            ot_availability[ot_idx][day] -= in.patients[patient_id].surgery_time;
        }
    }

    // 4. Aggiornamento Chirurgo
    // Se il paziente richiede un chirurgo specifico, scaliamo i minuti dalla sua disponibilità giornaliera
    if (in.patients[patient_id].surgeon_idx >= 0) {
        int surgeon_idx = in.patients[patient_id].surgeon_idx;
        assert(surgeon_idx < (int)surgeon_availability.size());
        if (day >= 0 && day < days) {
            surgeon_availability[surgeon_idx][day] -= in.patients[patient_id].surgery_time;
        }
    }
}

void IHTC_Output::seedOccupantStay(int room_idx, int length_of_stay, Gender sex) {
    // Controllo di sicurezza: verifichiamo che l'indice della stanza sia valido
    assert(room_idx >= 0 && room_idx < (int)room_occupancy.size());
    assert(!room_occupancy.empty() && !room_occupancy[room_idx].empty());
    
    // Recuperiamo il numero totale di giorni disponibili nella struttura dati
    int days = (int)room_occupancy[room_idx].size();
    
    // Assicuriamoci che la durata del soggiorno sia valida (almeno 1 giorno)
    int los = std::max(1, length_of_stay);
    
    // Iteriamo per ogni giorno di permanenza del paziente nella stanza (partendo obbligatoriamente dal day 0)
    for (int d = 0; d < los; ++d) {
        
        // Se il giorno va oltre l'orizzonte temporale pianificato, lo ignoriamo
        if (d >= days) continue;
        
        // Occupiamo un posto letto in quella stanza per quel giorno
        room_occupancy[room_idx][d] += 1;
        
        // Se il paziente ha un genere definito e la stanza in quel giorno era ancora vuota (o "senza genere"),
        // assegniamo il genere del paziente alla stanza, vincolando così i futuri inserimenti.
        if (sex != Gender::NONE && room_gender[room_idx][d] == Gender::NONE) {
            room_gender[room_idx][d] = sex;
        }
    }
}

void IHTC_Output::clearNurseAssignments() {
    nurse_assignments.clear();
}

void IHTC_Output::addNurseAssignment(int nurse_idx, int day, int shift, int room_idx) {
    for (NurseAssignment &na : nurse_assignments) {
        if (na.day == day && na.shift == shift && na.room_idx == room_idx) {
            na.nurse_idx = nurse_idx;
            return;
        }
    }
    nurse_assignments.push_back({nurse_idx, day, shift, room_idx});
}

bool IHTC_Output::isAdmitted(int patient_id) const {
    assert(patient_id >= 0 && patient_id < (int)admitted.size());
    return admitted[patient_id];
}

int IHTC_Output::getAdmitDay(int patient_id) const {
    assert(patient_id >= 0 && patient_id < (int)admit_day.size());
    return admit_day[patient_id];
}

int IHTC_Output::getRoomAssignedIdx(int patient_id) const {
    assert(patient_id >= 0 && patient_id < (int)room_assigned_idx.size());
    return room_assigned_idx[patient_id];
}

int IHTC_Output::getOtAssignedIdx(int patient_id) const {
    assert(patient_id >= 0 && patient_id < (int)ot_assigned_idx.size());
    return ot_assigned_idx[patient_id];
}

std::vector<std::tuple<int, int, int, int>> IHTC_Output::getNurseAssignmentTuples() const {
    // Creiamo un vettore per memorizzare il risultato
    std::vector<std::tuple<int, int, int, int>> tuples;
    
    // Riserviamo memoria in anticipo per questioni di performance
    tuples.reserve(nurse_assignments.size());
    
    // Iteriamo su tutte le assegnazioni interne salvate e le convertiamo in tuple 
    for (const auto &na : nurse_assignments) {
        // Ogni elemento della tupla rappresenta: (indice_infermiere, giorno, turno, indice_stanza)
        tuples.emplace_back(na.nurse_idx, na.day, na.shift, na.room_idx);
    }
    
    return tuples;
}

int IHTC_Output::getRoomOccupancy(int room_idx, int day) const {
    assert(room_idx >= 0 && room_idx < (int)room_occupancy.size());
    assert(day >= 0 && day < (int)room_occupancy[0].size());
    return room_occupancy[room_idx][day];
}

int IHTC_Output::getOtAvailability(int ot_idx, int day) const {
    assert(ot_idx >= 0 && ot_idx < (int)ot_availability.size());
    assert(day >= 0 && day < (int)ot_availability[0].size());
    return ot_availability[ot_idx][day];
}

IHTC_Output::CostBreakdown IHTC_Output::computeAllCosts() const {
    CostBreakdown cb;
    if (!bound_input) return cb;

    const IHTC_Input &in = *bound_input;
    
    int days = in.D;
    int shifts = in.shifts_per_day;
    int num_rooms = (int)in.rooms.size();
    int num_nurses = (int)in.nurses.size();
    int num_ots = (int)in.ots.size();

    // Mappatura 3D -> 1D: Indice lineare per Giorno (d), Turno (sh), Stanza (r)
    auto dsr_idx = [&](int d, int sh, int r) { return (d * shifts + sh) * num_rooms + r; };
    
    // Mappatura 2D -> 1D: Indice lineare per Stanza (r), Giorno (d)
    auto rd_idx = [&](int r, int d) { return r * days + d; };
    
    // Mappatura 2D -> 1D: Indice lineare per Infermiere (n), Turno globale (shift_idx)
    auto nsh_idx = [&](int n, int shift_idx) { return n * (days * shifts) + shift_idx; };

    int dsr_size = days * shifts * num_rooms;
    int rd_size = num_rooms * days;

    // --- 1. PREPARAZIONE DATI LOCALI (Inline) ---
    std::vector<int> room_shift_load(dsr_size, 0); 
    std::vector<int> room_shift_skill(dsr_size, 0); 

    std::vector<std::vector<int>> patients_in_room_day(rd_size, std::vector<int>()); 

    // Array 1D che mappa l'utilizzo di una sala operatoria: (Giorno * num_ots) + ot_idx
    std::vector<bool> ot_opened_day(days * num_ots, false);
    
    int total_delay = 0;
    int unscheduled_optional_count = 0;

    // 1A. Aggiungiamo il carico degli "Occupants" (Pazienti già presenti nella struttura all'inizio del planning)
    for (const Occupant &o : in.occupants) {
        
        int ridx = o.room_idx;        
        int los = o.length_of_stay; // È la degenza residua a partire dal Giorno 0
        
        // Iteriamo sui giorni di permanenza residui (partendo obbligatoriamente dal Giorno 0)
        for (int d = 0; d < los; ++d) {
            // Iteriamo su ogni singolo turno del giorno (es. Mattina, Pomeriggio, Notte)
            for (int sh = 0; sh < shifts; ++sh) {
                int shift_idx = d * shifts + sh; // Indice progressivo del turno
                int workload = 1;
                int req_skill = 0;
                // Carico di lavoro prodotto nel turno (unita di workload),
                // Usa l'ultimo valore disponibile se l'array e piu corto.
                if (!o.nurse_load_per_shift.empty()) {
                    assert(shift_idx >= 0 && shift_idx < (int)o.nurse_load_per_shift.size());
                    workload = o.nurse_load_per_shift[shift_idx];
                }
                if (!o.skill_level_required_per_shift.empty()) {
                    assert(shift_idx >= 0 && shift_idx < (int)o.skill_level_required_per_shift.size());
                    req_skill = o.skill_level_required_per_shift[shift_idx];
                }
                
                // Calcoliamo l'indice lineare 1D e aggiorniamo il fabbisogno per quella stanza/turno
                int dsr = dsr_idx(d, sh, ridx);
                room_shift_load[dsr] += workload;
                room_shift_skill[dsr] = std::max(room_shift_skill[dsr], req_skill);
            }
        }
    }

    // Aggiungiamo il carico dei Nuovi Pazienti
    for (size_t pid = 0; pid < in.patients.size(); ++pid) {
        if (!isAdmitted((int)pid)) {
            // Gli optional non ammessi contribuiscono solo al costo unscheduled.
            if (!in.patients[pid].mandatory) unscheduled_optional_count++;
            continue;
        }

        int admit = admit_day[pid];
        int los = in.patients[pid].length_of_stay;
        int room_idx = room_assigned_idx[pid];
        
        if (room_idx >= 0 && room_idx < num_rooms) {
            // Traccia i pazienti presenti in stanza/giorno (serve per il costo age_mix).
            for (int d = admit; d < admit + los && d < days; ++d) {
                if (d >= 0) patients_in_room_day[rd_idx(room_idx, d)].push_back((int)pid);
            }
            // Costruisce domanda infermieristica e skill richiesta per ogni turno del ricovero.
            for (int dd = 0; dd < los; ++dd) {
                int d = admit + dd;
                if (d < 0 || d >= days) continue;
                for (int sh = 0; sh < shifts; ++sh) {
                    int idx = dd * shifts + sh;
                    int workload = 1;
                    int req_skill = 0;
                    if (!in.patients[pid].nurse_load_per_shift.empty()) {
                        assert(idx >= 0 && idx < (int)in.patients[pid].nurse_load_per_shift.size());
                        workload = in.patients[pid].nurse_load_per_shift[idx];
                    }
                    if (!in.patients[pid].skill_level_required_per_shift.empty()) {
                        assert(idx >= 0 && idx < (int)in.patients[pid].skill_level_required_per_shift.size());
                        req_skill = in.patients[pid].skill_level_required_per_shift[idx];
                    }
                    int dsr = dsr_idx(d, sh, room_idx);
                    room_shift_load[dsr] += workload;
                    // Se piu pazienti condividono stanza/turno, vale il requisito piu alto.
                    room_shift_skill[dsr] = std::max(room_shift_skill[dsr], req_skill);
                }
            }
        }

        // Delay soft: giorni di attesa oltre la release date.
        if (admit >= 0) total_delay += std::max(0, admit - in.patients[pid].release_date);

        int ot_idx = ot_assigned_idx[pid];
        if (admit >= 0 && admit < days && ot_idx >= 0 && ot_idx < num_ots) {
            // Marca OT aperta nel giorno di ammissione/intervento.
            ot_opened_day[admit * num_ots + ot_idx] = true;
        }
    }

    // Limiti Infermieri
    int nsh_size = num_nurses * days * shifts;
    std::vector<int> nurse_level(num_nurses, 0);
    std::vector<int> nurse_max_load_by_shift(nsh_size, 9999);
    for (int i = 0; i < num_nurses; ++i) {
        nurse_level[i] = in.nurses[i].level;
        for (const auto& ws : in.nurses[i].working_shifts) {
            int shift_loc = ws.day * shifts + ws.shift;
            if (shift_loc >= 0 && shift_loc < days * shifts)
                nurse_max_load_by_shift[nsh_idx(i, shift_loc)] = ws.max_load;
        }
    }

    // Assegnazioni infermieri
    std::vector<int> nurse_load_by_shift(nsh_size, 0);
    std::vector<int> room_shift_nurse(dsr_size, -1);
    
    for (const auto &na : nurse_assignments) {
        if (na.nurse_idx < 0 || na.nurse_idx >= num_nurses) continue;
        if (na.day < 0 || na.day >= days || na.shift < 0 || na.shift >= shifts) continue;
        if (na.room_idx < 0 || na.room_idx >= num_rooms) continue;
        
        int dsr = dsr_idx(na.day, na.shift, na.room_idx);
        room_shift_nurse[dsr] = na.nurse_idx;
        nurse_load_by_shift[nsh_idx(na.nurse_idx, na.day * shifts + na.shift)] += room_shift_load[dsr];
    }

    // --- 2. CALCOLO DEI COSTI ---
    
    // -- Room Mixed Age --
    // Calcola la penalità per la presenza di pazienti di età diversa nella stessa stanza e giorno.
    // Per ogni stanza e giorno:
    //   - Se ci sono almeno 2 pazienti, si contano tutte le coppie possibili.
    //   - Si calcolano le coppie di pazienti che appartengono allo stesso gruppo di età.
    //   - La penalità è proporzionale al numero di coppie di età diversa (tutte le coppie meno quelle dello stesso gruppo).
    int raw_age = 0;
    for (int r = 0; r < num_rooms; ++r) {
        for (int d = 0; d < days; ++d) {
            const auto &plist = patients_in_room_day[rd_idx(r, d)]; // Lista pazienti in stanza r al giorno d
            if (plist.size() < 2) continue; // Nessuna penalità se c'è 0 o 1 paziente
            std::unordered_map<int, int> age_counts; // Conta quanti pazienti per ogni gruppo di età
            for (int pid : plist)
                age_counts[in.patients[pid].age_group]++;
            long long n = (long long)plist.size();
            long long total_pairs = (n * (n - 1)) / 2; // Numero totale di coppie possibili
            long long same_pairs = 0; // Coppie con stesso gruppo di età
            for (const auto &kv : age_counts) {
                // Per ogni gruppo di età, calcola le coppie interne (combinazioni di 2 tra quelli dello stesso gruppo)
                same_pairs += ((long long)kv.second * (kv.second - 1)) / 2;
            }
            // Penalità: tutte le coppie meno quelle dello stesso gruppo
            raw_age += (int)std::max(0LL, total_pairs - same_pairs);
        }
    }
    // Applica il peso del problema
    cb.age_mix = raw_age * in.w_room_mixed_age;

    // -- Room Nurse Skill --
    int raw_skill = 0;
    for (int d = 0; d < days; ++d) {
        for (int sh = 0; sh < shifts; ++sh) {
            for (int r = 0; r < num_rooms; ++r) {
                int dsr = dsr_idx(d, sh, r);
                int req = room_shift_skill[dsr];
                if (req <= 0) continue;
                int nidx = room_shift_nurse[dsr];
                if (nidx >= 0 && nidx < num_nurses && nurse_level[nidx] < req) {
                    raw_skill += (req - nurse_level[nidx]);
                }
            }
        }
    }
    cb.skill = raw_skill * in.w_room_nurse_skill;

    // -- Continuity of Care --
    int raw_cont = 0;
    for (size_t pid = 0; pid < in.patients.size(); ++pid) {
        if (!isAdmitted((int)pid)) continue;
        int day0 = admit_day[pid];
        int ridx = room_assigned_idx[pid];
        if (ridx < 0 || ridx >= num_rooms) continue;
        int los = std::max(1, in.patients[pid].length_of_stay);
        std::set<int> seen_nurses;
        for (int dd = 0; dd < los; ++dd) {
            int d = day0 + dd;
            if (d < 0 || d >= days) continue;
            for (int sh = 0; sh < shifts; ++sh) {
                int nidx = room_shift_nurse[dsr_idx(d, sh, ridx)];
                if (nidx >= 0) seen_nurses.insert(nidx);
            }
        }
        if (!seen_nurses.empty()) raw_cont += std::max(0, (int)seen_nurses.size() - 1);
    }
    cb.continuity = raw_cont * in.w_continuity_of_care;

    // -- Excessive Workload --
    int raw_excess = 0;
    for (int nidx = 0; nidx < num_nurses; ++nidx) {
        for (int t = 0; t < days * shifts; ++t) {
            int nsh = nsh_idx(nidx, t);
            int over = nurse_load_by_shift[nsh] - nurse_max_load_by_shift[nsh];
            if (over > 0) raw_excess += over;
        }
    }
    cb.excess = raw_excess * in.w_nurse_eccessive_workload;

    // -- Open Operating Theater --
    int raw_ot = 0;
    for (int d = 0; d < days; ++d) {
        for (int o = 0; o < num_ots; ++o) {
            if (ot_opened_day[d * num_ots + o]) {
                raw_ot++;
            }
        }
    }
    cb.open_ot = raw_ot * in.w_open_operating_theater;

    // -- Surgeon Transfer --
    int raw_surg = 0;
    std::unordered_map<int, std::vector<std::set<int>>> surgeon_ot_by_day;
    for (size_t pid = 0; pid < in.patients.size(); ++pid) {
        if (!isAdmitted((int)pid)) continue;
        int d = admit_day[pid];
        int ot_idx = ot_assigned_idx[pid];
        if (d < 0 || d >= days || ot_idx < 0 || ot_idx >= (int)in.ots.size()) continue;
        int surgeon_idx = in.patients[pid].surgeon_idx;
        if (surgeon_idx < 0) continue;
        if (!surgeon_ot_by_day.count(surgeon_idx)) surgeon_ot_by_day[surgeon_idx] = std::vector<std::set<int>>(days);
        surgeon_ot_by_day[surgeon_idx][d].insert(ot_idx);
    }
    for (const auto &kv : surgeon_ot_by_day) {
        for (int d = 0; d < days; ++d) {
            int sz = (int)kv.second[d].size();
            if (sz > 1) raw_surg += (sz - 1);
        }
    }
    cb.surgeon_transfer = raw_surg * in.w_surgeon_transfer;

    // -- Patient Delay --
    cb.delay = total_delay * in.w_patient_delay;

    // -- Unscheduled Optional --
    cb.unscheduled = unscheduled_optional_count * in.w_unscheduled_optional;

    // 3. CALCOLO TOTALE
    cb.total = cb.age_mix + cb.skill + cb.continuity + cb.excess + 
               cb.open_ot + cb.surgeon_transfer + cb.delay + cb.unscheduled;

    return cb;
}

void IHTC_Output::printCosts() const {
    CostBreakdown cb = computeAllCosts();

    std::cout << "Cost: " << cb.total
              << ", Unscheduled: " << cb.unscheduled
              << ",  Delay: " << cb.delay
              << ",  OpenOT: " << cb.open_ot
              << ",  AgeMix: " << cb.age_mix
              << ",  Skill: " << cb.skill
              << ",  Excess: " << cb.excess
              << ",  Continuity: " << cb.continuity
              << ",  SurgeonTransfer: " << cb.surgeon_transfer
              << std::endl;
}

void IHTC_Output::writeJSON(const std::string& filename) const {
    if (!bound_input) {
        std::cerr << "writeJSON: input is not bound to output state." << std::endl;
        return;
    }
    jsonio::write_solution(*bound_input, *this, filename);
}

bool IHTC_Input::loadInstance(const std::string &path) {
    return jsonio::load_instance(*this, path);
}
