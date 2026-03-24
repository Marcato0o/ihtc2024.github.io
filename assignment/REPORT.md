# IHTC 2024 — Greedy Solver · Architecture & Logic

*Based on the source code in `code/` as of 2026-03-24.*

---

## 1. Problem Overview

The **Integrated Healthcare Timetabling Problem (IHTP)** combines three scheduling sub-problems:

| Sub-problem | Abbreviation | Question answered |
|---|---|---|
| Patient Admission Scheduling | PAS | On which day and in which room is each patient admitted? |
| Surgical Case Planning | SCP | In which operating theater (OT) does each patient's surgery take place? |
| Nurse-to-Room Assignment | NRA | Which nurse covers which room on each shift of each day? |

The planning horizon spans **D** days; each day has a fixed number of shifts (`shifts_per_day`, typically 3: early/late/night). The solver must satisfy all **hard constraints** (H1–H8) exactly and minimise the weighted sum of **soft constraint** violations (S1–S8).

---

## 2. Architecture

### 2.1 File map

```
IHTC_Data.hh      — all structs + IHTC_Input + IHTC_Output declarations
IHTC_Data.cc      — IHTC_Input and IHTC_Output implementations
IHTC_Greedy.hh    — GreedySolver namespace declaration
IHTC_Greedy.cc    — solver implementation (Phase 1 + Phase 2)
IHTC_Driver.cc    — main() entry point
io.hh             — jsonio:: function declarations (defined in sub-files)
parser.cc         — jsonio::load_instance()
writer.cc         — jsonio::write_solution()
nlohmann/json.hpp — header-only JSON library
```

### 2.2 Core classes

**`IHTC_Input`** — immutable after construction. Holds all parsed problem data: `patients`, `rooms`, `nurses`, `surgeons`, `occupants`, `ots`, `D`, `shifts_per_day`, and the 8 soft-constraint weights (`w_*`). Never mutated by the solver.

**`IHTC_Output`** — mutable solution state. Constructed from a `const IHTC_Input &`; stores a `bound_input` raw pointer to it (never null; `IHTC_Output` must not outlive its `IHTC_Input`). The constructor calls `init()` which allocates and zeros all solution arrays. Every subsequent mutation goes through `assignPatient()`, `seedOccupantStay()`, and `addNurseAssignment()`.

### 2.3 I/O

`jsonio::load_instance()` (in `parser.cc`) populates an `IHTC_Input` from a JSON file. `jsonio::write_solution()` (in `writer.cc`) serialises an `IHTC_Output` to JSON. Both are declared in `IHTC_Data.hh` via a forward namespace declaration; `io.hh` re-exposes them for convenience.

---

## 3. Data Flow — Step by Step

### Step 0 — Entry point (`IHTC_Driver.cc`)

```
argv[1]
  → IHTC_Input in(argv[1])         // parses JSON; immutable from here on
  → IHTC_Output out(in)            // allocates solution arrays via init()
  → GreedySolver::runFullSolver(in, out)
  → out.printCosts()
  → out.writeJSON("solution.json") // method on IHTC_Output
```

`IHTC_Input` is the **read-only** problem description. `IHTC_Output` is the **mutable** solution being built. Every solver function takes `const IHTC_Input &` (read-only) and `IHTC_Output &` (write target).

---

### Step 1 — Parsing (`parser.cc` → `jsonio::load_instance()`)

Converts the JSON file into typed C++ structs. Processing order matters because some entities reference others by string ID — those IDs must be resolved to integer indices before they can be used downstream.

1. **Scalars**: `D` (days) and `shifts_per_day` — needed to size all subsequent arrays.
2. **OTs** (`in.ots[]`): `id` + per-day `availability[]` array (minutes of surgery capacity). If the JSON provides a scalar capacity it is broadcast across all D days.
3. **Rooms** (`in.rooms[]`): `id` + bed capacity. Immediately builds `room_idx_by_id` (string → int). All downstream references use integer indices only.
4. **Nurses** (`in.nurses[]`): `id` + skill `level` + `working_shifts` list (day, shift 0/1/2, `max_load`). Shift names `"early"/"late"/"night"` are canonicalised to 0/1/2 here.
5. **Surgeons** (`in.surgeons[]`): `id` + per-day `max_surgery_time[]`. Builds `surgeon_idx_by_id`.
6. **Age-group index map**: shared between patients and occupants. The first time a given age-group value appears it is assigned the next integer. Guarantees that e.g. `"elderly"` maps to the same integer in both patients and occupants — essential for correct S1 computation.
7. **Patients** (`in.patients[]`): all fields parsed; `surgeon_id` string → `surgeon_idx`; `incompatible_room` strings → `incompatible_room_idxs`; `age_group` via the shared map.
8. **Occupants** (`in.occupants[]`): pre-existing patients already in a room at day 0. `room_id` → `room_idx`; `age_group` via the shared map.
9. **Weights** (`in.w_*`): the 8 soft-constraint multipliers from the `"weights"` JSON object.

