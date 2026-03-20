#include "IHTC_Data.hh"
#include "json/io.hh"
#include <algorithm>
#include <iostream>
#include <set>
#include <cassert>
#include <climits>

using namespace std;

// =============================================================================
// IHTC_Input — constructors and accessors
// =============================================================================

IHTC_Input::IHTC_Input() = default;

// Convenience constructor: parses a JSON file directly.
IHTC_Input::IHTC_Input(const std::string &file_name) {
    loadInstance(file_name);
}

const std::string &IHTC_Input::getRawJsonText() const {
    return raw_json_text;
}

// =============================================================================
// IHTC_Output — construction and initialization
// =============================================================================

// Constructor: store a pointer to the problem instance (never null after this)
// and run init() to allocate and zero all solution arrays.
// bound_input is kept as a raw pointer because IHTC_Output must not outlive
// the IHTC_Input it was constructed from.
IHTC_Output::IHTC_Output(const IHTC_Input &in) {
    bound_input = &in;
    init(in);
}

// ---------------------------------------------------------------------------
// init — allocate and zero all mutable solution arrays; copy problem capacities.
//
// Called once at the start of solvePASandSCP(). All subsequent mutations go
// through assignPatient(), seedOccupantStay(), and addNurseAssignment().
//
// Capacity arrays (ot_availability, surgeon_availability) are pre-loaded from the
// problem maximums and decremented per assignment. Cost caches start at 0 and
// accumulate incrementally so computeAllCosts() only multiplies by weights.
// ---------------------------------------------------------------------------
void IHTC_Output::init(const IHTC_Input &in) {
    int days = in.D > 0 ? in.D : 1; // guard against degenerate inputs

    // Per-patient admission records (all false / -1 = not yet admitted).
    admitted.assign(in.patients.size(), false);
    admit_day.assign(in.patients.size(), -1);
    room_assigned_idx.assign(in.patients.size(), -1);
    ot_assigned_idx.assign(in.patients.size(), -1);

    nurse_assignments.clear();

    // room_occupancy[room][day] = number of beds occupied.
    // Starts at 0; canAssignPatient() rejects if this reaches room capacity.
    room_occupancy.assign(in.rooms.size(), std::vector<int>(days, 0));

    // ot_availability[ot][day] = remaining surgery minutes for OT on that day.
    // Copied from the instance's maximum capacity and decremented by assignPatient().
    ot_availability.resize(in.ots.size());
    for (size_t i = 0; i < in.ots.size(); ++i) {
        ot_availability[i] = in.ots[i].availability; // copy the max capacity
        ot_availability[i].resize(days, 0);           // ensure size == D (invariant always holds)
    }

    // surgeon_availability[surgeon][day] = remaining surgery minutes for that surgeon.
    // Same copy-and-decrement pattern as ot_availability.
    surgeon_availability.resize(in.surgeons.size());
    for (size_t i = 0; i < in.surgeons.size(); ++i) {
        surgeon_availability[i] = in.surgeons[i].max_surgery_time;
        surgeon_availability[i].resize(days, 0);
    }

    // room_gender[room][day] = Gender::NONE until the first patient locks it.
    room_gender.assign(in.rooms.size(), std::vector<Gender>(days, Gender::NONE));

    int n_rooms = (int)in.rooms.size();
    int n_ots   = (int)in.ots.size();

    // Flat arrays for age-mix tracking (S1).
    // Index: room * D + day.
    // INT_MAX / -1 means "no patient with a known age_group yet on this room/day".
    room_day_min_age.assign(n_rooms * days, INT_MAX);
    room_day_max_age.assign(n_rooms * days, -1);

    // Flat bool for surgeon-OT usage tracking (S6).
    // Index: (surgeon * D + day) * n_ots + ot.
    // true = this surgeon already used that OT on that day.
    surgeon_day_ot_used.assign(in.surgeons.size() * days * n_ots, false);

    // Incremental cost caches — accumulated during assignPatient() and
    // seedOccupantStay() so that computeAllCosts() reads them in O(1).
    cache_delay_raw        = 0; // S7: sum of (admit_day - release_date)
    cache_open_ot_count    = 0; // S5: count of (day, OT) pairs first opened
    cache_age_mix_raw      = 0; // S1: sum of (max-min) range increments
    cache_surgeon_xfer_raw = 0; // S6: count of surgeon OT-transfer events
    cache_unscheduled_raw  = 0; // S8: count of optional patients not admitted
}

