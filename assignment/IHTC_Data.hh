#ifndef IHTC_DATA_HH
#define IHTC_DATA_HH

#include <cstdint>
#include <string>
#include <vector>
#include <tuple>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

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
    std::vector<int> nurse_load_per_shift; // per-turn load during stay
    int min_nurse_level = 0;
};

struct Room {
    std::string id;
    int capacity = 0;
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
struct Surgeon {
    std::string id;
    std::vector<int> max_surgery_time;
};

struct Occupant {
    std::string id;
    int room_idx = -1;
    Gender sex = Gender::NONE;
    int admission_day = 0;
    int length_of_stay = 0;
    std::vector<int> nurse_load_per_shift;
    int min_nurse_level = 0;
};

struct OT {
    std::string id;
    std::vector<int> availability; // daily capacity array
};

class IHTC_Input {
public:
    IHTC_Input();
    explicit IHTC_Input(const std::string &file_name);
    bool loadInstance(const std::string &path);
    const std::string &getRawJsonText() const;

    std::vector<Patient> patients;
    std::vector<Room> rooms;
    std::vector<Nurse> nurses;
    std::vector<Surgeon> surgeons;
    std::vector<Occupant> occupants;
    std::vector<OT> ots;
    int D = 0; // days
    int shifts_per_day = 3;

    // Pesi soft letti dal JSON (chiave → valore)
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
    std::string raw_json_text;
};

class IHTC_Output {
public:
    explicit IHTC_Output(const IHTC_Input &in);

    void init(const IHTC_Input &in);
    bool canAssignPatient(int patient_id, int day, int room_idx, int ot_idx, const IHTC_Input &in) const;
    void assignPatient(int patient_id, int day, int room_idx, int ot_idx, const IHTC_Input &in);
    void seedOccupantStay(int room_idx, int admission_day, int length_of_stay, Gender sex);
    void clearNurseAssignments();
    void addNurseAssignment(int nurse_idx, int day, int shift, int room_idx);
    bool isAdmitted(int patient_id) const;
    int getAdmitDay(int patient_id) const;
    int getRoomAssignedIdx(int patient_id) const;
    int getOtAssignedIdx(int patient_id) const;
    std::vector<std::tuple<int, int, int, int>> getNurseAssignmentTuples() const;
    int getRoomOccupancy(int room_idx, int day) const;
    int getOtAvailability(int ot_idx, int day) const;

    struct CostBreakdown {
        int age_mix = 0;
        int skill = 0;
        int continuity = 0;
        int excess = 0;
        int open_ot = 0;
        int surgeon_transfer = 0;
        int delay = 0;
        int unscheduled = 0;
        int total = 0;
    };
    CostBreakdown computeAllCosts() const;
    void writeJSON(const std::string& filename) const;
    void printCosts() const;

private:
    struct NurseAssignment { int nurse_idx; int day; int shift; int room_idx; };
    const IHTC_Input *bound_input = nullptr;
    std::vector<bool> admitted;
    std::vector<int> admit_day;
    std::vector<int> room_assigned_idx;
    std::vector<int> ot_assigned_idx;
    std::vector<NurseAssignment> nurse_assignments;
    std::vector<std::vector<int>> room_occupancy;
    std::vector<std::vector<int>> ot_availability;
    std::vector<std::vector<int>> surgeon_availability;
    std::vector<std::vector<Gender>> room_gender;
};

#endif // IHTC_DATA_HH