After parsing, `IHTC_Input` is fully populated and never modified again.

---

### Step 2 — Output initialisation (`IHTC_Output::IHTC_Output(in)` → `init()`)

`init()` is called once, from the constructor. It allocates and zeroes all mutable solution arrays:

| Array | Storage | Size | Initial value | Purpose |
|---|---|---|---|---|
| `admitted` | `vector<bool>` | n_patients | false | Is patient scheduled? |
| `admit_day` | `vector<int>` | n_patients | -1 | Which day admitted |
| `room_assigned_idx` | `vector<int>` | n_patients | -1 | Which room |
| `ot_assigned_idx` | `vector<int>` | n_patients | -1 | Which OT (−1 = no surgery) |
| `room_occupancy` | flat `vector<int>` | rooms × D | 0 | Beds used; index = `room * D + day` |
| `room_gender` | flat `vector<Gender>` | rooms × D | NONE | Locked gender; index = `room * D + day` |
| `ot_availability` | `vector<vector<int>>` | ots × D | copied from `in.ots[i].availability` | Remaining surgery minutes |
| `surgeon_availability` | `vector<vector<int>>` | surgeons × D | copied from `in.surgeons[i].max_surgery_time` | Remaining surgeon minutes |
| `room_day_min_age` | flat `vector<int>` | rooms × D | INT_MAX | Min age_group in room/day (S1) |
| `room_day_max_age` | flat `vector<int>` | rooms × D | -1 | Max age_group in room/day (S1) |
| `surgeon_day_ot_used` | flat `vector<bool>` | surgeons × D × ots | false | OT-usage per surgeon/day (S6) |

`ot_availability` and `surgeon_availability` start as copies of the problem's maximum capacities and are **decremented** as patients are assigned, giving `canAssignPatient()` an O(1) remaining-capacity check.

`room_day_min_age` uses `INT_MAX` as sentinel for "no patient yet" because age groups are non-negative integers; `INT_MAX` cannot be confused with a real value and lets the first-patient case be handled uniformly with subsequent ones.

Five incremental cost caches, also zeroed here:

| Cache | Tracks | Soft cost |
|---|---|---|
| `cache_delay_raw` | Sum of `(admit_day − release_date)` | S7 |
| `cache_open_ot_count` | Count of first-use `(day, OT)` pairs | S5 |
| `cache_age_mix_raw` | Sum of `(max−min)` range increments | S1 |
| `cache_surgeon_xfer_raw` | Count of surgeon OT-switch events | S6 |
| `cache_unscheduled_raw` | Count of optional patients not placed | S8 |

---

### Step 3 — Phase 1: PAS + SCP (`GreedySolver::solvePASandSCP()`)

Assigns each patient a `(day, room, OT)` triple. Proceeds in five sub-steps.

#### 3a. Seed occupants (`seedOccupants()` → `IHTC_Output::seedOccupantStay()`)

Occupants are patients already admitted before day 0. Their placement is fixed — they are not decision variables — but they consume beds, lock room genders, and contribute to age-mix cost from day 0.

`seedOccupantStay(room_idx, length_of_stay, sex, age_group)` iterates over days `[0, los)`:
1. Increments `room_occupancy[room * D + d]`.
2. Sets `room_gender[room * D + d]` on the first occupant with a known sex.
3. Calls `applyAgeMixUpdate(room * D + d, age_group)` to update the S1 range cache.

Must run **before** the patient scheduling loop so that `canAssignPatient()` already sees the real state at day 0.

#### 3b. Static priority order (`sortPatientsByPriority()`)

Returns a vector of patient indices sorted by (highest → lowest priority):

