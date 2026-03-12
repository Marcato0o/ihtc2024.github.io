#include "IHTC_Data.hh"
#include "json/io.hh"
#include <algorithm>
#include <iostream>
#include <set>
#include <unordered_map>

using namespace std;

IHTC_Input::IHTC_Input() = default;

IHTC_Input::IHTC_Input(const std::string &file_name) {
    loadInstance(file_name);
}

const std::string &IHTC_Input::getRawJsonText() const {
    return raw_json_text;
}

int IHTC_Input::getSurgeonIdx(const std::string& surgeon_id) const {
    for (size_t i = 0; i < surgeons.size(); ++i) {
        if (surgeons[i].id == surgeon_id) {
            return (int)i;
        }
    }
    return -1;
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

    room_gender.assign(in.rooms.size(), std::vector<std::string>(days, ""));
}

bool IHTC_Output::canAssignPatient(int patient_id, int day, int room_idx, int ot_idx, const IHTC_Input &in) const {
    // 0. Sicurezza: Evitiamo crash per indici fuori dal mondo
    if (room_idx < 0 || room_idx >= (int)room_occupancy.size()) return false;
    if (day < 0 || day >= (int)room_occupancy[0].size()) return false;
    
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
        if (d_idx < 0 || d_idx >= days) break; // Sfiora l'orizzonte (OK, l'ospedale non esplode)
        
        // C'è almeno un letto libero per la notte in esame?
        if (room_occupancy[room_idx][d_idx] >= r.capacity) return false;
        
        // Politica del sesso: in una stanza non ci possono stare sessi diversi
        if (!p.sex.empty()) {
            const std::string &g = room_gender[room_idx][d_idx];
            if (!g.empty() && g != p.sex) return false;
        }
    }

    // 3. Stanze vietate per questo paziente (es. un bambino non può stare in geriatria)
    for (const auto &bad : p.incompatible_rooms) {
        if (bad == r.id) return false;
    }

    // 4. Vincoli della Sala Operatoria (Solo se gli serve operarsi, ot_idx >= 0)
    if (ot_idx >= 0 && ot_idx < (int)ot_availability.size()) {
        // Controlliamo direttamente se la capacità residua per quel giorno basta
        if (day >= (int)ot_availability[ot_idx].size() || ot_availability[ot_idx][day] < p.surgery_time) {
            return false;
        }
    }

    // 5. Vincoli del Chirurgo (Ha un limite di ore di lavoro al giorno)
    if (!p.surgeon_id.empty()) {
        int surgeon_idx = in.getSurgeonIdx(p.surgeon_id);
        if (surgeon_idx >= 0 && surgeon_idx < (int)surgeon_availability.size()) {
            if (day >= (int)surgeon_availability[surgeon_idx].size() || surgeon_availability[surgeon_idx][day] < p.surgery_time) {
                return false;
            }
        }
    }

    // Se sopravvive a tutti i controlli, l'assegnazione è VALIDA
    return true;
}

void IHTC_Output::assignPatient(int patient_id, int day, int room_idx, int ot_idx, const IHTC_Input &in) {
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
            if (!in.patients[patient_id].sex.empty() && room_gender[room_idx][dd_idx].empty()) {
                room_gender[room_idx][dd_idx] = in.patients[patient_id].sex;
            }
        }
    }
    
    // 3. Aggiornamento Sala Operatoria
    // Se ha bisogno della sala operatoria, scaliamo i minuti dell'intervento dalla disponibilità residua
    if (ot_idx >= 0 && ot_idx < (int)ot_availability.size()) {
        if (day >= 0 && day < days) {
            ot_availability[ot_idx][day] -= in.patients[patient_id].surgery_time;
        }
    }

    // 4. Aggiornamento Chirurgo
    // Se il paziente richiede un chirurgo specifico, scaliamo i minuti dalla sua disponibilità giornaliera
    if (!in.patients[patient_id].surgeon_id.empty()) {
        int surgeon_idx = in.getSurgeonIdx(in.patients[patient_id].surgeon_id);
        if (surgeon_idx >= 0 && surgeon_idx < (int)surgeon_availability.size()) {
            if (day >= 0 && day < days) {
                surgeon_availability[surgeon_idx][day] -= in.patients[patient_id].surgery_time;
            }
        }
    }
}