// =============================================================================
// Hard-constraint checking
// =============================================================================

// ---------------------------------------------------------------------------
// canAssignPatient — true iff placing patient_id at (day, room_idx, ot_idx) satisfies all hard constraints.
//
// Short-circuits on the first violation, checked cheapest first:
//   H6 — admission day in [release_date, due_date]
//   H7 — free bed on every day of the stay
//   H1 — room gender compatible on every day of the stay
//   H2 — room not in the patient's incompatible-room list
//   H4 — OT has enough remaining surgery minutes
//   H3 — surgeon has enough remaining daily time budget
// ---------------------------------------------------------------------------
bool IHTC_Output::canAssignPatient(int patient_id, int day, int room_idx, int ot_idx, const IHTC_Input &in) const {
    // Bounds checks (active in debug builds, compiled away in release -DNDEBUG).
    assert(room_idx >= 0 && room_idx < (int)room_occupancy.size());
    assert(day >= 0 && day < (int)room_occupancy[0].size());
    assert(patient_id >= 0 && patient_id < (int)in.patients.size());

    const Patient &p = in.patients[patient_id];
    const Room    &r = in.rooms[room_idx];

    // H6: Admission window.
    // Patient cannot arrive before they are "released" (ready for admission).
    if (day < p.release_date) return false;
    // Mandatory patients have a hard deadline: missing it is a violation.
    if (p.mandatory && day > p.due_date) return false;

    int los  = p.length_of_stay;
    int days = in.D;

    // H7 + H1: Room capacity and gender, verified for every day of the stay.
    // We must check the entire stay, not just the admission day, because a room
    // that is free today may be at capacity tomorrow.
    for (int dd = 0; dd < los; ++dd) {
        int d_idx = day + dd;
        if (d_idx >= days) break; // stay extends beyond planning horizon; days >= D have no tracking arrays

        // H7: At least one free bed on this day.
        if (room_occupancy[room_idx][d_idx] >= r.capacity) return false;

        // H1: No gender mixing. Once a room has a gender, it stays that way.
        if (p.sex != Gender::NONE) {
            Gender g = room_gender[room_idx][d_idx];
            if (g != Gender::NONE && g != p.sex) return false;
        }
    }

    // H2: Incompatible rooms (e.g., a pediatric patient in a geriatric ward).
    for (int bad_idx : p.incompatible_room_idxs) {
        if (bad_idx == room_idx) return false;
    }

    // H4: OT capacity — only checked if the patient needs surgery (ot_idx >= 0).
    if (ot_idx >= 0) {
        // ot_availability[ot][day] was initialized to the max capacity and
        // decremented by previous assignPatient() calls.
        if (ot_availability[ot_idx][day] < p.surgery_time)
            return false;
    }

    // H3: Surgeon daily time budget.
    if (p.surgeon_idx >= 0) {
        if (surgeon_availability[p.surgeon_idx][day] < p.surgery_time)
            return false;
    }

    return true; // all hard constraints satisfied
}

// =============================================================================
// State mutation — patient assignment
// =============================================================================