1. **Mandatory before optional** — a skipped mandatory patient is an H5 hard-constraint violation.
2. **Earlier `due_date` first** (mandatory only) — the patient who expires sooner must be placed first.
3. **Earlier `release_date` first** (mandatory, same due_date) — available sooner, easier to place.
4. **Narrower admission window first** (`due_date − release_date`) — fewer valid days.
5. **Longer `length_of_stay` first** — occupies more future bed-days; placing it late blocks many slots.
6. **Longer `surgery_time` first** — harder to fit into OT capacity.

This order is computed once and used both as the iteration sequence and as the tiebreaker in the dynamic loop.

#### 3c. Dynamic scheduling loop

The loop runs `n_patients` steps. At each step it picks the best unscheduled patient via a **lexicographic comparison**:

```
for each unscheduled patient pid (iterated in static-priority order):
    feasible = countFeasiblePlacements(pid, stop_after = best_feasible_count)
    better if:
      1. no candidate yet
      2. mandatory status: mandatory beats optional
      3. fewer feasible placements (most constrained)
      4. lower base_rank from static priority order
```

**Why dynamic feasibility counting?** Each assignment can fill a room, exhaust an OT, or use up a surgeon's time — shrinking the option set of remaining patients. A patient that had 50 valid options initially may now have only 1. Always picking the most constrained patient first (`countFeasiblePlacements` minimum) prevents it from being left with zero options.

**`stop_after` early exit**: `countFeasiblePlacements` returns as soon as its count exceeds `stop_after`. When the current best has `k` options, any patient with more than `k` cannot win on criterion 3, so counting beyond `k` is wasted work.

After the loop selects `best_pid`, it calls `schedulePatient(in, out, best_pid)`. On failure:
- If mandatory → emits an `[H5 VIOLATION]` error.
- If optional → calls `out.markOptionalUnscheduled()` which increments `cache_unscheduled_raw` (S8 cache).

#### 3d. `schedulePatient()` — find and apply the best placement

Iterates over all `(d, r, ot)` combinations in the patient's admission window:

```
for d in [release_date .. D-1]  (mandatory: capped at due_date):
  for r in all rooms:
    for ot in all OTs  (or ot = -1 if surgery_time == 0):
      if canAssignPatient(patient_id, d, r, ot, in):
        cost = evaluatePlacementCost(in, out, patient_id, d, r, ot)
        if cost < min_cost: record as best
        if min_cost == 0: break all loops immediately
```

`canAssignPatient()` enforces **hard constraints** in a single short-circuiting pass:
- **H6**: `day ∈ [release_date, due_date]` for mandatory; `day ≥ release_date` for optional.
- **H7**: `room_occupancy[room * D + d] < room.capacity` for every day of the stay.
- **H1**: `room_gender[room * D + d]` compatible with the patient's sex on every stay day.
- **H2**: `room_idx` not in `incompatible_room_idxs`.
- **H4**: `ot_availability[ot][day] ≥ surgery_time` (when ot ≥ 0).
- **H3**: `surgeon_availability[surgeon][day] ≥ surgery_time`.

`evaluatePlacementCost()` computes the **marginal soft costs** that depend on placement:
- **S7 (delay)**: `(d − release_date) × w_patient_delay`.
- **S5 (open OT)**: `w_open_operating_theater` if `ot_availability[ot][day] == in.ots[ot].availability[day]` (OT unused today).
- **S1 (age mix)**: for each stay day, `getRoomAgeMixMarginal(room, d_stay, age_group) × w_room_mixed_age`. `getRoomAgeMixMarginal` reads `room_day_min/max_age` and returns the delta in `(max−min)` range without committing any state.
- **S6 (surgeon transfer)**: `w_surgeon_transfer` if `surgeonHasOtherOTOnDay(surgeon, day, ot)` returns true.

S2/S3/S4 are excluded — they depend on nurse assignments that are not known until Phase 2.

#### 3e. `assignPatient()` — atomic state update

Commits a placement and updates all state in order:

