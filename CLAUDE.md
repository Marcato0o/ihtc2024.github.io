# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a C++17 greedy solver for the **Integrated Healthcare Timetabling Problem (IHTP)**, a combinatorial optimization problem combining:
- **PAS** – Patient Admission Scheduling
- **NRA** – Nurse-to-Room Assignment
- **SCP** – Surgical Case Planning

The goal is to minimize soft constraint penalty costs while satisfying all hard constraints. Constraint weights are instance-specific and embedded in input JSON files.

## Build

All source code is in [assignment/](assignment/).

```bash
cd assignment/
make            # builds IHTC_Test.exe
make clean      # removes objects and executable
```

Compiler: `g++ -Wall -Wfatal-errors -O3 -std=c++17`

## Run

```bash
cd assignment/
./IHTC_Test.exe <path/to/instance.json>
```

Outputs a cost breakdown to stdout and writes `solution.json` in the current directory. Test instances are in [assets/files/test/](assets/files/test/), public instances in [assets/files/instances/](assets/files/instances/).

## Validate

Compile the official validator once, then run it against any solution:

```bash
g++ -O2 -std=c++17 -o assets/files/IHTP_Validator assets/files/IHTP_Validator.cc
assets/files/IHTP_Validator <instance.json> <solution.json>
```

Reports total hard constraint violations and total soft cost.

## Run All Tests

```bash
cd assignment/
bash run_all_tests.sh
```

Runs the solver on all 10 test instances, auto-compiles the validator if missing, and writes a CSV report to `test_solutions_<timestamp>/validation_report.csv`.

The script accepts optional positional args:
```
run_all_tests.sh [solver_path] [input_dir] [output_dir] [validator_path]
```

## Architecture

### Data Layer (`IHTC_Data.hh/.cc`)
- Domain structs: `Patient`, `Room`, `OT`, `Nurse`, `Surgeon`, `Occupant`
- `IHTC_Input`: holds all parsed instance data
- `IHTC_Output`: holds the evolving solution; exposes `canAssignPatient()`, `assignPatient()`, `computeAllCosts()`, and `writeJSON()`

### JSON I/O (`json/parser.cc`, `json/writer.cc`)
- Uses the header-only [nlohmann/json](assignment/nlohmann/json.hpp) library
- `parser.cc` populates `IHTC_Input` from a JSON file
- `writer.cc` serializes `IHTC_Output` to `solution.json`

### Solver (`IHTC_Greedy.hh/.cc`)
Three-phase greedy pipeline invoked via `GreedySolver::runFullSolver()`:
1. **`solvePASandSCP()`** – orders patients (mandatory first, then by urgency/due date/window), greedily assigns admission day + room + operating theater using feasibility counts and marginal soft-cost evaluation (S1, S5, S6, S7)
2. **`solveNRA()`** – iterates over all occupied shifts, assigns nurses while respecting roster/workload limits and minimizing skill deficits (S2) and continuity violations (S3)

### Entry Point (`IHTC_Driver.cc`)
Parses CLI args, creates `IHTC_Input`/`IHTC_Output`, calls the solver, prints cost breakdown, writes `solution.json`.

## Constraints Reference

**Hard (H1–H8):** gender separation, room compatibility, surgeon/OT daily capacity, mandatory admission, admission window, room capacity, nurse coverage.

**Soft (S1–S8, weights in instance JSON):** age mix, nurse skill, continuity of care, nurse workload, open OTs per day, surgeon theater transfers, admission delay, unscheduled optional patients.