// ---------------------------------------------------------------------------
// assignPatient — commit patient_id at (day, room_idx, ot_idx) and update all state.
//
// Updates in order: admission records → room occupancy + gender lock →
// OT capacity (+ S5 cache) → surgeon budget → cost caches (S7, S6, S1).
//
// Precondition: canAssignPatient() must have returned true for these arguments.
// ---------------------------------------------------------------------------
void IHTC_Output::assignPatient(int patient_id, int day, int room_idx, int ot_idx, const IHTC_Input &in) {

    // --- 1. Record the admission decision ---
    admitted[patient_id]          = true;
    admit_day[patient_id]         = day;
    room_assigned_idx[patient_id] = room_idx;
    ot_assigned_idx[patient_id]   = ot_idx;

    int los  = in.patients[patient_id].length_of_stay;
    int days = in.D;

    // --- 2. Room occupancy and gender lock ---
    // For every calendar day the patient is in the room, consume one bed.
    // On the first patient of a given sex, lock the room's gender for that day.
    for (int dd = 0; dd < los; ++dd) {
        int d_abs = day + dd; // absolute calendar day
        if (d_abs >= 0 && d_abs < days) {
            room_occupancy[room_idx][d_abs] += 1;
            if (room_gender[room_idx][d_abs] == Gender::NONE) {
                room_gender[room_idx][d_abs] = in.patients[patient_id].sex;
            }
        }
    }

    // --- 3. OT capacity decrement + S5 cache ---
    if (ot_idx >= 0) {
        // S5 (OpenOT): if the OT has not been used at all today, its remaining
        // capacity still equals the original max. Opening it now costs w_open_operating_theater.
        int max_cap = in.ots[ot_idx].availability[day];
        if (ot_availability[ot_idx][day] == max_cap) cache_open_ot_count++;

        ot_availability[ot_idx][day] -= in.patients[patient_id].surgery_time;
    }

    // --- 4. Surgeon time budget decrement ---
    if (in.patients[patient_id].surgeon_idx >= 0) {
        int surgeon_idx = in.patients[patient_id].surgeon_idx;
        surgeon_availability[surgeon_idx][day] -= in.patients[patient_id].surgery_time;
    }

    // --- 5a. S7 cache: patient delay ---
    // Each day the patient waits past their release date adds to the delay cost.
    cache_delay_raw += std::max(0, day - in.patients[patient_id].release_date);

    // --- 5b. S6 cache + tracking: surgeon OT transfer ---
    // A surgeon who uses more than one OT on the same day incurs a transfer penalty.
    // surgeon_day_ot_used[(surgeon * D + day) * n_ots + ot] tracks which OTs were used.
    // If this surgeon already has a *different* OT marked for today, that is a transfer.
    if (ot_idx >= 0 && in.patients[patient_id].surgeon_idx >= 0) {
        int sidx  = in.patients[patient_id].surgeon_idx;
        int n_ots = (int)in.ots.size();
        int base  = (sidx * days + day) * n_ots; // flat base index for this surgeon/day
        if (!surgeon_day_ot_used[base + ot_idx]) {
            // This surgeon is using ot_idx for the first time today.
            // Check if they already used any other OT today → transfer event.
            for (int o = 0; o < n_ots; ++o) {
                if (o != ot_idx && surgeon_day_ot_used[base + o]) {
                    cache_surgeon_xfer_raw++;
                    break; // at most one transfer event per assignment
                }
            }
            surgeon_day_ot_used[base + ot_idx] = true; // mark this OT as used
        }
    }

    // --- 5c. S1 cache + tracking: age-group mixing ---
    int ag = in.patients[patient_id].age_group;
    for (int dd = 0; dd < los; ++dd) {
        int d_abs = day + dd;
        if (d_abs >= days) break;
        applyAgeMixUpdate(room_idx * days + d_abs, ag);
    }
}

// =============================================================================
// Occupant seeding
// =============================================================================

