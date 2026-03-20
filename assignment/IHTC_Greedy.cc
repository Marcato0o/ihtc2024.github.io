#include "IHTC_Greedy.hh"

#include <algorithm>
#include <iostream>
#include <climits>
#include "nlohmann/json.hpp"

namespace { // internal helpers

// ---------------------------------------------------------------------------
// sortPatientsByPriority — rank all patients by static scheduling priority.
//
// Used as the fallback tiebreaker in solvePASandSCP's dynamic loop; the actual
// order is primarily driven by feasibility counts at each step.
//
// Priority (highest → lowest): mandatory before optional; then among mandatory:
// earlier due_date, earlier release_date, narrower window (due-release). Shared
// by all: longer length_of_stay, longer surgery_time.
// ---------------------------------------------------------------------------
std::vector<int> sortPatientsByPriority(const IHTC_Input& in) {
    // Build an index vector [0, 1, ..., N-1] and sort it by the comparator.
    std::vector<int> p_ids(in.patients.size());
    for (int i = 0; i < (int)in.patients.size(); ++i) p_ids[i] = i;

    std::sort(p_ids.begin(), p_ids.end(), [&](int a, int b) {
        const Patient &pa = in.patients[a];
        const Patient &pb = in.patients[b];

        // Criterion 1: mandatory patients always precede optional ones.
        if (pa.mandatory != pb.mandatory) return pa.mandatory > pb.mandatory;

        // Criteria 2–4 apply only when both patients are mandatory.
        if (pa.mandatory) {
            // Criterion 2: tighter deadline first.
            if (pa.due_date != pb.due_date) return pa.due_date < pb.due_date;
            // Criterion 3: available sooner first (same deadline).
            if (pa.release_date != pb.release_date) return pa.release_date < pb.release_date;
            // Criterion 4: narrower admission window first.
            int window_a = pa.due_date - pa.release_date;
            int window_b = pb.due_date - pb.release_date;
            if (window_a != window_b) return window_a < window_b;
        }

        // Criteria 5–6: shared by mandatory and optional.
        // Criterion 5: longer stay first — blocks more future bed-days.
        if (pa.length_of_stay != pb.length_of_stay) return pa.length_of_stay > pb.length_of_stay;
        // Criterion 6: longer surgery first — harder to fit into OT capacity.
        return pa.surgery_time > pb.surgery_time;
    });

    return p_ids;
}

// ---------------------------------------------------------------------------
// evaluatePlacementCost — marginal soft cost of placing patient_id at (day, room, ot).
//
// Computes only costs that depend on placement:
//   S7 — days of delay past release_date
//   S5 — first use of an OT on a day (opening penalty)
//   S1 — increase in (max-min) age-group range in the room, summed over stay
//   S6 — surgeon already using a different OT today (transfer penalty)
//
// S2/S3/S4 (skill, continuity, workload) are excluded — they depend on nurse
// assignments that are unknown until Phase 2.
// ---------------------------------------------------------------------------
int evaluatePlacementCost(const IHTC_Input& in, const IHTC_Output& out, int patient_id, int day, int room_id, int ot_id) {
    int cost = 0;
    const Patient& p = in.patients[patient_id];

    // S7: every day of delay past the release date contributes w_patient_delay.
    cost += (day - p.release_date) * in.w_patient_delay;

    // S5: if the OT has not been used at all today (remaining capacity ==
    // original capacity), opening it now incurs the open-OT penalty.
    if (ot_id >= 0) {
        int max_cap = in.ots[ot_id].availability[day];
        if (out.getOtAvailability(ot_id, day) == max_cap) {
            cost += in.w_open_operating_theater;
        }
    }

    // S1: for each day of the patient's stay, compute the increase in the
    // (max_age - min_age) range that this patient's age_group would cause.
    // getRoomAgeMixMarginal returns the delta; multiply by the weight.
    int los = p.length_of_stay;
    for (int dd = 0; dd < los; ++dd) {
        int d_stay = day + dd;
        if (d_stay >= in.D) break;
        cost += out.getRoomAgeMixMarginal(room_id, d_stay, p.age_group) * in.w_room_mixed_age;
    }

    // S6: if this surgeon already has a patient in a *different* OT on `day`,
    // assigning another patient here creates a surgeon-transfer event.
    if (p.surgeon_idx >= 0 && ot_id >= 0) {
        if (out.surgeonHasOtherOTOnDay(p.surgeon_idx, day, ot_id))
            cost += in.w_surgeon_transfer;
    }

    return cost;
}

// ---------------------------------------------------------------------------
// schedulePatient — find and commit the cheapest feasible (day, room, OT) for patient_id.
//
// Scans all (day ∈ [release_date..D-1], room, OT) triples; mandatory patients
// cap the window at due_date. Patients with no surgery use ot = -1 (one iteration).
// Commits via assignPatient(); returns false if no feasible slot exists.
// ---------------------------------------------------------------------------
bool schedulePatient(const IHTC_Input& in, IHTC_Output& out, int patient_id) {
    const Patient& p = in.patients[patient_id];

    // Track the best (day, room, OT) triple found so far.
    int best_day = -1;
    int best_room = -1;
    int best_ot = -1;
    int min_cost = INT_MAX;

    // Admission window boundaries.
    int start_d = p.release_date;
    int end_d = in.D - 1;
    if (p.mandatory) end_d = p.due_date;

    bool needs_ot = p.surgery_time > 0;
    if (needs_ot && in.ots.empty()) return false; // surgery needed but no OTs available

    // Exhaustive search: day × room × OT.
    for (int d = start_d; d <= end_d; ++d) {
        for (int r = 0; r < (int)in.rooms.size(); ++r) {
            // When surgery is not needed, the inner loop runs once with ot = -1.
            int ot_start = needs_ot ? 0 : -1;
            int ot_end   = needs_ot ? (int)in.ots.size() - 1 : -1;
            for (int ot = ot_start; ot <= ot_end; ++ot) {
                // Hard-constraint check: skip infeasible placements.
                if (!out.canAssignPatient(patient_id, d, r, ot, in)) continue;

                // Soft-constraint cost of this placement.
                int current_cost = evaluatePlacementCost(in, out, patient_id, d, r, ot);
                if (current_cost < min_cost) {
                    min_cost = current_cost;
                    best_day = d; best_room = r; best_ot = ot;
                    // Cost can't go below 0 — exit all loops immediately.
                    if (min_cost == 0) break;
                }
            }
            if (min_cost == 0) break;
        }
        if (min_cost == 0) break;
    }

    // Commit the best placement found (if any).
    if (best_day != -1) {
        out.assignPatient(patient_id, best_day, best_room, best_ot, in);
        return true;
    }
    return false; // no feasible placement exists for this patient
}

// ---------------------------------------------------------------------------
// countFeasiblePlacements — count (day, room, OT) triples that pass canAssignPatient().
//
// Returns early once the count exceeds stop_after: solvePASandSCP only needs to
// know whether a patient has fewer options than the current best, so counting
// beyond that threshold is wasted work.
// ---------------------------------------------------------------------------
int countFeasiblePlacements(const IHTC_Input& in, const IHTC_Output& out, int patient_id, int stop_after) {
    const Patient& p = in.patients[patient_id];

    int start_d = p.release_date;
    int end_d = in.D - 1;
    if (p.mandatory) end_d = p.due_date;

    bool needs_ot = p.surgery_time > 0;
    if (needs_ot && in.ots.empty()) return 0;

    int feasible = 0;
    for (int d = start_d; d <= end_d; ++d) {
        for (int r = 0; r < (int)in.rooms.size(); ++r) {
            int ot_start = needs_ot ? 0 : -1;
            int ot_end   = needs_ot ? (int)in.ots.size() - 1 : -1;
            for (int ot = ot_start; ot <= ot_end; ++ot) {
                if (!out.canAssignPatient(patient_id, d, r, ot, in)) continue;
                ++feasible;
                // Early exit: already know this patient cannot be "more constrained"
                // than the current best, so further counting is useless.
                if (feasible > stop_after) return feasible;
            }
        }
    }
    return feasible;
}

// ---------------------------------------------------------------------------
// seedOccupants — project all pre-existing occupants into the output state.
//
// Calls seedOccupantStay() for each occupant so room_occupancy, room_gender,
// and the age-mix cache reflect the real state at day 0 before any new patient
// is placed. Must run before the scheduling loop.
// ---------------------------------------------------------------------------
void seedOccupants(const IHTC_Input& in, IHTC_Output& out) {
    for (const auto& f : in.occupants)
        out.seedOccupantStay(f.room_idx, f.length_of_stay, f.sex, f.age_group);
}

} // namespace