1. **Admission records**: `admitted[pid] = true`, `admit_day`, `room_assigned_idx`, `ot_assigned_idx`.
2. **Room occupancy + gender lock**: for each `d ∈ [day, day+los)`, increments `room_occupancy[room * D + d]` and sets `room_gender[room * D + d]` on first patient with a known sex.
3. **OT capacity + S5 cache**: decrements `ot_availability[ot][day]` by `surgery_time`. If the OT's remaining capacity equalled its maximum (first use today), increments `cache_open_ot_count`.
4. **Surgeon budget**: decrements `surgeon_availability[surgeon][day]` by `surgery_time`.
5. **S7 cache**: adds `max(0, day − release_date)` to `cache_delay_raw`.
6. **S6 cache + tracking**: checks `surgeon_day_ot_used[(surgeon * D + day) * n_ots + ot]`. If the surgeon already used a *different* OT today, increments `cache_surgeon_xfer_raw`. Then marks this OT as used.
7. **S1 cache + tracking**: for each stay day, calls `applyAgeMixUpdate(room * D + d_abs, age_group)` which reads `room_day_min/max_age[flat_idx]`, computes the delta, adds it to `cache_age_mix_raw`, and updates the min/max arrays. `INT_MAX` sentinel means "first patient on this room/day" — delta is 0.

---

### Step 4 — Phase 2: NRA (`GreedySolver::solveNRA()`)

Nurse assignment is independent of patient placement (nurses do not affect H1–H7), so it runs as a completely separate pass after Phase 1.

All working arrays are **flat vectors with lambda indices** to keep memory contiguous:

```cpp
auto nds_idx = [&](int n, int d, int s) { return (n * days + d) * shifts + s; };      // nurse × day × shift
auto dr_idx  = [&](int d, int r)        { return d * room_count + r; };                 // day × room
auto dsr_idx = [&](int d, int s, int r) { return (d * shifts + s) * room_count + r; }; // day × shift × room
auto dsn_idx = [&](int d, int s, int n) { return (d * shifts + s) * nurse_count + n; };// day × shift × nurse
```

#### 4a. Nurse availability and capacity tables

Two flat arrays indexed by `nds_idx(n, d, s)`:
- `nurse_available[nds_idx(n,d,s)]` — true if nurse `n` works shift `s` on day `d`.
- `nurse_shift_cap[nds_idx(n,d,s)]` — `max_load` for that shift.

Populated by iterating over each nurse's `working_shifts` list.

#### 4b. Room demand tables

Three flat arrays:
- `room_occupied[dr_idx(d,r)]` — true if room `r` has ≥1 patient on day `d`. Only occupied rooms need a nurse.
- `room_shift_load[dsr_idx(d,s,r)]` — total nursing workload demanded by all patients in room `r` during shift `s` on day `d`.
- `room_shift_skill[dsr_idx(d,s,r)]` — maximum skill level required by any patient in that room/shift/day.

Populated by the `add_stay` lambda called once for each occupant (start_day=0) and each admitted patient (start_day=admit_day). Both use the same per-shift index `dd * shifts + s` where `dd` is days elapsed since admission start.

#### 4c. Nurse selection loop

Iterates over every `(day, shift, room)` triple that is occupied. For each, selects the best available nurse via **explicit lexicographic comparison** (hardest consequence first):

```
for each nurse n available on (day, shift):
    skill_gap  = max(0, req_skill − nurse.level)
    overload   = max(0, (current_load[dsn_idx(d,s,n)] + demand) − shift_cap)
    projected  = current_load[dsn_idx(d,s,n)] + demand

    better if:
      1. no candidate yet
      2. skill_gap  < best_skill_gap   (directly raises S2; skill is fixed — can't recover)
      3. overload   < best_overload    (directly raises S4; exceeding cap has irreversible cost)
      4. projected  < best_projected  (load balancing; softest criterion)
      5. nurse index < best (stable tiebreaker)
```

The winning nurse is recorded with `addNurseAssignment(n, d, s, r)` and their load for this shift is accumulated into `nurse_load[dsn_idx(d,s,n)]` so subsequent room assignments in the same shift see the updated load.

If no nurse is available for an occupied slot, an `[H8 VIOLATION]` is emitted to stderr.

---

### Step 5 — Cost computation (`IHTC_Output::computeAllCosts()`)

Returns a `CostBreakdown` struct. Called once after the solver finishes (and again in the writer).

**Five caches read in O(1):**

| Cost | Expression |
|---|---|
| S1 (AgeMix) | `cache_age_mix_raw × w_room_mixed_age` |
| S5 (OpenOT) | `cache_open_ot_count × w_open_operating_theater` |
| S6 (SurgeonTransfer) | `cache_surgeon_xfer_raw × w_surgeon_transfer` |
| S7 (PatientDelay) | `cache_delay_raw × w_patient_delay` |
| S8 (Unscheduled) | `cache_unscheduled_raw × w_unscheduled_optional` |