// ---------------------------------------------------------------------------
// seedOccupantStay — reflect one occupant's fixed stay in the output state.
//
// Mirrors assignPatient() for a patient already in a room at day 0: increments
// room_occupancy, locks room_gender, and updates the S1 age-mix cache for
// each day in [0, length_of_stay).
// ---------------------------------------------------------------------------
void IHTC_Output::seedOccupantStay(int room_idx, int length_of_stay, Gender sex, int age_group) {
    assert(room_idx >= 0 && room_idx < (int)room_occupancy.size());
    assert(!room_occupancy.empty() && !room_occupancy[room_idx].empty());

    int los  = length_of_stay;
    int days = (int)room_occupancy[room_idx].size();

    // Occupants always start from day 0 of the planning horizon.
    for (int d = 0; d < los; ++d) {
        // Consume one bed.
        room_occupancy[room_idx][d] += 1;

        // Lock room gender on the first occupant with a known sex.
        if (sex != Gender::NONE && room_gender[room_idx][d] == Gender::NONE)
            room_gender[room_idx][d] = sex;

        // S1 incremental update.
        applyAgeMixUpdate(room_idx * days + d, age_group);
    }
}

// =============================================================================
// Small mutators and accessors
// =============================================================================

// S8 cache: called by the greedy solver when an optional patient cannot be
// placed. Incrementing the counter here avoids a full patient scan in
// computeAllCosts().
void IHTC_Output::markOptionalUnscheduled() { cache_unscheduled_raw++; }

void IHTC_Output::clearNurseAssignments() {
    nurse_assignments.clear();
}

// ---------------------------------------------------------------------------
// addNurseAssignment — record nurse coverage for one (day, shift, room).
//
// Called at most once per (day, shift, room) by solveNRA.
// ---------------------------------------------------------------------------
void IHTC_Output::addNurseAssignment(int nurse_idx, int day, int shift, int room_idx) {
    nurse_assignments.push_back({nurse_idx, day, shift, room_idx});
}

bool IHTC_Output::isAdmitted(int patient_id) const {
    assert(patient_id >= 0 && patient_id < (int)admitted.size());
    return admitted[patient_id];
}

int IHTC_Output::getAdmitDay(int patient_id) const {
    assert(patient_id >= 0 && patient_id < (int)admit_day.size());
    return admit_day[patient_id];
}

int IHTC_Output::getRoomAssignedIdx(int patient_id) const {
    assert(patient_id >= 0 && patient_id < (int)room_assigned_idx.size());
    return room_assigned_idx[patient_id];
}

int IHTC_Output::getOtAssignedIdx(int patient_id) const {
    assert(patient_id >= 0 && patient_id < (int)ot_assigned_idx.size());
    return ot_assigned_idx[patient_id];
}

// Converts the internal NurseAssignment structs to (nurse, day, shift, room) tuples
// for use by the JSON writer, which does not depend on the private struct type.
std::vector<std::tuple<int, int, int, int>> IHTC_Output::getNurseAssignmentTuples() const {
    std::vector<std::tuple<int, int, int, int>> tuples;
    tuples.reserve(nurse_assignments.size());
    for (const auto &na : nurse_assignments)
        tuples.emplace_back(na.nurse_idx, na.day, na.shift, na.room_idx);
    return tuples;
}

int IHTC_Output::getRoomOccupancy(int room_idx, int day) const {
    assert(room_idx >= 0 && room_idx < (int)room_occupancy.size());
    assert(day >= 0 && day < (int)room_occupancy[0].size());
    return room_occupancy[room_idx][day];
}

int IHTC_Output::getOtAvailability(int ot_idx, int day) const {
    assert(ot_idx >= 0 && ot_idx < (int)ot_availability.size());
    assert(day >= 0 && day < (int)ot_availability[0].size());
    return ot_availability[ot_idx][day];
}

// ---------------------------------------------------------------------------
// applyAgeMixUpdate — commit one S1 age-mix update for (flat_idx, age_group).
//
// flat_idx = room_idx * D + day (pre-computed by caller).
// Updates room_day_min/max_age and accumulates the range delta into cache_age_mix_raw.
// ---------------------------------------------------------------------------
void IHTC_Output::applyAgeMixUpdate(int flat_idx, int age_group) {
    int old_min = room_day_min_age[flat_idx];
    int old_max = room_day_max_age[flat_idx];
    bool was_empty = (old_min == INT_MAX);
    int old_range  = was_empty ? 0 : (old_max - old_min);
    int new_min    = was_empty ? age_group : std::min(old_min, age_group);
    int new_max    = was_empty ? age_group : std::max(old_max, age_group);
    cache_age_mix_raw += (new_max - new_min) - old_range;
    room_day_min_age[flat_idx] = new_min;
    room_day_max_age[flat_idx] = new_max;
}

