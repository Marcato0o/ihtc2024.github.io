# Integrated Healthcare Timetabling Problem (IHTP) - Greedy Solver Report

## 1) Problem state

The project solves the **Integrated Healthcare Timetabling Problem (IHTP)**, where three sub-problems are coupled:

1. **PAS**: Patient Admission Scheduling
2. **SCP**: Surgical Case Planning
3. **NRA**: Nurse-to-Room Assignment

Planning is done over **D days** and **3 shifts/day**. The solver must always satisfy hard constraints (room capacity/gender, OT and surgeon capacities, mandatory patient admission window, nurse coverage) and minimize soft penalties (delay, optional unscheduled patients, continuity of care, etc.).

Current implementation status:
- Constructive greedy algorithm (no backtracking, no local search)
- Complete JSON I/O pipeline restored and working
- Batch test script with validator integration and CSV reporting

Reference docs:
- Problem description: `README.md`
- Official validator source: `assets/files/IHTP_Validator.cc`

### 1.1 Deep definition of the three coupled sub-problems

The optimization objective can be written as:

`min sum_{k=1..8} (w_k * C_k)` subject to all hard constraints being satisfied.

This is a **coupled** problem: PAS, SCP, and NRA are not independent; each decision changes feasibility and cost space for the others.

#### PAS — Patient Admission Scheduling

- Decision: for each patient `p`, choose admission day `d` (or unscheduled if optional) and room `r`.
- Equivalent binary modeling view: `x[p,d,r] = 1` if patient `p` starts stay on day `d` in room `r`.
- The assignment induces a stay interval `[d, d + LOS_p - 1]`, consuming room capacity each day.
- PAS must satisfy admission-window and room constraints (compatibility, capacity, gender consistency).
- PAS directly generates downstream load for NRA (room/shift workload + required skill).

#### SCP — Surgical Case Planning

- Decision: for each admitted surgical patient, assign an operating theater on the admission day.
- Binary view: `y[p,o,d] = 1` if patient `p` uses OT `o` on day `d`.
- Feasibility constraints: OT daily minutes and surgeon daily available minutes.
- Strong coupling with PAS: a PAS candidate `(d, r)` is only truly feasible if SCP can assign OT/surgeon resources on day `d`.
- SCP also affects soft terms such as open OTs and surgeon transfer.

#### NRA — Nurse-to-Room Assignment

- Decision: for each occupied room `r`, day `d`, shift `s`, assign nurse(s) `n`.
- Binary view: `z[n,r,s,d] = 1` if nurse `n` covers room `r` at `(d, s)`.
- Demand is induced by PAS + fixed occupants: per room/shift workload and minimum required skill.
- Feasibility uses nurse roster/availability and coverage constraints.
- Main quality drivers are skill mismatch, overload, and continuity of care.

### 1.2 Hard and soft constraints (validator-aligned)

#### Hard constraints

From the validator output categories (`IHTP_Validator.cc`):

- `RoomGenderMix`: no mixed gender in the same room/day.
- `PatientRoomCompatibility`: patient cannot be placed in incompatible room.
- `SurgeonOvertime`: surgeon daily surgery minutes cannot exceed daily limit.
- `OperatingTheaterOvertime`: OT daily minutes cannot exceed capacity.
- `MandatoryUnscheduledPatients`: mandatory patients cannot remain unscheduled.
- `AdmissionDay`: admission must respect release/due-day rules.
- `RoomCapacity`: room occupancy cannot exceed bed capacity.
- `NursePresence`: assigned nurse must be valid/present per roster availability.
- `UncoveredRoom`: every occupied room/shift must be covered.

#### Soft constraints

From the validator cost terms:

- `RoomAgeMix`: penalize age-group mixing in shared rooms.
- `RoomSkillLevel`: penalize nurse skill deficits versus required skill.
- `ContinuityOfCare`: penalize too many distinct nurses across one patient stay.
- `ExcessiveNurseWorkload`: penalize workload above nurse max load.
- `OpenOperatingTheater`: penalize number of opened OTs per day.
- `SurgeonTransfer`: penalize surgeons split across multiple OTs in one day.
- `PatientDelay`: penalize admission delay beyond release day.
- `ElectiveUnscheduledPatients`: penalize optional patients left unscheduled.

---

## 2) Code structure

Main modules in `assignment/`:

- **`IHTC_Driver.cc`**
  - CLI entry point
  - Loads an instance, runs solver, prints cost, writes `solution.json`

- **`IHTC_Data.hh` / `IHTC_Data.cc`**
  - Domain model (`Patient`, `Room`, `Nurse`, `Surgeon`, `Occupant`, `OT`)
  - `IHTC_Input`: parsed instance data
  - `IHTC_Output`: encapsulated mutable state + cost computation + JSON export API