**Three costs computed from scratch** (require scanning nurse assignments):

`computeAllCosts` first rebuilds two auxiliary tables from `nurse_assignments`:
- `room_shift_load[dsr_idx(d,sh,r)]` — total workload in each room/shift/day (from occupants + admitted patients via `add_stay_load` lambda).
- `nurse_load_by_shift[nsh_idx(n, d*shifts+sh)]` — workload each nurse accumulated per shift (indexed by `n * (days * shifts) + day * shifts + shift`).
- `room_shift_nurse[dsr_idx(d,sh,r)]` — which nurse covers each room/shift/day (−1 if none).

Then:
- **S2 (Skill)**: for each patient/occupant, for each stay day and shift, if `room_shift_nurse[dsr]` is valid and `nurse.level < required_skill`, add the shortfall. Multiplied by `w_room_nurse_skill`.
- **S3 (Continuity)**: for each patient/occupant, collect the set of distinct nurse indices who covered their room during their stay; `raw_cont += set.size()`. Multiplied by `w_continuity_of_care`.
- **S4 (Excess workload)**: for each nurse, iterate their `working_shifts` only; `over = max(0, nurse_load_by_shift[...] − ws.max_load)`. Multiplied by `w_nurse_eccessive_workload`.

S2/S3/S4 cannot be cached during Phase 1 because they depend on nurse assignments not yet made.

---

### Step 6 — JSON output (`writer.cc` → `jsonio::write_solution()`)

Serialises to `solution.json` (called via `out.writeJSON()`):
- **patients array**: `id` + `admission_day` (integer or `"none"`) + `room` id + `operating_theater` id.
- **nurses array**: for each nurse, `id` + `assignments` list of `{day, shift_name, [room_ids]}`. A nurse can cover multiple rooms per shift.
- **costs array**: one string with all 8 cost components and the total (from `computeAllCosts()`).

---

## 4. State Mutation Summary

| Event | Arrays mutated | Caches updated |
|---|---|---|
| `init()` | All arrays allocated/zeroed | All 5 caches = 0 |
| `seedOccupantStay()` | `room_occupancy`, `room_gender`, `room_day_min/max_age` | `cache_age_mix_raw` |
| `assignPatient()` | `admitted`, `admit_day`, `room_assigned_idx`, `ot_assigned_idx`, `room_occupancy`, `room_gender`, `ot_availability`, `surgeon_availability`, `room_day_min/max_age`, `surgeon_day_ot_used` | `cache_delay_raw`, `cache_open_ot_count`, `cache_age_mix_raw`, `cache_surgeon_xfer_raw` |
| `markOptionalUnscheduled()` | — | `cache_unscheduled_raw` |
| `addNurseAssignment()` | `nurse_assignments` | — |
| `computeAllCosts()` | nothing (read-only) | — |

---

## 5. Hard and Soft Constraints

| Label | Name | Violation condition | Enforced by |
|---|---|---|---|
| **H1** | RoomGenderMix | Room contains both Gender A and B on the same day | `canAssignPatient()` — gender lock check over entire stay |
| **H2** | PatientRoomCompatibility | Patient placed in an incompatible room | `canAssignPatient()` — `incompatible_room_idxs` loop |
| **H3** | SurgeonOvertime | Surgeon surgery time on a day > `max_surgery_time[day]` | `canAssignPatient()` — `surgeon_availability` check |
| **H4** | OperatingTheaterOvertime | OT surgery time on a day > `availability[day]` | `canAssignPatient()` — `ot_availability` check |
| **H5** | MandatoryUnscheduledPatients | Mandatory patient not admitted | Greedy priority (mandatory always chosen first) |
| **H6** | AdmissionDay | Patient admitted outside `[release_date, due_date]` | `canAssignPatient()` — day bounds check |
| **H7** | RoomCapacity | `room_occupancy[room][day] ≥ room.capacity` | `canAssignPatient()` — occupancy loop over entire stay |
| **H8** | NursePresence | Occupied room/shift has no assigned nurse | `solveNRA()` — covers every occupied slot; emits `[H8 VIOLATION]` to stderr if no nurse works that shift |