// ---------------------------------------------------------------------------
// getRoomAgeMixMarginal — marginal S1 cost of adding age_group to room_idx on day.
//
// Returns the increase in (max_age - min_age) range without committing any state.
// Returns 0 when the room has no age-group patient yet or when age_group is
// already within the current range.
// ---------------------------------------------------------------------------
int IHTC_Output::getRoomAgeMixMarginal(int room_idx, int day, int age_group) const {
    int idx     = room_idx * bound_input->D + day;
    int cur_min = room_day_min_age[idx];
    int cur_max = room_day_max_age[idx];
    if (cur_min == INT_MAX) return 0; // room is empty — first patient sets range = 0
    int old_range = cur_max - cur_min;
    int new_range = std::max(cur_max, age_group) - std::min(cur_min, age_group);
    return new_range - old_range; // marginal increase (can be 0 if age fits in existing range)
}

// ---------------------------------------------------------------------------
// surgeonHasOtherOTOnDay — true if the surgeon uses any OT other than ot_idx on day.
//
// Used by evaluatePlacementCost() to detect S6 surgeon-transfer events before
// committing a placement. Reads surgeon_day_ot_used[(surgeon*D+day)*n_ots + ot].
// ---------------------------------------------------------------------------
bool IHTC_Output::surgeonHasOtherOTOnDay(int surgeon_idx, int day, int ot_idx) const {
    if (bound_input->ots.empty()) return false;
    int n_ots = (int)bound_input->ots.size();
    int base  = (surgeon_idx * bound_input->D + day) * n_ots;
    for (int o = 0; o < n_ots; ++o)
        if (o != ot_idx && surgeon_day_ot_used[base + o]) return true;
    return false;
}

// =============================================================================
// Cost computation
// =============================================================================