- **`IHTC_Greedy.hh` / `IHTC_Greedy.cc`**
  - `GreedySolver::solvePASandSCP(...)`
  - `GreedySolver::solveNRA(...)`
  - `GreedySolver::runFullSolver(...)`
  - Hidden helper functions in anonymous namespace

- **`json/io.hh`, `json/parser.cc`, `json/writer.cc`**
  - Parsing input JSON to `IHTC_Input`
  - Writing output JSON from `IHTC_Output`

- **`run_all_tests.sh`**
  - Runs all `test*.json` instances
  - Runs official validator per test
  - Produces `validation_report.csv` with solver + validator metrics

---

## 3) Algorithm structure and logic (step by step)

### 3.1 Global flow

`runFullSolver(in, out)`:
1. Print start message
2. Run `solvePASandSCP` (joint patient placement)
3. Run `solveNRA` (nurse assignment)
4. Print end message

### 3.2 PAS + SCP (joint greedy placement)

1. Initialize output state and seed fixed occupants.
2. Build a base patient priority order:
   - mandatory before optional
   - tighter mandatory windows first
   - then longer stay / larger surgery time
3. At each iteration, among unscheduled patients:
   - compute the number of **currently feasible** placements `(day, room, OT)`
   - choose the patient with **minimum feasible options** (most constrained first), preserving mandatory priority and base rank as tie-breakers
4. For the selected patient, evaluate feasible placements with a greedy cost score:
   - delay penalty contribution
   - opening new OT penalty proxy
   - room occupancy pressure proxy
   - minor surgery-time proxy
5. Commit the best feasible placement immediately.
6. Continue until all patients are processed.

This remains fully greedy: no rollback and no exchange moves.

### 3.3 NRA (greedy nurse assignment)

1. Build nurse availability from roster (and optional raw JSON `working_shifts` fallback).
2. Build room demand per `(day, shift, room)` from occupants + admitted patients:
   - workload
   - required minimum nurse skill
3. For each occupied room/shift, select one nurse greedily with lexicographic score:
   - prioritize skill feasibility gap
   - then overload amount
   - then projected load
   - deterministic nurse index tie-break
4. Commit assignment and update nurse load state.

### 3.4 Cost and validation

- Internal solver prints a weighted cost breakdown.
- Official validator independently computes hard violations and official total cost.
- Both are stored by the batch script into CSV.

### 3.5 Phase-by-phase code walkthrough (with simple line-by-line explanation)

This section uses real excerpts from `assignment/IHTC_Greedy.cc`.

## PAS + SCP

### Phase 1 — Initialize state and patient ordering

```cpp
// [1]
out.init(in.patients.size(), in.rooms.size(), in.ots.size(), horizonDays(in));
// [2]
seedOccupants(in, out);

// [3]
std::vector<int> order = sortPatientsByPriority(in);
// [4]
std::vector<int> base_rank(in.patients.size(), 0);
// [5]
for (int i = 0; i < (int)order.size(); ++i) base_rank[order[i]] = i;
// [6]
std::vector<bool> done(in.patients.size(), false);
```

Line-by-line:
- [1] Resets the output state (admissions, room usage, OT usage) for all patients/rooms/OTs.
- [2] Inserts already-present occupants into room occupancy before scheduling new patients.
- [3] Builds the base greedy order (mandatory first, tighter windows first, etc.).
- [4] Allocates an array to remember each patient’s position in that base order.
- [5] Fills the rank array, used later as a tie-breaker.
- [6] Marks all patients as “not processed yet”.

### Phase 2 — Choose next patient (most constrained first)

```cpp
for (int pid : order) {
  // [1]
  if (done[pid]) continue;
  // [2]
  bool is_mandatory = in.patients[pid].mandatory;
  // [3]
  int current_stop = (chosen_feasible == INT_MAX) ? INT_MAX : chosen_feasible;
  // [4]
  int feasible = countFeasiblePlacements(in, out, pid, current_stop);
  // [5]
  int rank = base_rank[pid];

  // [6]
  bool better = false;
  // [7]
  if (chosen_pid == -1) better = true;
  // [8]
  else if (is_mandatory != chosen_mandatory) better = is_mandatory;
  // [9]
  else if (feasible != chosen_feasible) better = (feasible < chosen_feasible);
  // [10]
  else if (rank < chosen_rank) better = true;

  // [11]
  if (better) {
    // [12]
    chosen_pid = pid;
    // [13]
    chosen_mandatory = is_mandatory;
    // [14]
    chosen_feasible = feasible;
    // [15]
    chosen_rank = rank;
  }
}
```

