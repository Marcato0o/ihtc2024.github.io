# TASK: Ottimizzazione Cache e Flattening delle Matrici in solveNRA

**Ruolo:** Agisci come un Senior C++ Software Engineer esperto di High Performance Computing (HPC).

**Contesto del problema:**
La funzione `solveNRA` nel file `IHTC_Greedy.cc` fa largo uso di `std::vector` annidati (2D e 3D) per mappare lo stato dell'ospedale (es. `nurse_available`, `room_shift_load`, ecc.). Questo causa grave frammentazione dell'heap (Pointer Chasing) e distrugge la CPU Cache Locality durante i cicli annidati ad alta frequenza.

**Obiettivo:**
Sostituire tutte le matrici multi-dimensionali all'interno di `solveNRA` con singoli `std::vector` 1D contigui (Flat Arrays) e usare funzioni lambda inline per il calcolo degli indici, allineando il layout di memoria al pattern di accesso dei cicli (row-major order).

### STEP 1: Sostituzione integrale della funzione in `IHTC_Greedy.cc`
1. Apri il file `IHTC_Greedy.cc`.
2. Trova la definizione della funzione `void solveNRA(const IHTC_Input& in, IHTC_Output& out)`.
3. Sostituisci l'INTERA funzione con il codice seguente, che implementa i flat array e le lambda di indicizzazione:

```cpp
void solveNRA(const IHTC_Input& in, IHTC_Output& out) {
    int days = in.D;
    int shifts = std::max(1, in.shifts_per_day);
    int room_count = (int)in.rooms.size();
    int nurse_count = (int)in.nurses.size();

    out.clearNurseAssignments();
    if (room_count == 0 || nurse_count == 0 || days <= 0) return;

    // --- FUNZIONI LAMBDA PER INDICIZZAZIONE FLAT ARRAY ---
    // Ordine ottimizzato per i cicli: Giorni -> Turni -> Infermieri/Stanze
    auto idxDSN = [&](int d, int s, int n) { return d * (shifts * nurse_count) + s * nurse_count + n; };
    auto idxDSR = [&](int d, int s, int r) { return d * (shifts * room_count) + s * room_count + r; };
    auto idxDR  = [&](int d, int r) { return d * room_count + r; };

    int sz_DSN = days * shifts * nurse_count;
    int sz_DSR = days * shifts * room_count;
    int sz_DR  = days * room_count;

    // --- ALLOCAZIONI FLAT (1D) CONTIGUE ---
    std::vector<bool> nurse_available(sz_DSN, false);
    std::vector<int> nurse_shift_cap(sz_DSN, 0);
    std::vector<bool> room_occupied(sz_DR, false);
    std::vector<int> room_shift_load(sz_DSR, 0);
    std::vector<int> room_shift_skill(sz_DSR, 0);
    std::vector<int> nurse_load(sz_DSN, 0);

    // 1. Popolamento disponibilità infermieri
    for (int n = 0; n < nurse_count; ++n) {
        if (in.nurses[n].working_shifts.empty()) {
            for (int d = 0; d < days; ++d) {
                for (int s = 0; s < shifts; ++s) nurse_available[idxDSN(d, s, n)] = true;
            }
        } else {
            for (const auto& ws : in.nurses[n].working_shifts) {
                int d = ws.day, s = ws.shift;
                if (d >= 0 && d < days && s >= 0 && s < shifts) {
                    nurse_available[idxDSN(d, s, n)] = true;
                    nurse_shift_cap[idxDSN(d, s, n)] = ws.max_load;
                }
            }
        }
    }

    std::unordered_map<std::string, int> room_idx;
    for (int r = 0; r < room_count; ++r) room_idx[in.rooms[r].id] = r;

    // 2. Popolamento da occupanti
    for (const auto& f : in.occupants) {
        auto it = room_idx.find(f.room_id);
        if (it == room_idx.end()) continue;
        int ridx = it->second;
        int start = std::max(0, f.admission_day);
        int los = std::max(1, f.length_of_stay);
        for (int dd = 0; dd < los; ++dd) {
            int d = start + dd;
            if (d < 0 || d >= days) continue;
            room_occupied[idxDR(d, ridx)] = true;
            for (int s = 0; s < shifts; ++s) {
                int idx = dd * shifts + s;
                int load = 1;
                if (!f.nurse_load_per_shift.empty()) {
                    if (idx < (int)f.nurse_load_per_shift.size()) load = f.nurse_load_per_shift[idx];
                    else load = f.nurse_load_per_shift.back();
                }
                room_shift_load[idxDSR(d, s, ridx)] += load;
                room_shift_skill[idxDSR(d, s, ridx)] = std::max(room_shift_skill[idxDSR(d, s, ridx)], f.min_nurse_level);
            }
        }
    }

    // 3. Popolamento da pazienti ammessi
    for (int pid = 0; pid < (int)in.patients.size(); ++pid) {
        if (!out.isAdmitted(pid)) continue;
        int ridx = out.getRoomAssignedIdx(pid);
        if (ridx < 0 || ridx >= room_count) continue;
        int ad = out.getAdmitDay(pid);
        int los = std::max(1, in.patients[pid].length_of_stay);
        for (int dd = 0; dd < los; ++dd) {
            int d = ad + dd;
            if (d < 0 || d >= days) continue;
            room_occupied[idxDR(d, ridx)] = true;
            for (int s = 0; s < shifts; ++s) {
                int idx = dd * shifts + s;
                int load = 1;
                if (!in.patients[pid].nurse_load_per_shift.empty()) {
                    if (idx < (int)in.patients[pid].nurse_load_per_shift.size()) load = in.patients[pid].nurse_load_per_shift[idx];
                    else load = in.patients[pid].nurse_load_per_shift.back();
                }
                room_shift_load[idxDSR(d, s, ridx)] += load;
                room_shift_skill[idxDSR(d, s, ridx)] = std::max(room_shift_skill[idxDSR(d, s, ridx)], in.patients[pid].min_nurse_level);
            }
        }
    }

    // 4. Assegnazione greedy degli infermieri
    for (int d = 0; d < days; ++d) {
        for (int s = 0; s < shifts; ++s) {
            for (int r = 0; r < room_count; ++r) {
                if (!room_occupied[idxDR(d, r)]) continue;

                int demand = room_shift_load[idxDSR(d, s, r)];
                int req_skill = room_shift_skill[idxDSR(d, s, r)];

                int best_nurse = -1;
                long long best_score = std::numeric_limits<long long>::max();

                for (int n = 0; n < nurse_count; ++n) {
                    if (!nurse_available[idxDSN(d, s, n)]) continue;

                    int cur = nurse_load[idxDSN(d, s, n)];
                    int projected = cur + demand;
                    int cap = nurse_shift_cap[idxDSN(d, s, n)] > 0 ? nurse_shift_cap[idxDSN(d, s, n)] : 9999;
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
                    nurse_load[idxDSN(d, s, best_nurse)] += demand;
                    out.addNurseAssignment(best_nurse, d, s, r);
                }
            }
        }
    }
}
```