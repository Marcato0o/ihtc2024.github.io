#!/usr/bin/env bash
set -euo pipefail

SOLVER_PATH="${1:-}"
INPUT_DIR="${2:-}"
OUTPUT_DIR="${3:-}"
VALIDATOR_PATH="${4:-}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

if [[ -z "$INPUT_DIR" ]]; then
  INPUT_DIR="$REPO_ROOT/assets/files/test"
fi

if [[ -z "$OUTPUT_DIR" ]]; then
  OUTPUT_DIR="$SCRIPT_DIR/test_solutions_$(date +%Y%m%d_%H%M%S)"
fi

if [[ -z "$VALIDATOR_PATH" ]]; then
  VALIDATOR_PATH="$REPO_ROOT/assets/files/IHTP_Validator"
fi
VALIDATOR_SRC="$REPO_ROOT/assets/files/IHTP_Validator.cc"

if [[ -z "$SOLVER_PATH" ]]; then
  if [[ -x "$SCRIPT_DIR/IHTC_Test" ]]; then
    SOLVER_PATH="$SCRIPT_DIR/IHTC_Test"
  elif [[ -x "$SCRIPT_DIR/IHTC_Test.exe" ]]; then
    SOLVER_PATH="$SCRIPT_DIR/IHTC_Test.exe"
  elif [[ -f "$SCRIPT_DIR/IHTC_Test.exe" ]]; then
    SOLVER_PATH="$SCRIPT_DIR/IHTC_Test.exe"
  fi
fi

if [[ -z "$SOLVER_PATH" || ! -f "$SOLVER_PATH" ]]; then
  echo "Solver executable not found. Build it first or pass solver path as arg1." >&2
  exit 2
fi

if [[ ! -d "$INPUT_DIR" ]]; then
  echo "Input directory not found: $INPUT_DIR" >&2
  exit 2
fi

if [[ ! -x "$VALIDATOR_PATH" ]]; then
  if [[ -f "$VALIDATOR_SRC" ]]; then
    echo "Validator not executable; compiling: $VALIDATOR_SRC"
    g++ -O2 -std=c++17 -o "$VALIDATOR_PATH" "$VALIDATOR_SRC"
    chmod +x "$VALIDATOR_PATH" || true
  else
    echo "Validator not found: $VALIDATOR_PATH" >&2
    exit 2
  fi
fi