| Label | Name | Cost formula | Weight |
|---|---|---|---|
| **S1** | RoomAgeMix | `cache_age_mix_raw × w` — sum of `(max_age − min_age)` per room/day | `w_room_mixed_age` |
| **S2** | RoomSkillLevel | Sum of skill shortfalls (nurse level < required) | `w_room_nurse_skill` |
| **S3** | ContinuityOfCare | Sum of distinct-nurse-count per patient/occupant over their stay | `w_continuity_of_care` |
| **S4** | ExcessWorkload | Sum of excess load per nurse per working shift | `w_nurse_eccessive_workload` |
| **S5** | OpenOperatingTheater | `cache_open_ot_count × w` — count of first-used `(day, OT)` pairs | `w_open_operating_theater` |
| **S6** | SurgeonTransfer | `cache_surgeon_xfer_raw × w` — surgeon using >1 OT on the same day | `w_surgeon_transfer` |
| **S7** | PatientDelay | `cache_delay_raw × w` — sum of `(admit_day − release_date)` | `w_patient_delay` |
| **S8** | UnscheduledOptional | `cache_unscheduled_raw × w` — count of optional patients not admitted | `w_unscheduled_optional` |

---

## 6. Design Philosophy

### Principle 1: Flat arrays with lambda indices for rectangular grids

`vector<vector<vector<T>>>` causes one heap allocation per inner vector and cache misses on traversal. For any fixed-size grid inside a function, the correct pattern is a single flat `vector<T>` of size `A*B*C` with a local lambda:

```cpp
auto xyz = [&](int x, int y, int z) { return (x*B + y)*C + z; };
```

Nested vectors are used only when the inner dimension is genuinely variable (e.g., the list of patients in a room changes per room). Note: `ot_availability` and `surgeon_availability` are kept as `vector<vector<int>>` because their outer dimension (number of OTs / surgeons) is small and the access pattern is always `[ot][day]` or `[surgeon][day]`, which does not cross inner-vector boundaries in a hot loop.

### Principle 2: Explicit lexicographic comparison — no weighted scores

A weighted score like `1'000'000*a + 10'000*b + c` encodes priority as a numeric encoding with a hidden correctness assumption: no component may exceed the coefficient gap. This assumption is not enforced and can break silently.

The correct pattern: one `best_*` variable per criterion, selected with chained `else if`:

```cpp
bool better = false;
if      (best == -1)               better = true;
else if (crit_a != best_a)         better = (crit_a < best_a);
else if (crit_b != best_b)         better = (crit_b < best_b);
else                               better = (tiebreaker);
```

Each criterion activates only when all higher-priority ones are tied. The hierarchy is visible in the code structure, correct for any value magnitudes, and short-circuits early. **Never introduce a weighted numeric score.**

### Principle 3: Hardest consequence first

In any single-pass greedy, every decision is irrevocable. Rank criteria so that the hardest-to-recover-from violation comes first:

- **Patient selection**: mandatory > optional (hard constraint); fewer feasible options > more (avoids stranding); earlier due date; longer stay.
- **Nurse selection**: smallest skill gap (skill is fixed; shortfall directly raises S2); smallest overload (exceeding cap raises S4); lightest projected load (S3/S4 health, softest criterion).

When adding a new selection loop, ask: *if I choose wrong here, how hard is it to fix later in this same pass?* The answer determines the criterion's rank.

---

## 7. Verified Instance-Format Invariants

These invariants were confirmed across all 165 available instances. No defensive guards are needed for them.

- `nurse_load_per_shift` / `workload_produced`: always present for every patient and occupant; values always ≥ 1.
- `skill_level_required_per_shift`: always present; values may be 0 (any nurse qualifies). Direct array access is safe.
- `operating_theater.availability`: always an array of exactly D elements; direct `[day]` indexing is safe.
- `patient.length_of_stay` / `occupant.length_of_stay`: always ≥ 1.
- `patient.age_group` / `occupant.age_group`: always present and ≥ 0 after parsing.
- `patient.due_date` (mandatory): always ≤ D−1.
- `patient.release_date`: always ≥ 0.
- `working_shift.max_load`: always ≥ 1.
- `occupant.room_idx`: always a valid room index ≥ 0.
- `working_shift.day` and `.shift`: always within bounds for their respective instance.
- `nurse.working_shifts`: always present and non-empty.
- `days`, rooms, nurses: always > 0.
