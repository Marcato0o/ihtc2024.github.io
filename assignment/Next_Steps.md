# Next Steps

## 1) Prepare Data and Move JSON Logic

**Target files:** `IHTC_Data.hh`, `IHTC_Data.cc`

**Context**
- We are working on the IHTP (Integrated Healthcare Timetabling Problem) in C++.
- Goal: streamline `IHTC_Driver.cc` and encapsulate output logic.

**Critical rule**
- In `.hh` header files, write **only one-line function declarations**.
- Do **not** write function bodies in `.hh` files.
- All implementations must go in the corresponding `.cc` source files.

### Tasks

- [ ] **Task 1 — Update `IHTC_Data.hh`**
    - Inside `IHTC_Output`, add a flat data structure for nurse assignments:
        ```cpp
        struct NurseAssignment { int nurse_idx; int day; int shift; int room_idx; };
        std::vector<NurseAssignment> nurse_assignments;
        ```
    - Add exactly these two one-line declarations:
        ```cpp
        void writeJSON(const std::string& filename) const;
        void printCosts() const;
        ```

- [ ] **Task 2 — Implement `printCosts()` in `IHTC_Data.cc`**
    - Calculate and print all Soft costs:
        - Total
        - Unscheduled
        - Delay
        - OpenOT
        - AgeMix
        - Skill
        - Excess
        - Continuity
        - SurgeonTransfer

- [ ] **Task 3 — Implement `writeJSON(...)` in `IHTC_Data.cc`**
    - Move **all** JSON building logic (`nlohmann::ordered_json sol`) currently in `IHTC_Driver.cc` into this method.
    - Use internal state of `IHTC_Output`.
    - Write final output file while preserving the exact JSON structure required by the validator.

---

## 2) Implement Nurse Greedy Algorithm

**Target files:** `IHTC_Greedy.hh`, `IHTC_Greedy.cc`

**Context**
- Current greedy algorithm in `IHTC_Greedy.cc` solves PAS and SCP, but ignores Nurse-to-Room Assignment (NRA).

**Critical rules**
1. The algorithm must remain strictly **Greedy** (constructive only):
     - no backtracking
     - no local search
     - no external solvers
2. `.hh` files must contain only one-line declarations.

### Tasks

- [ ] **Task 1 — Update `IHTC_Greedy.hh`**
    - Add only this one-line declaration:
        ```cpp
        void assignNursesGreedy(const IHTC_Input& in, IHTC_Output& out);
        ```

- [ ] **Task 2 — Implement `assignNursesGreedy` in `IHTC_Greedy.cc`**
    - Iterate constructively by:
        - day
        - shift
        - room
    - If room is occupied for day/shift (Hard constraint H8), pick the best available nurse.

- [ ] **Task 3 — Define greedy selection rule**
    - Filter only nurses on shift (check roster).
    - Choose nurse minimizing soft-impact (examples):
        - lowest current workload (reduce S4)
        - closest skill match (reduce S2)
    - Save assignments in `out.nurse_assignments`.

- [ ] **Task 4 — Integrate into solver flow**
    - In `IHTC_Greedy.cc`, update `GreedyIHTCSolver` to call:
        ```cpp
        assignNursesGreedy(in, out);
        ```
        immediately after `scheduleInOrder`.

---

## 3) Clean Up the Driver

**Target file:** `IHTC_Driver.cc`

**Context**
- JSON export logic is now in `IHTC_Data.cc`.
- Nurse assignment logic (NRA) is in `IHTC_Greedy.cc`.
- Driver currently contains dead/disorganized logic.

### Task

- [ ] Rewrite `main` from scratch so it is lean and readable, with this exact sequence:

1. Parse command-line arguments (`input_file`).
2. Initialize `IHTC_Input in(argv[1]);` and handle loading errors.
3. Initialize `IHTC_Output out(in);`.
4. Print a brief data summary (patients, rooms, days).
5. Call solver engine: `GreedyIHTCSolver(in, out);`.
6. Call output methods:
     - `out.printCosts();`
     - `out.writeJSON("solution.json");`
7. Return `0`.

**Strict requirement**
- No calculations.
- No `json.push_back`.
- No algorithmic logic in driver.
- Driver must only orchestrate calls.