Line-by-line:
- [1] Skips patients already handled.
- [2] Reads whether this patient is mandatory.
- [3] Sets an early-stop threshold for faster feasibility counting.
- [4] Counts how many `(day, room, OT)` placements are feasible right now.
- [5] Gets base-order rank for tie-break.
- [6] Starts with “not better”.
- [7] First candidate is automatically best so far.
- [8] Mandatory patient beats optional patient.
- [9] Fewer feasible placements wins (most constrained first).
- [10] If still tied, earlier base rank wins.
- [11] If candidate is better, update best choice.
- [12]-[15] Save all comparison fields for next iterations.

### Phase 3 — Place selected patient with best feasible tuple

```cpp
bool schedulePatient(const IHTC_Input& in, IHTC_Output& out, int patient_id) {
  // [1]
  int best_day = -1, best_room = -1, best_ot = -1;
  // [2]
  double min_cost = std::numeric_limits<double>::max();

  // [3]
  int start_d = std::max(0, in.patients[patient_id].release_date);
  // [4]
  int end_d = (in.D > 0) ? (in.D - 1) : start_d;
  // [5]
  if (in.patients[patient_id].mandatory) end_d = std::min(end_d, in.patients[patient_id].due_date);

  // [6]
  for (int d = start_d; d <= end_d; ++d) {
    // [7]
    for (int r = 0; r < (int)in.rooms.size(); ++r) {
      // [8]
      int ot_start = in.patients[patient_id].surgery_time > 0 ? 0 : -1;
      // [9]
      int ot_end = in.patients[patient_id].surgery_time > 0 ? (int)in.ots.size() - 1 : -1;
      // [10]
      for (int ot = ot_start; ot <= ot_end; ++ot) {
        // [11]
        if (!out.canAssignPatient(patient_id, d, r, ot, in)) continue;
        // [12]
        double c = evaluatePlacementCost(in, out, patient_id, d, r, ot);
        // [13]
        if (c < min_cost) { min_cost = c; best_day = d; best_room = r; best_ot = ot; }
      }
    }
  }

  // [14]
  if (best_day != -1) { out.assignPatient(patient_id, best_day, best_room, best_ot, in); return true; }
  // [15]
  return false;
}
```

Line-by-line:
- [1] Initializes best placement as “not found”.
- [2] Starts with a very large best cost.
- [3]-[5] Computes legal admission day window.
- [6]-[10] Enumerates all candidate day/room/OT tuples.
- [11] Rejects candidates breaking hard constraints.
- [12] Computes greedy local cost for feasible candidate.
- [13] Keeps the lowest-cost feasible candidate.
- [14] If a candidate was found, commits assignment.
- [15] Otherwise reports scheduling failure.

## NRA

### Phase 1 — Build nurse availability

```cpp
// [1]
std::vector<std::vector<std::vector<bool>>> nurse_available(
  nurse_count,
  std::vector<std::vector<bool>>(days, std::vector<bool>(shifts, false))
);
// [2]
std::vector<bool> has_explicit_availability(nurse_count, false);

// [3]
for (int n = 0; n < nurse_count; ++n) {
  // [4]
  if (!in.nurses[n].roster.empty()) {
    // [5]
    has_explicit_availability[n] = true;
    // [6]
    for (int g = 0; g < (int)in.nurses[n].roster.size(); ++g) {
      // [7]
      if (in.nurses[n].roster[g] <= 0) continue;
      // [8]
      int d = g / shifts;
      // [9]
      int s = g % shifts;
      // [10]
      if (d >= 0 && d < days && s >= 0 && s < shifts) nurse_available[n][d][s] = true;
    }
  }
}
```

Line-by-line:
- [1] Creates a `nurse x day x shift` availability matrix initialized to false.
- [2] Tracks which nurses have explicit availability info.
- [3] Loops all nurses.
- [4] Uses roster if present.
- [5] Marks this nurse as explicitly constrained.
- [6] Loops all roster entries.
- [7] Ignores “not working” entries.
- [8]-[9] Converts flat index into `(day, shift)`.
- [10] Marks that nurse available in that time slot.

### Phase 2 — Build room demand from occupants and admitted patients

```cpp
// [1]
std::vector<std::vector<bool>> room_occupied(days, std::vector<bool>(room_count, false));
// [2]
std::vector<std::vector<std::vector<int>>> room_shift_load(days, std::vector<std::vector<int>>(shifts, std::vector<int>(room_count, 0)));
// [3]
std::vector<std::vector<std::vector<int>>> room_shift_skill(days, std::vector<std::vector<int>>(shifts, std::vector<int>(room_count, 0)));

// [4] occupants contribution
for (const auto& f : in.occupants) {
  ...
  room_occupied[d][ridx] = true;
  room_shift_load[d][s][ridx] += load;
  room_shift_skill[d][s][ridx] = std::max(room_shift_skill[d][s][ridx], f.min_nurse_level);
}

// [5] admitted patients contribution
for (int pid = 0; pid < (int)in.patients.size(); ++pid) {
  if (!out.isAdmitted(pid)) continue;
  ...
  room_occupied[d][ridx] = true;
  room_shift_load[d][s][ridx] += load;
  room_shift_skill[d][s][ridx] = std::max(room_shift_skill[d][s][ridx], in.patients[pid].min_nurse_level);
}
```

