# TASK: Ottimizzazione del calcolo dei Soft Costs (Refactoring IHTC_Output)

**Ruolo:** Agisci come un Senior C++ Software Engineer. Il tuo obiettivo è eseguire un refactoring mirato sui file del progetto IHTP per risolvere un grave problema di performance (collo di bottiglia e memory allocation ridondante) legato alla funzione `buildSoftCostContext`.

**Contesto del problema:**
Attualmente, le funzioni `ComputeCost...()` in `IHTC_Data.cc` chiamano ciascuna `buildSoftCostContext()`. Quando si calcola il costo totale o si esporta il JSON, il contesto (una struttura dati pesante con matrici 3D) viene allocato, calcolato e distrutto fino a 16 volte consecutive.

**Obiettivo:**
Eliminare le 9 funzioni pubbliche individuali di calcolo dei costi e sostituirle con un'unica funzione `computeAllCosts()` che genera il `SoftCostContext` **una sola volta**, calcola tutti i pesi e restituisce una struct `CostBreakdown`.

Esegui i seguenti step in ordine:

### STEP 1: Modifica `IHTC_Data.hh`
1. Apri `IHTC_Data.hh`.
2. Trova la classe `IHTC_Output`.
3. **ELIMINA** le dichiarazioni di queste 9 funzioni:
   - `ComputeCostRoomMixedAge()`
   - `ComputeCostRoomNurseSkill()`
   - `ComputeCostContinuityOfCare()`
   - `ComputeCostNurseExcessiveWorkload()`
   - `ComputeCostOpenOperatingTheater()`
   - `ComputeCostSurgeonTransfer()`
   - `ComputeCostPatientDelay()`
   - `ComputeCostUnscheduledOptional()`
   - `ComputeCostTotal()`
4. **AGGIUNGI** al loro posto questa struct e il nuovo metodo pubblico:

```cpp
    struct CostBreakdown {
        int age_mix = 0;
        int skill = 0;
        int continuity = 0;
        int excess = 0;
        int open_ot = 0;
        int surgeon_transfer = 0;
        int delay = 0;
        int unscheduled = 0;
        int total = 0;
    };
    CostBreakdown computeAllCosts() const;
```

### STEP 2: Modifica `IHTC_Data.cc`
1. Apri `IHTC_Data.cc`.
2. **ELIMINA** le implementazioni delle 9 funzioni rimosse allo Step 1.
3. **AGGIUNGI** l'implementazione del nuovo mega-calcolatore in fondo al file:

```cpp
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
```

4. **MODIFICA** `IHTC_Output::printCosts()` sempre in `IHTC_Data.cc` affinché usi la struct:

```cpp
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
```

### STEP 3: Modifica il JSON Writer
1. Trova il file responsabile della scrittura JSON (es. `json/writer.cc` o la funzione che implementa l'esportazione).
2. All'inizio del blocco in cui si salvano i costi nel JSON, inserisci:
   `IHTC_Output::CostBreakdown cb = out.computeAllCosts();`
3. Sostituisci tutte le vecchie chiamate (es. `out.ComputeCostTotal()`) con le variabili della struct (es. `cb.total`, `cb.delay`, ecc.).