namespace GreedySolver {

// ---------------------------------------------------------------------------
// solvePASandSCP — Phase 1: assign each patient a (day, room, OT) triple.
//
// Dynamic greedy — one step per patient:
//   1. Pick the unscheduled patient with the fewest feasible (day, room, OT)
//      options. Mandatory patients always precede optional; ties broken by
//      static priority rank (sortPatientsByPriority).
//   2. Place it in the cheapest valid slot (minimum marginal soft cost).
//
// Feasibility is re-counted after every assignment because each placement can
// shrink the option set of remaining patients. Always serving the most
// constrained patient first prevents it from being left with no valid slot.
// ---------------------------------------------------------------------------
void solvePASandSCP(const IHTC_Input& in, IHTC_Output& out) {
    // Seed occupants so room constraints reflect the state at day 0.
    seedOccupants(in, out);

    // Static priority order: mandatory first, then earlier due date, longer stay, etc.
    // Computed once; never updated. Drives the iteration sequence
    // and acts as a tiebreaker when two patients have equal feasibility.
    std::vector<int> order = sortPatientsByPriority(in);

    // Inverse of order: base_rank[pid] = position in order. Precomputed so the
    // selection loop can compare ranks in O(1) instead of O(n) per patient.
    std::vector<int> base_rank(in.patients.size(), 0);
    for (int i = 0; i < (int)order.size(); ++i) base_rank[order[i]] = i;

    // done[pid] = true once a scheduling decision has been made for pid.
    std::vector<bool> done(in.patients.size(), false);

    int admitted_count  = 0;
    int mandatory_failed = 0;

    // One step per patient — each step picks and schedules exactly one patient.
    for (int step = 0; step < (int)in.patients.size(); ++step) {
        // Variables that track the best candidate found in this step.
        int best_pid            = -1;
        bool best_mandatory     = false;
        int best_feasible_count = INT_MAX; // feasibility count of best_pid
        int best_rank           = INT_MAX;

        for (int pid : order) {
            if (done[pid]) continue;
            bool is_mandatory = in.patients[pid].mandatory;

            // Early-exit optimisation: a patient with more options than best_feasible_count
            // cannot win on criterion 3, so stop counting beyond that threshold.
            // INT_MAX means no candidate yet — count fully.
            int feasible_cap = (best_feasible_count == INT_MAX) ? INT_MAX : best_feasible_count;
            int feasible = countFeasiblePlacements(in, out, pid, feasible_cap);
            int rank     = base_rank[pid];

            // Lexicographic comparison: is pid better than the current best?
            bool better = false;
            // 1. No candidate yet — take the first one unconditionally.
            if      (best_pid == -1)                  better = true;
            // 2. Mandatory status: mandatory always beats optional.
            else if (is_mandatory != best_mandatory)  better = is_mandatory;
            // 3. Fewest feasible options first (most constrained patient).
            else if (feasible != best_feasible_count) better = (feasible < best_feasible_count);
            // 4. Static priority rank as final tiebreaker.
            else if (rank < best_rank)                better = true;

            if (better) {
                best_pid            = pid;
                best_mandatory      = is_mandatory;
                best_feasible_count = feasible;
                best_rank           = rank;
            }
        }

        // No unscheduled patient remains (or all have feasibility 0 — handled
        // by schedulePatient returning false below).
        if (best_pid == -1) break;
        done[best_pid] = true;

        if (schedulePatient(in, out, best_pid))
            admitted_count++;
        else if (in.patients[best_pid].mandatory)
            mandatory_failed++;       // hard constraint violation
        else
            out.markOptionalUnscheduled(); // increments S8 cache
    }

    std::cout << "[SOLVER] Patients admitted: " << admitted_count << "/" << in.patients.size() << std::endl;
    if (mandatory_failed > 0) {
        std::cerr << "[H5 VIOLATION] Mandatory patients not admitted: " << mandatory_failed << std::endl;
    }
}

// ---------------------------------------------------------------------------
// solveNRA — Phase 2: assign one nurse to every occupied (day, shift, room) triple.
//
// Nurse assignments do not affect H1–H7 (fixed by Phase 1), so this phase
// runs independently after solvePASandSCP.
//
// Three steps:
//   1. Build nurse availability + capacity tables (from working_shifts).
//   2. Build per-room demand tables (total load and max required skill per shift).
//   3. For each occupied slot, pick the best available nurse lexicographically:
//      skill gap → overload → projected load → nurse index.
//
// All working arrays are flat vectors with lambda indices to keep memory
// contiguous and avoid nested-vector overhead.
// ---------------------------------------------------------------------------
void solveNRA(const IHTC_Input& in, IHTC_Output& out) {
    int days        = in.D;
    int shifts      = in.shifts_per_day;
    int room_count  = (int)in.rooms.size();
    int nurse_count = (int)in.nurses.size();

    out.clearNurseAssignments();

    // Linear index lambdas for flat arrays.
    // Using the same naming convention as computeAllCosts() in IHTC_Data.cc.
    auto nds_idx = [&](int n, int d, int s) { return (n * days + d) * shifts + s; };       // nurse × day × shift
    auto dr_idx  = [&](int d, int r)        { return d * room_count + r; };                  // day × room
    auto dsr_idx = [&](int d, int s, int r) { return (d * shifts + s) * room_count + r; };  // day × shift × room
    auto dsn_idx = [&](int d, int s, int n) { return (d * shifts + s) * nurse_count + n; }; // day × shift × nurse

    // -----------------------------------------------------------------------
    // Step 1 — Nurse availability and shift capacity tables.
    //
    // nurse_available[nds_idx(n,d,s)] = true if nurse n works shift s on day d.
    // nurse_shift_cap[nds_idx(n,d,s)] = maximum workload units for that shift.
    // -----------------------------------------------------------------------
    std::vector<bool> nurse_available(nurse_count * days * shifts, false);
    std::vector<int>  nurse_shift_cap(nurse_count * days * shifts, 0);

    for (int n = 0; n < nurse_count; ++n) {
        for (const auto& ws : in.nurses[n].working_shifts) {
            int d = ws.day, s = ws.shift;
            nurse_available[nds_idx(n,d,s)] = true;
            nurse_shift_cap[nds_idx(n,d,s)] = ws.max_load;
        }
    }

    // -----------------------------------------------------------------------
    // Step 2 — Room demand tables.
    //
    // room_occupied[dr_idx(d,r)]       = true if room r has ≥1 patient on day d.
    // room_shift_load[dsr_idx(d,s,r)]  = total nursing workload units for room r
    //                                   during shift s on day d.
    // room_shift_skill[dsr_idx(d,s,r)] = maximum skill level required by any
    //                                   patient in room r during shift s on day d.
    //
    // Both occupants and admitted patients use the same per-shift indexing:
    //   idx = dd * shifts + s, where dd is days elapsed since admission.
    // Occupants have start_day=0; patients start at their admit_day.
    // -----------------------------------------------------------------------
    std::vector<bool> room_occupied(days * room_count, false);
    std::vector<int>  room_shift_load(days * shifts * room_count, 0);
    std::vector<int>  room_shift_skill(days * shifts * room_count, 0);

    auto add_stay = [&](int ridx, int start_day, int los,
                         const std::vector<int> &load_ps,
                         const std::vector<int> &skill_ps) {
        for (int dd = 0; dd < los; ++dd) {
            int d = start_day + dd;
            if (d >= days) break;
            room_occupied[dr_idx(d, ridx)] = true;
            for (int s = 0; s < shifts; ++s) {
                int idx = dd * shifts + s;
                room_shift_load[dsr_idx(d,s,ridx)]  += load_ps[idx];
                room_shift_skill[dsr_idx(d,s,ridx)] = std::max(room_shift_skill[dsr_idx(d,s,ridx)], skill_ps[idx]);
            }
        }
    };

    for (const auto& o : in.occupants)
        add_stay(o.room_idx, 0, o.length_of_stay,
                 o.nurse_load_per_shift, o.skill_level_required_per_shift);

    for (int pid = 0; pid < (int)in.patients.size(); ++pid) {
        if (!out.isAdmitted(pid)) continue;
        add_stay(out.getRoomAssignedIdx(pid), out.getAdmitDay(pid),
                 in.patients[pid].length_of_stay,
                 in.patients[pid].nurse_load_per_shift,
                 in.patients[pid].skill_level_required_per_shift);
    }

    // -----------------------------------------------------------------------
    // Step 3 — Nurse selection loop.
    //
    // For every occupied (day, shift, room), select the best available nurse.
    // nurse_load[dsn_idx(d,s,n)] tracks how much workload nurse n has already
    // accumulated on shift s of day d (from rooms assigned earlier in this loop).
    //
    // Lexicographic selection criterion (hardest consequence first):
    //   1. skill_gap  = max(0, req_skill - nurse.level)
    //                  Smallest gap first — an underqualified nurse directly
    //                  increases S2 cost; skill cannot be fixed later.
    //   2. overload   = max(0, projected_load - shift_cap)
    //                  Smallest overload first — exceeding the cap increases
    //                  S4 cost; prefer a nurse who fits within their limit.
    //   3. projected  = current_load + room_demand
    //                  Smallest projected load — balances workload across
    //                  nurses (soft proxy for S3/S4 health).
    //   4. nurse index — stable tiebreaker for deterministic output.
    // -----------------------------------------------------------------------
    std::vector<int> nurse_load(days * shifts * nurse_count, 0);

    for (int d = 0; d < days; ++d) {
        for (int s = 0; s < shifts; ++s) {
            for (int r = 0; r < room_count; ++r) {
                // Skip rooms that have no patients on this day.
                if (!room_occupied[dr_idx(d,r)]) continue;

                int demand    = room_shift_load[dsr_idx(d,s,r)];
                int req_skill = room_shift_skill[dsr_idx(d,s,r)];

                // Track best candidate found so far for this (day, shift, room).
                int best_nurse     = -1;
                int best_skill_gap = INT_MAX;
                int best_overload  = INT_MAX;
                int best_projected = INT_MAX;

                for (int n = 0; n < nurse_count; ++n) {
                    if (!nurse_available[nds_idx(n,d,s)]) continue;

                    int cur       = nurse_load[dsn_idx(d,s,n)];
                    int projected = cur + demand;
                    int cap       = nurse_shift_cap[nds_idx(n,d,s)];
                    int overload  = std::max(0, projected - cap);
                    int skill_gap = std::max(0, req_skill - in.nurses[n].level);

                    bool better = false;
                    if      (best_nurse == -1)              better = true;                         // 1. no candidate yet
                    else if (skill_gap != best_skill_gap)   better = (skill_gap < best_skill_gap); // 2. smallest skill gap
                    else if (overload != best_overload)     better = (overload < best_overload);   // 3. smallest overload
                    else if (projected != best_projected)   better = (projected < best_projected); // 4. lightest load
                    else                                    better = (n < best_nurse);             // 5. stable tiebreaker

                    if (better) {
                        best_nurse     = n;
                        best_skill_gap = skill_gap;
                        best_overload  = overload;
                        best_projected = projected;
                    }
                }

                if (best_nurse >= 0) {
                    // Accumulate load on the chosen nurse for this shift, then record.
                    nurse_load[dsn_idx(d,s,best_nurse)] += demand;
                    out.addNurseAssignment(best_nurse, d, s, r);
                } else {
                    std::cerr << "[H8 VIOLATION] No nurse available for day=" << d
                              << " shift=" << s << " room=" << r << "\n";
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// runFullSolver — top-level entry point: run Phase 1 then Phase 2.
//
// Calls solvePASandSCP (patient → day/room/OT) then solveNRA (nurse → room/shift).
// The phases are sequential and non-interacting: nurse assignments have no effect
// on patient placement decisions.
// ---------------------------------------------------------------------------
void runFullSolver(const IHTC_Input& in, IHTC_Output& out) {
    std::cout << "[SOLVER] Starting greedy solver..." << std::endl;
    solvePASandSCP(in, out);
    solveNRA(in, out);
    std::cout << "[SOLVER] Greedy pass finished." << std::endl;
}

} // namespace GreedySolver