void IHTC_Output::seedOccupantStay(int room_idx, int admission_day, int length_of_stay, const std::string &sex) {
    if (room_idx < 0 || room_idx >= (int)room_occupancy.size()) return;
    if (room_occupancy.empty() || room_occupancy[room_idx].empty()) return;
    int days = (int)room_occupancy[room_idx].size();
    int los = std::max(1, length_of_stay);
    int start = std::max(0, admission_day);
    for (int dd = 0; dd < los; ++dd) {
        int d = start + dd;
        if (d < 0 || d >= days) continue;
        room_occupancy[room_idx][d] += 1;
        if (!sex.empty() && room_gender[room_idx][d].empty()) room_gender[room_idx][d] = sex;
    }
}

void IHTC_Output::clearNurseAssignments() {
    nurse_assignments.clear();
}

void IHTC_Output::addNurseAssignment(int nurse_idx, int day, int shift, int room_idx) {
    nurse_assignments.push_back({nurse_idx, day, shift, room_idx});
}

bool IHTC_Output::isAdmitted(int patient_id) const {
    return patient_id >= 0 && patient_id < (int)admitted.size() && admitted[patient_id];
}

int IHTC_Output::getAdmitDay(int patient_id) const {
    if (patient_id < 0 || patient_id >= (int)admit_day.size()) return -1;
    return admit_day[patient_id];
}

int IHTC_Output::getRoomAssignedIdx(int patient_id) const {
    if (patient_id < 0 || patient_id >= (int)room_assigned_idx.size()) return -1;
    return room_assigned_idx[patient_id];
}

int IHTC_Output::getOtAssignedIdx(int patient_id) const {
    if (patient_id < 0 || patient_id >= (int)ot_assigned_idx.size()) return -1;
    return ot_assigned_idx[patient_id];
}

std::vector<std::tuple<int, int, int, int>> IHTC_Output::getNurseAssignmentTuples() const {
    std::vector<std::tuple<int, int, int, int>> tuples;
    tuples.reserve(nurse_assignments.size());
    for (const auto &na : nurse_assignments) {
        tuples.emplace_back(na.nurse_idx, na.day, na.shift, na.room_idx);
    }
    return tuples;
}

int IHTC_Output::getRoomOccupancy(int room_idx, int day) const {
    if (room_idx < 0 || room_idx >= (int)room_occupancy.size()) return 0;
    if (day < 0 || day >= (int)room_occupancy[0].size()) return 0;
    return room_occupancy[room_idx][day];
}

int IHTC_Output::getOtAvailability(int ot_idx, int day) const {
    if (ot_idx < 0 || ot_idx >= (int)ot_availability.size()) return 0;
    if (day < 0 || day >= (int)ot_availability[0].size()) return 0;
    return ot_availability[ot_idx][day];
}