mapfile -t TESTS < <(find "$INPUT_DIR" -maxdepth 1 -type f -name 'test*.json' | sort)
if [[ ${#TESTS[@]} -eq 0 ]]; then
  echo "No test instances found in: $INPUT_DIR" >&2
  exit 2
fi

mkdir -p "$OUTPUT_DIR"
VALIDATION_CSV="$OUTPUT_DIR/validation_report.csv"
echo "test,solution,patients,rooms,days,solver_started,admitted,admitted_total,solver_finished,solver_cost,unscheduled,delay,open_ot,age_mix,skill,excess,continuity,surgeon_transfer,total_violations,total_cost,validator_exit" > "$VALIDATION_CSV"

echo "Solver:     $SOLVER_PATH"
echo "Validator:  $VALIDATOR_PATH"
echo "Input dir:  $INPUT_DIR"
echo "Output dir: $OUTPUT_DIR"
echo "Tests:      ${#TESTS[@]}"

passed=0
failed=0
failed_tests=()
hard_violation_cases=0
validator_failed=0

pushd "$SCRIPT_DIR" >/dev/null

for test_file in "${TESTS[@]}"; do
  test_name="$(basename "$test_file")"
  instance_name="${test_name%.json}"
  temp_solution="$SCRIPT_DIR/solution.json"

  rm -f "$temp_solution"

  echo "[RUN] $test_name"
  solver_log="$OUTPUT_DIR/solver_${instance_name}.txt"
  if ! "$SOLVER_PATH" "$test_file" > "$solver_log" 2>&1; then
    cat "$solver_log"
    echo "[FAIL] solver failed on $test_name" >&2
    failed=$((failed + 1))
    failed_tests+=("$test_name")
    continue
  fi
  cat "$solver_log"

  if [[ ! -f "$temp_solution" ]]; then
    echo "[FAIL] missing solution.json after $test_name" >&2
    failed=$((failed + 1))
    failed_tests+=("$test_name")
    continue
  fi

  target_solution="$OUTPUT_DIR/sol_${instance_name}.json"
  mv "$temp_solution" "$target_solution"

  validator_log="$OUTPUT_DIR/validator_${instance_name}.txt"
  if "$VALIDATOR_PATH" "$test_file" "$target_solution" > "$validator_log" 2>&1; then
    validator_exit=0
  else
    validator_exit=$?
    validator_failed=$((validator_failed + 1))
  fi

  total_violations="$(sed -n 's/^Total violations = \([0-9][0-9]*\).*/\1/p' "$validator_log" | tail -n 1)"
  total_cost="$(sed -n 's/^Total cost = \([0-9][0-9]*\).*/\1/p' "$validator_log" | tail -n 1)"

  if [[ -z "$total_violations" ]]; then
    total_violations="NA"
  fi
  if [[ -z "$total_cost" ]]; then
    total_cost="NA"
  fi

  if [[ "$total_violations" != "NA" ]] && (( total_violations > 0 )); then
    hard_violation_cases=$((hard_violation_cases + 1))
  fi

  patients="$(sed -n 's/^  patients:[[:space:]]*\([0-9][0-9]*\).*/\1/p' "$solver_log" | tail -n 1)"
  rooms="$(sed -n 's/^  rooms:[[:space:]]*\([0-9][0-9]*\).*/\1/p' "$solver_log" | tail -n 1)"
  days="$(sed -n 's/^  days D:[[:space:]]*\([0-9][0-9]*\).*/\1/p' "$solver_log" | tail -n 1)"
  solver_started="$(grep -c '^\[SOLVER\] Starting greedy solver\.\.\.$' "$solver_log" || true)"
  solver_finished="$(grep -c '^\[SOLVER\] Greedy pass finished\.$' "$solver_log" || true)"

  admitted_pair="$(sed -n 's/^\[SOLVER\] Patients admitted: \([0-9][0-9]*\)\/\([0-9][0-9]*\).*/\1,\2/p' "$solver_log" | tail -n 1)"
  admitted="NA"
  admitted_total="NA"
  if [[ -n "$admitted_pair" ]]; then
    admitted="${admitted_pair%%,*}"
    admitted_total="${admitted_pair##*,}"
  fi

  cost_pair="$(sed -n 's/^Cost: \([0-9][0-9]*\), Unscheduled: \([0-9][0-9]*\),[[:space:]]*Delay: \([0-9][0-9]*\),[[:space:]]*OpenOT: \([0-9][0-9]*\),[[:space:]]*AgeMix: \([0-9][0-9]*\),[[:space:]]*Skill: \([0-9][0-9]*\),[[:space:]]*Excess: \([0-9][0-9]*\),[[:space:]]*Continuity: \([0-9][0-9]*\),[[:space:]]*SurgeonTransfer: \([0-9][0-9]*\).*/\1,\2,\3,\4,\5,\6,\7,\8,\9/p' "$solver_log" | tail -n 1)"
  solver_cost="NA"
  unscheduled="NA"
  delay="NA"
  open_ot="NA"
  age_mix="NA"
  skill="NA"
  excess="NA"
  continuity="NA"
  surgeon_transfer="NA"
  if [[ -n "$cost_pair" ]]; then
    IFS=',' read -r solver_cost unscheduled delay open_ot age_mix skill excess continuity surgeon_transfer <<< "$cost_pair"
  fi

  if [[ -z "$patients" ]]; then patients="NA"; fi
  if [[ -z "$rooms" ]]; then rooms="NA"; fi
  if [[ -z "$days" ]]; then days="NA"; fi

  echo "$test_name,$(basename "$target_solution"),$patients,$rooms,$days,$solver_started,$admitted,$admitted_total,$solver_finished,$solver_cost,$unscheduled,$delay,$open_ot,$age_mix,$skill,$excess,$continuity,$surgeon_transfer,$total_violations,$total_cost,$validator_exit" >> "$VALIDATION_CSV"
  passed=$((passed + 1))
done

popd >/dev/null

echo
echo "Done. Passed: $passed, Failed: $failed"
echo "Validator failures: $validator_failed"
echo "Cases with hard violations > 0: $hard_violation_cases"
echo "Solutions written to: $OUTPUT_DIR"
echo "Validation report: $VALIDATION_CSV"

if [[ $failed -gt 0 ]]; then
  echo "Failed tests: ${failed_tests[*]}" >&2
  exit 1
fi

exit 0