Line-by-line:
- [1] Marks whether each room is occupied in each day.
- [2] Stores workload demand for each room/day/shift.
- [3] Stores required minimum skill for each room/day/shift.
- [4] Adds demand generated by pre-existing occupants.
- [5] Adds demand generated by newly admitted patients.

### Phase 3 — Greedy nurse selection per occupied room/shift

```cpp
for (int d = 0; d < days; ++d) {
  for (int s = 0; s < shifts; ++s) {
    for (int r = 0; r < room_count; ++r) {
      // [1]
      if (!room_occupied[d][r]) continue;

      // [2]
      int demand = room_shift_load[d][s][r];
      // [3]
      int req_skill = room_shift_skill[d][s][r];

      // [4]
      int best_nurse = -1;
      long long best_score = std::numeric_limits<long long>::max();

      for (int n = 0; n < nurse_count; ++n) {
        // [5]
        if (!nurse_available[n][d][s]) continue;

        // [6]
        int projected = nurse_load[d][s][n] + demand;
        int cap = in.nurses[n].max_load > 0 ? in.nurses[n].max_load : 9999;
        int overload = std::max(0, projected - cap);
        int skill_gap = std::max(0, req_skill - in.nurses[n].level);

        // [7]
        long long score = 0;
        score += 1000000LL * skill_gap;
        score += 10000LL * overload;
        score += 10LL * projected;
        score += n;

        // [8]
        if (score < best_score) { best_score = score; best_nurse = n; }
      }

      // [9]
      if (best_nurse >= 0) {
        nurse_load[d][s][best_nurse] += demand;
        out.addNurseAssignment(best_nurse, d, s, r);
      }
    }
  }
}
```

Line-by-line:
- [1] Skip empty rooms (no nurse needed).
- [2] Get workload to cover for that room/shift.
- [3] Get minimum skill required for that room/shift.
- [4] Initialize best-candidate search.
- [5] Ignore nurses unavailable in that day/shift.
- [6] Compute projected load, overload, and skill deficit.
- [7] Build a score where skill is most important, then overload, then load balance, then deterministic tie-break.
- [8] Keep the best-scoring nurse.
- [9] Commit assignment and update nurse load state.

---

## 4) Results

### 4.1 Experimental setup

- Script: `assignment/run_all_tests.sh`
- Instance set: `assets/files/test/test01.json` ... `test10.json`
- Result folder used in this report:
  - `assignment/test_solutions_20260305_134058`
- CSV used:
  - `assignment/test_solutions_20260305_134058/validation_report.csv`

### 4.2 Aggregate metrics

- Tests executed: **10**
- Total admitted patients: **896 / 1308**
- Admission rate: **68.50%**
- Total hard violations (sum across tests): **0**
- Sum solver printed cost: **196,554**
- Sum official validator cost: **249,434**
- Highest validator cost: **test10.json**
- Lowest validator cost: **test02.json**

### 4.3 Per-test results

| Test | Admitted | Hard Violations | Solver Cost | Validator Cost |
|---|---:|---:|---:|---:|
| test01 | 29 / 42 | 0 | 3,995 | 4,429 |
| test02 | 32 / 37 | 0 | 3,167 | 3,373 |
| test03 | 15 / 45 | 0 | 11,319 | 11,875 |
| test04 | 51 / 54 | 0 | 2,387 | 5,475 |
| test05 | 23 / 62 | 0 | 16,729 | 17,171 |
| test06 | 70 / 111 | 0 | 25,235 | 26,090 |
| test07 | 67 / 113 | 0 | 21,070 | 20,017 |
| test08 | 67 / 173 | 0 | 30,037 | 30,820 |
| test09 | 99 / 146 | 0 | 22,417 | 29,127 |
| test10 | 443 / 525 | 0 | 60,198 | 101,057 |

### 4.4 Interpretation

Strengths:
- Hard constraints are satisfied on all 10 benchmark tests.
- Deterministic greedy pipeline with reproducible outputs.
- Scales to large test (`test10`) with full feasible schedule.

Limitations:
- Admission rate is still moderate on some instances (many optional patients remain unscheduled).
- Gap between solver internal cost and validator cost indicates objective mismatch in some cost components (notably nurse workload/continuity).

---

## 5) Conclusion

This project delivers a complete, modular, and working greedy baseline for IHTP with:
- robust end-to-end pipeline,
- automated benchmarking and validation,
- zero hard violations on the current 10-test benchmark set.

For further improvement while staying greedy, the next priority is to align the internal objective with validator cost terms and add stronger future-capacity-aware placement tie-breakers.