namespace {

static std::string ageGroupKey(const Patient &p) {
    if (p.raw_json.contains("age_group") && !p.raw_json["age_group"].is_null()) {
        if (p.raw_json["age_group"].is_string()) return p.raw_json["age_group"].get<std::string>();
        if (p.raw_json["age_group"].is_number_integer()) return std::to_string(p.raw_json["age_group"].get<int>());
    }
    if (p.age_group >= 0) return std::to_string(p.age_group);
    return "unknown";
}

struct SoftCostContext {
    int days = 1;
    int shifts = 1;
    int total_delay = 0;
    int unscheduled_optional_count = 0;
    std::vector<std::set<std::string>> occupied_rooms_by_day;
    std::vector<std::set<std::string>> ot_by_day;
    std::vector<std::vector<std::vector<int>>> room_shift_load;
    std::vector<std::vector<std::vector<int>>> room_shift_skill;
    std::vector<std::vector<std::vector<int>>> patients_in_room_day;
    std::vector<int> nurse_level;
    std::vector<std::vector<int>> nurse_max_load_by_shift; // [nurse][day*shifts+shift]
    std::vector<std::vector<int>> nurse_load_by_shift;
    std::vector<std::vector<std::vector<std::vector<int>>>> room_shift_nurses;
};

static SoftCostContext buildSoftCostContext(const IHTC_Input &in, const IHTC_Output &out) {
    SoftCostContext ctx;
    ctx.days = in.D > 0 ? in.D : 1;
    ctx.shifts = std::max(1, in.shifts_per_day);

    ctx.occupied_rooms_by_day.assign(ctx.days, {});
    ctx.ot_by_day.assign(ctx.days, {});
    ctx.room_shift_load.assign(ctx.days, std::vector<std::vector<int>>(ctx.shifts, std::vector<int>(in.rooms.size(), 0)));
    ctx.room_shift_skill.assign(ctx.days, std::vector<std::vector<int>>(ctx.shifts, std::vector<int>(in.rooms.size(), 0)));
    ctx.patients_in_room_day.assign(in.rooms.size(), std::vector<std::vector<int>>(ctx.days));

    std::unordered_map<std::string, int> room_name_to_idx;
    for (int i = 0; i < (int)in.rooms.size(); ++i) room_name_to_idx[in.rooms[i].id] = i;

    for (const auto &f : in.occupants) {
        auto it = room_name_to_idx.find(f.room_id);
        if (it == room_name_to_idx.end()) continue;
        int ridx = it->second;
        int start = std::max(0, f.admission_day);
        int los = std::max(1, f.length_of_stay);
        for (int dd = 0; dd < los; ++dd) {
            int d = start + dd;
            if (d < 0 || d >= ctx.days) continue;
            ctx.occupied_rooms_by_day[d].insert(in.rooms[ridx].id);
            for (int sh = 0; sh < ctx.shifts; ++sh) {
                int idx = dd * ctx.shifts + sh;
                int load = 1;
                if (!f.nurse_load_per_shift.empty()) {
                    if (idx < (int)f.nurse_load_per_shift.size()) load = f.nurse_load_per_shift[idx];
                    else load = f.nurse_load_per_shift.back();
                }
                ctx.room_shift_load[d][sh][ridx] += load;
                ctx.room_shift_skill[d][sh][ridx] = std::max(ctx.room_shift_skill[d][sh][ridx], f.min_nurse_level);
            }
        }
    }

    for (size_t pid = 0; pid < in.patients.size(); ++pid) {
        bool is_admitted = out.isAdmitted((int)pid);
        if (!is_admitted) {
            if (!in.patients[pid].mandatory) ctx.unscheduled_optional_count++;
            continue;
        }

        int admit_day = out.getAdmitDay((int)pid);
        const auto &p = in.patients[pid];
        int los = std::max(1, p.length_of_stay);
        int room_idx = out.getRoomAssignedIdx((int)pid);
        if (room_idx >= 0 && room_idx < (int)in.rooms.size()) {
            for (int d = admit_day; d < admit_day + los && d < ctx.days; ++d) {
                if (d >= 0) {
                    ctx.occupied_rooms_by_day[d].insert(in.rooms[room_idx].id);
                    ctx.patients_in_room_day[room_idx][d].push_back((int)pid);
                }
            }

            for (int dd = 0; dd < los; ++dd) {
                int d = admit_day + dd;
                if (d < 0 || d >= ctx.days) continue;
                for (int sh = 0; sh < ctx.shifts; ++sh) {
                    int idx = dd * ctx.shifts + sh;
                    int load = 1;
                    if (!p.nurse_load_per_shift.empty()) {
                        if (idx < (int)p.nurse_load_per_shift.size()) load = p.nurse_load_per_shift[idx];
                        else load = p.nurse_load_per_shift.back();
                    }
                    ctx.room_shift_load[d][sh][room_idx] += load;
                    ctx.room_shift_skill[d][sh][room_idx] = std::max(ctx.room_shift_skill[d][sh][room_idx], p.min_nurse_level);
                }
            }
        }

        if (admit_day >= 0) ctx.total_delay += std::max(0, admit_day - p.release_date);

        int ot_idx = out.getOtAssignedIdx((int)pid);
        if (admit_day >= 0 && admit_day < ctx.days && ot_idx >= 0 && ot_idx < (int)in.ots.size()) {
            ctx.ot_by_day[admit_day].insert(in.ots[ot_idx].id);
        }
    }

    int nurse_count = (int)in.nurses.size();
    ctx.nurse_level.assign(nurse_count, 0);
    ctx.nurse_max_load_by_shift.assign(nurse_count, std::vector<int>(ctx.days * ctx.shifts, 9999));
    for (int i = 0; i < nurse_count; ++i) {
        ctx.nurse_level[i] = in.nurses[i].level;
        for (const auto& ws : in.nurses[i].working_shifts) {
            int idx = ws.day * ctx.shifts + ws.shift;
            if (idx >= 0 && idx < ctx.days * ctx.shifts)
                ctx.nurse_max_load_by_shift[i][idx] = ws.max_load;
        }
    }

    ctx.nurse_load_by_shift.assign(nurse_count, std::vector<int>(ctx.days * ctx.shifts, 0));
    ctx.room_shift_nurses.assign(ctx.days, std::vector<std::vector<std::vector<int>>>(ctx.shifts, std::vector<std::vector<int>>(in.rooms.size())));

    std::vector<std::tuple<int, int, int, int>> nurse_assignments = out.getNurseAssignmentTuples();
    for (const auto &na : nurse_assignments) {
        int n = std::get<0>(na);
        int d = std::get<1>(na);
        int sh = std::get<2>(na);
        int r = std::get<3>(na);
        if (n < 0 || n >= nurse_count) continue;
        if (d < 0 || d >= ctx.days) continue;
        if (sh < 0 || sh >= ctx.shifts) continue;
        if (r < 0 || r >= (int)in.rooms.size()) continue;
        ctx.room_shift_nurses[d][sh][r].push_back(n);
        ctx.nurse_load_by_shift[n][d * ctx.shifts + sh] += ctx.room_shift_load[d][sh][r];
    }

    return ctx;
}

} // namespace