// ---------------------------------------------------------------------------
// computeAllCosts — compute and return the weighted cost of all 8 soft constraints.
//
// S1/S5/S6/S7/S8 are read from incremental caches (O(1)).
// S2 (skill gap), S3 (continuity of care), S4 (excess workload) scan nurse
// assignments from scratch.
// ---------------------------------------------------------------------------
IHTC_Output::CostBreakdown IHTC_Output::computeAllCosts() const {
    CostBreakdown cb;
    const IHTC_Input &in = *bound_input;
    int days       = in.D;
    int shifts     = in.shifts_per_day;
    int num_rooms  = (int)in.rooms.size();
    int num_nurses = (int)in.nurses.size();

    // Flat index lambdas.
    auto dsr_idx = [&](int d, int sh, int r) { return (d * shifts + sh) * num_rooms + r; };
    auto nsh_idx = [&](int n, int shift_idx) { return n * (days * shifts) + shift_idx; };

    int dsr_size = days * shifts * num_rooms;

    // -----------------------------------------------------------------------
    // Step 1 — Build room_shift_load[dsr_idx(d, sh, r)]
    //   = total nursing workload units demanded by all patients in room r
    //     during shift sh on day d.
    //
    // Both occupants and admitted patients use the same per-shift indexing:
    //   idx = dd * shifts + sh, where dd is days elapsed since admission.
    // Occupants have start_day=0; patients start at their admit_day.
    // -----------------------------------------------------------------------
    std::vector<int> room_shift_load(dsr_size, 0);

    auto add_stay_load = [&](int ridx, int start_day, int los,
                              const std::vector<int> &load_per_shift) {
        for (int dd = 0; dd < los; ++dd) {
            int d = start_day + dd;
            if (d >= days) break;
            for (int sh = 0; sh < shifts; ++sh)
                room_shift_load[dsr_idx(d, sh, ridx)] += load_per_shift[dd * shifts + sh];
        }
    };

    for (const Occupant &o : in.occupants)
        add_stay_load(o.room_idx, 0, o.length_of_stay, o.nurse_load_per_shift);

    for (size_t pid = 0; pid < in.patients.size(); ++pid) {
        if (!isAdmitted((int)pid)) continue;
        add_stay_load(room_assigned_idx[pid], admit_day[pid],
                      in.patients[pid].length_of_stay,
                      in.patients[pid].nurse_load_per_shift);
    }

    // -----------------------------------------------------------------------
    // Step 2 — Build nurse actual load tables.
    //
    // nurse_load_by_shift[nsh]         = actual workload assigned to nurse n
    //                                    on shift t (accumulated from assignments).
    // room_shift_nurse[dsr_idx(d,s,r)] = which nurse covers room r on shift s/day d
    //                                    (-1 if no assignment).
    // -----------------------------------------------------------------------
    int nsh_size = num_nurses * days * shifts;

    std::vector<int> nurse_load_by_shift(nsh_size, 0);
    std::vector<int> room_shift_nurse(dsr_size, -1);

    // For each nurse assignment, record which nurse covers which room/shift/day
    // and accumulate the corresponding workload into nurse_load_by_shift.
    for (const auto &na : nurse_assignments) {
        int dsr = dsr_idx(na.day, na.shift, na.room_idx);
        room_shift_nurse[dsr] = na.nurse_idx;
        nurse_load_by_shift[nsh_idx(na.nurse_idx, na.day * shifts + na.shift)]
            += room_shift_load[dsr]; // nurse absorbs all workload of the assigned room
    }

    // -----------------------------------------------------------------------
    // Step 3 — Compute each soft constraint cost.
    // -----------------------------------------------------------------------

    // S1 (AgeMix): sum over all (room, day) of (max_age_group - min_age_group).
    // Accumulated incrementally in assignPatient() / seedOccupantStay().
    cb.age_mix = cache_age_mix_raw * in.w_room_mixed_age;

    // -----------------------------------------------------------------------
    // S2 (RoomSkillLevel): for each patient/occupant on each shift of their
    // stay, if the assigned nurse's skill level is below the required level,
    // add the shortfall to raw_skill.
    // -----------------------------------------------------------------------
    int raw_skill = 0;

    auto add_stay_skill = [&](int ridx, int start_day, int los,
                               const std::vector<int> &skill_per_shift) {
        for (int dd = 0; dd < los; ++dd) {
            int d = start_day + dd;
            if (d >= days) break;
            for (int sh = 0; sh < shifts; ++sh) {
                int nidx = room_shift_nurse[dsr_idx(d, sh, ridx)];
                if (nidx < 0) continue;
                int req_skill = skill_per_shift[dd * shifts + sh];
                if (req_skill > in.nurses[nidx].level)
                    raw_skill += req_skill - in.nurses[nidx].level;
            }
        }
    };

    for (const Occupant &o : in.occupants)
        add_stay_skill(o.room_idx, 0, o.length_of_stay, o.skill_level_required_per_shift);

    for (size_t pid = 0; pid < in.patients.size(); ++pid) {
        if (!isAdmitted((int)pid)) continue;
        add_stay_skill(room_assigned_idx[pid], admit_day[pid],
                       in.patients[pid].length_of_stay,
                       in.patients[pid].skill_level_required_per_shift);
    }
    cb.skill = raw_skill * in.w_room_nurse_skill;

    // -----------------------------------------------------------------------
    // S3 (ContinuityOfCare): for each patient/occupant, count the number of
    // *distinct* nurses who covered their room during their stay. Fewer distinct
    // nurses → better continuity. It counts the set size, so even a single nurse 
    // covering all shifts still contributes 1 per shift where they appear — the
    // set collapses repeated appearances automatically.
    // -----------------------------------------------------------------------
    int raw_cont = 0;

    auto add_stay_continuity = [&](int ridx, int start_day, int los) {
        std::set<int> seen_nurses;
        for (int dd = 0; dd < los; ++dd) {
            int d = start_day + dd;
            if (d >= days) break;
            for (int sh = 0; sh < shifts; ++sh) {
                int nidx = room_shift_nurse[dsr_idx(d, sh, ridx)];
                if (nidx >= 0) seen_nurses.insert(nidx);
            }
        }
        raw_cont += (int)seen_nurses.size();
    };

    for (const Occupant &o : in.occupants)
        add_stay_continuity(o.room_idx, 0, o.length_of_stay);

    for (size_t pid = 0; pid < in.patients.size(); ++pid) {
        if (!isAdmitted((int)pid)) continue;
        add_stay_continuity(room_assigned_idx[pid], admit_day[pid],
                            in.patients[pid].length_of_stay);
    }
    cb.continuity = raw_cont * in.w_continuity_of_care;

    // -----------------------------------------------------------------------
    // S4 (ExcessWorkload): for each (nurse, shift), if the total workload
    // assigned exceeds the nurse's max_load for that shift, the excess is
    // penalised. Non-working shifts always have load=0, so only working shifts
    // need to be checked.
    // -----------------------------------------------------------------------
    int raw_excess = 0;
    for (int nidx = 0; nidx < num_nurses; ++nidx) {
        for (const auto &ws : in.nurses[nidx].working_shifts) {
            int shift_loc = ws.day * shifts + ws.shift;
            int over = nurse_load_by_shift[nsh_idx(nidx, shift_loc)] - ws.max_load;
            if (over > 0) raw_excess += over;
        }
    }
    cb.excess = raw_excess * in.w_nurse_eccessive_workload;

    // S5 (OpenOT): count of (day, OT) pairs that were used at least once.
    // Accumulated in assignPatient() — first use of an OT on a day increments cache.
    cb.open_ot = cache_open_ot_count * in.w_open_operating_theater;

    // S6 (SurgeonTransfer): count of OT-switch events per surgeon per day.
    // Accumulated in assignPatient() when a surgeon's second distinct OT is detected.
    cb.surgeon_transfer = cache_surgeon_xfer_raw * in.w_surgeon_transfer;

    // S7 (PatientDelay): sum of (admit_day - release_date) over all admitted patients.
    // Accumulated in assignPatient().
    cb.delay = cache_delay_raw * in.w_patient_delay;

    // S8 (Unscheduled): count of optional patients who could not be placed.
    // Incremented by markOptionalUnscheduled() in the greedy loop.
    cb.unscheduled = cache_unscheduled_raw * in.w_unscheduled_optional;

    // Total weighted cost.
    cb.total = cb.age_mix + cb.skill + cb.continuity + cb.excess
             + cb.open_ot + cb.surgeon_transfer + cb.delay + cb.unscheduled;

    return cb;
}

// =============================================================================
// Output helpers
// =============================================================================

void IHTC_Output::printCosts() const {
    CostBreakdown cb = computeAllCosts();
    std::cout << "Cost: "            << cb.total
              << ", Unscheduled: "   << cb.unscheduled
              << ",  Delay: "        << cb.delay
              << ",  OpenOT: "       << cb.open_ot
              << ",  AgeMix: "       << cb.age_mix
              << ",  Skill: "        << cb.skill
              << ",  Excess: "       << cb.excess
              << ",  Continuity: "   << cb.continuity
              << ",  SurgeonTransfer: " << cb.surgeon_transfer
              << std::endl;
}

void IHTC_Output::writeJSON(const std::string& filename) const {
    jsonio::write_solution(*bound_input, *this, filename);
}

// =============================================================================
// IHTC_Input — instance loading
// =============================================================================

bool IHTC_Input::loadInstance(const std::string &path) {
    return jsonio::load_instance(*this, path);
}
