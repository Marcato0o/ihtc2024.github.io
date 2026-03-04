**Title:** C++ Encapsulation: IHTC_Data.hh

**Context:** I am refactoring my C++ header file `IHTC_Data.hh` for the IHTP problem. I want to strictly enforce Object-Oriented Programming (OOP) encapsulation principles and code organization.

**Task:** Rewrite the provided `IHTC_Data.hh` code according to these objective architectural criteria:

**1. Class Conversion & Layout:**
* Both `IHTC_Input` and `IHTC_Output` must be declared as `class` (not `struct`). 
* Inside each class, place the `public:` block first, followed by the `private:` block.

**2. Positioning of Structs (Domain vs State):**
* The domain model structs (`Patient`, `Room`, `Nurse`, `Surgeon`, `Occupant`, and `OT`) describe the global entities of the problem. Define them ALL **outside** the classes at the top of the file. Extract the `OT` struct from inside `IHTC_Input` and place it alongside the others.
* The `NurseAssignment` struct is specific to the output state tracking. Keep it nested **inside** `IHTC_Output`, but move it into the `private:` section.

**3. Encapsulation Rules (Public vs Private):**
* **For `IHTC_Input`**: This acts as a Read-Only Data Transfer Object (DTO). Keep the data vectors and constants (`patients`, `rooms`, `nurses`, `surgeons`, `occupants`, `ots`, `D`, `shifts_per_day`, `weights`) `public` so the greedy solver can read them efficiently without getters. Move internal parsing artifacts (like `raw_json_text`) to the `private` section.
* **For `IHTC_Output`**: This class manages complex mutating state. **ALL** of its state vectors (`admitted`, `admit_day`, `room_assigned_idx`, `ot_assigned_idx`, `nurse_assignments`, `room_occupancy`, `ot_minutes_used`, `surgeon_minutes_used`, `room_gender`, and the pointer `bound_input`) MUST be moved to the `private:` section to prevent external mutation and protect invariants. The external world must only interact through the `public:` methods (`init`, `canAssignPatient`, `assignPatient`, cost computers, `writeJSON`, `printCosts`, etc.).

**Constraint:** Output ONLY the newly refactored C++ code for `IHTC_Data.hh`. Keep all function declarations as one-liners without implementations. Do not modify the function signatures.

**Title:** Restructure IHTC_Greedy: Strict Header Rules and Explicit PAS/SCP/NRA Phases

**Context:** I am refactoring `IHTC_Greedy.hh` and `IHTC_Greedy.cc`. I need to strictly enforce that `.hh` files contain ONLY one-line declarations, and `.cc` files contain all implementations. Also, I want the logic to explicitly reflect the domain sub-problems: PAS (Patient Admission), SCP (Surgical Case Planning), and NRA (Nurse-to-Room Assignment).

**Algorithmic Constraint:** Since this is a strictly Greedy algorithm (no backtracking allowed), PAS and SCP must be solved *jointly* per patient to avoid dead-ends (e.g., picking a room/day where no Operating Theater is available). NRA must be solved sequentially *after* all patients are placed.

**Task 1: Update `IHTC_Greedy.hh`**
* Declare a namespace `GreedySolver`.
* Inside it, provide ONLY these three one-line function declarations:
  1. `void solvePASandSCP(const IHTC_Input& in, IHTC_Output& out);` 
  2. `void solveNRA(const IHTC_Input& in, IHTC_Output& out);` 
  3. `void runFullSolver(const IHTC_Input& in, IHTC_Output& out);`
* **CRITICAL:** Do NOT write any function bodies in the `.hh` file.

**Task 2: Update `IHTC_Greedy.cc`**
* Include the header.
* Move all the internal helper functions (like `horizonDays`, `sortPatientsByPriority`, `evaluatePlacementCost`, `schedulePatient`) into an anonymous namespace `namespace { ... }` at the top of the file so they are completely hidden from the global scope.
* Implement `GreedySolver::solvePASandSCP`: This function should sort the patients and iterate over them to find the joint valid tuple (Day, Room, OT). This effectively replaces your old `scheduleInOrder` logic.
* Implement `GreedySolver::solveNRA`: Move your nurse assignment logic here.
* Implement `GreedySolver::runFullSolver` to simply act as the orchestrator:
  ```cpp
  void GreedySolver::runFullSolver(const IHTC_Input& in, IHTC_Output& out) {
      std::cout << "[SOLVER] Starting greedy solver..." << std::endl;
      solvePASandSCP(in, out);
      solveNRA(in, out);
      std::cout << "[SOLVER] Greedy pass finished." << std::endl;
  }