IHTC_Output::CostBreakdown IHTC_Output::computeAllCosts() const {
    CostBreakdown cb;
    if (!bound_input) return cb;

    const IHTC_Input &in = *bound_input;
    
    // 1. ALLOCAZIONE UNICA DEL CONTESTO
    SoftCostContext ctx = buildSoftCostContext(in, *this);

    // 2. CALCOLO DEI COSTI
    
    // -- Room Mixed Age --
    int raw_age = 0;
    for (int r = 0; r < (int)in.rooms.size(); ++r) {
        for (int d = 0; d < ctx.days; ++d) {
            const auto &plist = ctx.patients_in_room_day[r][d];
            if (plist.size() < 2) continue;
            std::unordered_map<std::string, int> age_counts;
            for (int pid : plist) age_counts[ageGroupKey(in.patients[pid])]++;
            long long n = (long long)plist.size();
            long long total_pairs = (n * (n - 1)) / 2;
            long long same_pairs = 0;
            for (const auto &kv : age_counts) {
                long long c = kv.second;
                same_pairs += (c * (c - 1)) / 2;
            }
            raw_age += (int)std::max(0LL, total_pairs - same_pairs);
        }
    }
    cb.age_mix = raw_age * in.w_room_mixed_age;

    // -- Room Nurse Skill --
    int raw_skill = 0;
    for (int d = 0; d < ctx.days; ++d) {
        for (int sh = 0; sh < ctx.shifts; ++sh) {
            for (int r = 0; r < (int)in.rooms.size(); ++r) {
                int req = ctx.room_shift_skill[d][sh][r];
                if (req <= 0) continue;
                for (int nidx : ctx.room_shift_nurses[d][sh][r]) {
                    if (nidx >= 0 && nidx < (int)ctx.nurse_level.size() && ctx.nurse_level[nidx] < req) {
                        raw_skill += (req - ctx.nurse_level[nidx]);
                    }
                }
            }
        }
    }
    cb.skill = raw_skill * in.w_room_nurse_skill;

    // -- Continuity of Care --
    int raw_cont = 0;
    for (size_t pid = 0; pid < in.patients.size(); ++pid) {
        if (!(pid < admitted.size() && admitted[pid])) continue;
        int day0 = admit_day[pid];
        int ridx = room_assigned_idx[pid];
        if (ridx < 0 || ridx >= (int)in.rooms.size()) continue;
        int los = std::max(1, in.patients[pid].length_of_stay);
        std::set<int> seen_nurses;
        for (int dd = 0; dd < los; ++dd) {
            int d = day0 + dd;
            if (d < 0 || d >= ctx.days) continue;
            for (int sh = 0; sh < ctx.shifts; ++sh) {
                for (int nidx : ctx.room_shift_nurses[d][sh][ridx]) seen_nurses.insert(nidx);
            }
        }
        if (!seen_nurses.empty()) raw_cont += std::max(0, (int)seen_nurses.size() - 1);
    }
    cb.continuity = raw_cont * in.w_continuity_of_care;

    // -- Excessive Workload --
    int raw_excess = 0;
    for (int nidx = 0; nidx < (int)ctx.nurse_level.size(); ++nidx) {
        for (int t = 0; t < ctx.days * ctx.shifts; ++t) {
            int cap = (nidx < (int)ctx.nurse_max_load_by_shift.size()) ? ctx.nurse_max_load_by_shift[nidx][t] : 9999;
            int over = ctx.nurse_load_by_shift[nidx][t] - cap;
            if (over > 0) raw_excess += over;
        }
    }
    cb.excess = raw_excess * in.w_nurse_eccessive_workload;

    // -- Open Operating Theater --
    int raw_ot = 0;
    for (int d = 0; d < ctx.days; ++d) {
        if (!ctx.ot_by_day[d].empty()) raw_ot += (int)ctx.ot_by_day[d].size();
    }
    cb.open_ot = raw_ot * in.w_open_operating_theater;

    // -- Surgeon Transfer --
    int raw_surg = 0;
    std::unordered_map<std::string, std::vector<std::set<std::string>>> surgeon_ot_by_day;
    for (size_t pid = 0; pid < in.patients.size(); ++pid) {
        if (!(pid < admitted.size() && admitted[pid])) continue;
        int d = admit_day[pid];
        int ot_idx = ot_assigned_idx[pid];
        if (d < 0 || d >= ctx.days || ot_idx < 0 || ot_idx >= (int)in.ots.size()) continue;
        const std::string &sid = in.patients[pid].surgeon_id;
        if (sid.empty()) continue;
        if (!surgeon_ot_by_day.count(sid)) surgeon_ot_by_day[sid] = std::vector<std::set<std::string>>(ctx.days);
        surgeon_ot_by_day[sid][d].insert(in.ots[ot_idx].id);
    }
    for (const auto &kv : surgeon_ot_by_day) {
        for (int d = 0; d < ctx.days; ++d) {
            int sz = (int)kv.second[d].size();
            if (sz > 1) raw_surg += (sz - 1);
        }
    }
    cb.surgeon_transfer = raw_surg * in.w_surgeon_transfer;

    // -- Patient Delay --
    cb.delay = ctx.total_delay * in.w_patient_delay;

    // -- Unscheduled Optional --
    cb.unscheduled = ctx.unscheduled_optional_count * in.w_unscheduled_optional;

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
