#ifndef IHTC_DATA_HH
#define IHTC_DATA_HH

#include <cstdint>
#include <string>
#include <vector>
#include <tuple>
#include "nlohmann/json.hpp"

enum class Gender : int8_t {
    NONE = 0,
    A = 1,
    B = 2
};

class IHTC_Input;
class IHTC_Output;

namespace jsonio {
bool load_instance(IHTC_Input &in, const std::string &path);
void write_solution(const IHTC_Input &in, const IHTC_Output &out, const std::string &filename);
} // namespace jsonio

// --- Physical spaces ---

struct Room {
    std::string id;
    int capacity = 0;
};

struct OT {
    std::string id;
    std::vector<int> availability; // daily capacity array (minutes per day)
};

// --- Staff ---

struct Surgeon {
    std::string id;
    std::vector<int> max_surgery_time; // daily budget (minutes per day)
};

struct WorkingShift {
    int day = 0;
    int shift = 0; // 0=early, 1=late, 2=night
    int max_load = 0;
};

struct Nurse {
    std::string id;
    int level = 0;
    std::vector<WorkingShift> working_shifts;
};

// --- Patients ---

struct Patient {
    std::string id;
    bool mandatory = false;
    int release_date = 0;
    int due_date = 0;
    int length_of_stay = 0;
    int age_group = -1;
    Gender sex = Gender::NONE;
    int surgery_time = 0; // minutes
    int surgeon_idx = -1;
    std::vector<int> incompatible_room_idxs;
    std::vector<int> nurse_load_per_shift;        // flat: [day * shifts + shift]
    std::vector<int> skill_level_required_per_shift; // flat: [day * shifts + shift]
};

struct Occupant {
    std::string id;
    int room_idx = -1;
    Gender sex = Gender::NONE;
    int length_of_stay = 0;
    int age_group = -1;
    std::vector<int> nurse_load_per_shift;        // flat: [day * shifts + shift]
    std::vector<int> skill_level_required_per_shift; // flat: [day * shifts + shift]
};

// =============================================================================
// Input Class:
// =============================================================================

class IHTC_Input {
public:
    IHTC_Input();
    explicit IHTC_Input(const std::string &file_name);
    const std::string &getRawJsonText() const;

    // -- Problem data --
    std::vector<Patient>  patients;
    std::vector<Room>     rooms;
    std::vector<Nurse>    nurses;
    std::vector<Surgeon>  surgeons;
    std::vector<Occupant> occupants;
    std::vector<OT>       ots;
    int D              = 0; // planning horizon (days)
    int shifts_per_day = 0; // set from JSON "shift_types" array size

    // -- Instance metadata --
    int skill_levels = 0;                  // number of distinct skill levels (e.g. 3)
    std::vector<std::string> shift_types;  // canonical shift names; index = shift integer
    std::vector<std::string> age_groups;   // canonical age-group names; index = age_group integer

    // -- Soft-constraint weights --
    int w_room_mixed_age;
    int w_open_operating_theater;
    int w_patient_delay;
    int w_unscheduled_optional;
    int w_room_nurse_skill;
    int w_continuity_of_care;
    int w_nurse_eccessive_workload;
    int w_surgeon_transfer;

private:
    friend bool jsonio::load_instance(IHTC_Input &in, const std::string &path);
    bool loadInstance(const std::string &path);
    std::string raw_json_text;
};

// =============================================================================
// Output Class:
// =============================================================================

class IHTC_Output {
public:
    explicit IHTC_Output(const IHTC_Input &in);

    // -- Phase 1: patient placement --
    bool canAssignPatient(int patient_id, int day, int room_idx, int ot_idx, const IHTC_Input &in) const;
    void assignPatient(int patient_id, int day, int room_idx, int ot_idx, const IHTC_Input &in);
    void seedOccupantStay(int room_idx, int length_of_stay, Gender sex, int age_group = -1);
    void markOptionalUnscheduled();

    // -- Phase 2: nurse assignment --
    void clearNurseAssignments();
    void addNurseAssignment(int nurse_idx, int day, int shift, int room_idx);

    // -- Queries --
    bool isAdmitted(int patient_id) const;
    int  getAdmitDay(int patient_id) const;
    int  getRoomAssignedIdx(int patient_id) const;
    int  getOtAssignedIdx(int patient_id) const;
    int  getOtAvailability(int ot_idx, int day) const;
    int  getRoomAgeMixMarginal(int room_idx, int day, int age_group) const; // marginal S1 delta, read-only
    bool surgeonHasOtherOTOnDay(int surgeon_idx, int day, int ot_idx) const; // S6 detection, read-only
    std::vector<std::tuple<int, int, int, int>> getNurseAssignmentTuples() const; // for writer

    // -- Output --
    struct CostBreakdown {
        int age_mix = 0, skill = 0, continuity = 0, excess = 0;
        int open_ot = 0, surgeon_transfer = 0, delay = 0, unscheduled = 0;
        int total = 0;
    };
    CostBreakdown computeAllCosts() const;
    void writeJSON(const std::string& filename) const;
    void printCosts() const;

private:
    void init(const IHTC_Input &in);

    struct NurseAssignment { int nurse_idx; int day; int shift; int room_idx; };

    const IHTC_Input *bound_input = nullptr;

    // -- Admission state --
    std::vector<bool> admitted;
    std::vector<int>  admit_day;
    std::vector<int>  room_assigned_idx;
    std::vector<int>  ot_assigned_idx;
    std::vector<NurseAssignment> nurse_assignments;

    // -- Capacity tracking --
    std::vector<int>              room_occupancy;       // [room * D + day]
    std::vector<Gender>           room_gender;          // [room * D + day]
    std::vector<std::vector<int>> ot_availability;      // [ot][day]
    std::vector<std::vector<int>> surgeon_availability; // [surgeon][day]

    // -- S1: age-mix tracking --
    std::vector<int> room_day_min_age; // [room * D + day], INT_MAX = empty
    std::vector<int> room_day_max_age; // [room * D + day], -1 = empty
    void applyAgeMixUpdate(int flat_idx, int age_group);

    // -- S6: surgeon-OT tracking --
    std::vector<bool> surgeon_day_ot_used; // [(surgeon * D + day) * n_ots + ot]

    // -- Cost caches (S1/S5/S6/S7/S8, accumulated in assignPatient) --
    int cache_delay_raw        = 0;
    int cache_open_ot_count    = 0;
    int cache_age_mix_raw      = 0;
    int cache_surgeon_xfer_raw = 0;
    int cache_unscheduled_raw  = 0;
};

#endif // IHTC_DATA_HH
