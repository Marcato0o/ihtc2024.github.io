#ifndef IHTC_DATA_HH
#define IHTC_DATA_HH

#include <string>
#include <vector>
#include <map>
#include <tuple>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

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
    std::string sex;
    int surgery_time = 0; // minutes
    std::string surgeon_id;
    std::vector<std::string> incompatible_rooms;
    std::vector<int> nurse_load_per_shift; // per-turn load during stay
    int min_nurse_level = 0;
    json raw_json; // store full patient JSON for later
};

struct Room {
    std::string id;
    int capacity = 0;
};

struct Nurse {
    std::string id;
    int level = 0;
    std::vector<int> roster; // 0/1 per shift index
    int max_load = 0;
};
struct Surgeon {
    std::string id;
    int max_daily_time = 0;
    std::vector<int> daily_max_time;
};

struct Occupant {
    std::string id;
    std::string room_id;
    std::string sex;
    int admission_day = 0;
    int length_of_stay = 0;
    std::vector<int> nurse_load_per_shift;
    int min_nurse_level = 0;
};

struct OT {
    std::string id;
    int daily_capacity = 0; // minutes
    std::vector<int> unavailable_days; // day indices
    std::vector<int> daily_capacity_by_day;
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
    std::map<std::string,int> weights;

private:
    friend bool jsonio::load_instance(IHTC_Input &in, const std::string &path);
    std::string raw_json_text;
};

class IHTC_Output {
public:
    explicit IHTC_Output(const IHTC_Input &in);

    void init(size_t num_patients, size_t num_rooms, size_t num_ots, int days);
    bool canAssignPatient(int patient_id, int day, int room_idx, int ot_idx, const IHTC_Input &in) const;
    void assignPatient(int patient_id, int day, int room_idx, int ot_idx, const IHTC_Input &in);
    void seedOccupantStay(int room_idx, int admission_day, int length_of_stay, const std::string &sex);
    void clearNurseAssignments();
    void addNurseAssignment(int nurse_idx, int day, int shift, int room_idx);
    bool isAdmitted(int patient_id) const;
    int getAdmitDay(int patient_id) const;
    int getRoomAssignedIdx(int patient_id) const;
    int getOtAssignedIdx(int patient_id) const;
    std::vector<std::tuple<int, int, int, int>> getNurseAssignmentTuples() const;
    int getRoomOccupancy(int room_idx, int day) const;
    int getOtMinutesUsed(int ot_idx, int day) const;

    int ComputeCostRoomMixedAge() const;
    int ComputeCostRoomNurseSkill() const;
    int ComputeCostContinuityOfCare() const;
    int ComputeCostNurseExcessiveWorkload() const;
    int ComputeCostOpenOperatingTheater() const;
    int ComputeCostSurgeonTransfer() const;
    int ComputeCostPatientDelay() const;
    int ComputeCostUnscheduledOptional() const;
    int ComputeCostTotal() const;
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
    std::vector<std::vector<int>> ot_minutes_used;
    std::vector<std::vector<int>> surgeon_minutes_used;
    std::vector<std::vector<std::string>> room_gender;
};

#endif // IHTC_DATA_HH
