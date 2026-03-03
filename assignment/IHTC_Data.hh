// Minimal IHTC data structures and IO stubs
#ifndef IHTC_DATA_HH
#define IHTC_DATA_HH

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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
    bool optional = false;
    std::vector<std::string> incompatible_rooms;
    std::vector<int> nurse_load_per_shift; // per-turn load during stay
    int min_nurse_level = 0;
    json raw_json; // store full patient JSON for later
};

struct Room {
    std::string id;
    int capacity = 0;
    std::vector<std::string> incompatible_patients; // ids or groups
    std::vector<int> unavailable_days;
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

class IHTC_Input {
public:
    IHTC_Input() = default;
    explicit IHTC_Input(const std::string &file_name);
    // Load instance JSON (stub - implement JSON parsing later)
    bool loadInstance(const std::string &path);

    // Basic data containers (expand to match instance schema)
    std::vector<Patient> patients;
    std::vector<Room> rooms;
    std::vector<Nurse> nurses;
    std::vector<Surgeon> surgeons;
    std::vector<Occupant> occupants;

    // Scheduling horizon and structure
    int D = 0; // days
    int shifts_per_day = 3;

    // Operating theatres
    struct OT {
        std::string id;
        int daily_capacity = 0; // minutes
        std::vector<int> unavailable_days; // day indices
        std::vector<int> daily_capacity_by_day;
    };
    std::vector<OT> ots;

    // Weights for soft constraints
    std::map<std::string,int> weights;

    // Raw JSON text (keeps full instance for later)
    std::string raw_json_text;
};

// Output container in WL style (paired with IHTC_Input in the data module).
struct IHTC_Output {
    std::vector<bool> admitted;
    std::vector<int> admit_day;
    std::vector<int> room_assigned_idx;
    std::vector<int> ot_assigned_idx;

    std::vector<std::vector<int>> room_occupancy;
    std::vector<std::vector<int>> ot_minutes_used;
    std::vector<std::vector<int>> surgeon_minutes_used;
    std::vector<std::vector<std::string>> room_gender;

    void init(size_t num_patients, size_t num_rooms, size_t num_ots, int days) {
        admitted.assign(num_patients, false);
        admit_day.assign(num_patients, -1);
        room_assigned_idx.assign(num_patients, -1);
        ot_assigned_idx.assign(num_patients, -1);
        room_occupancy.assign(num_rooms, std::vector<int>(days, 0));
        ot_minutes_used.assign(num_ots, std::vector<int>(days, 0));
        surgeon_minutes_used.assign(0, std::vector<int>());
        room_gender.assign(num_rooms, std::vector<std::string>(days, ""));
    }

    bool canAssignPatient(int patient_id, int day, int room_idx, int ot_idx, const IHTC_Input &in) const {
        if (room_idx < 0 || room_idx >= (int)room_occupancy.size()) return false;
        if (day < 0 || day >= (int)room_occupancy[0].size()) return false;
        const Patient &p = in.patients[patient_id];
        const Room &r = in.rooms[room_idx];

        // H6: admission day within patient time window.
        if (day < p.release_date) return false;
        if (p.mandatory && day > p.due_date) return false;

        int los = std::max(1, p.length_of_stay);
        int days = (int)room_occupancy[0].size();

        // H1 + H7 over full stay.
        for (int dd = 0; dd < los; ++dd) {
            int d_idx = day + dd;
            if (d_idx < 0 || d_idx >= days) break;
            if (room_occupancy[room_idx][d_idx] >= r.capacity) return false;
            if (!p.sex.empty()) {
                const std::string &g = room_gender[room_idx][d_idx];
                if (!g.empty() && g != p.sex) return false;
            }
        }

        for (const auto &bad : p.incompatible_rooms) if (bad == r.id) return false;

        // H4: OT daily capacity (respect per-day availability if present).
        if (ot_idx >= 0 && ot_idx < (int)ot_minutes_used.size()) {
            int used = ot_minutes_used[ot_idx][day];
            int cap = in.ots[ot_idx].daily_capacity;
            if (!in.ots[ot_idx].daily_capacity_by_day.empty() && day < (int)in.ots[ot_idx].daily_capacity_by_day.size()) {
                cap = in.ots[ot_idx].daily_capacity_by_day[day];
            }
            for (int bad_day : in.ots[ot_idx].unavailable_days) {
                if (bad_day == day) cap = 0;
            }
            if (used + p.surgery_time > cap) return false;
        }

        // H3: surgeon daily maximum time.
        if (!p.surgeon_id.empty()) {
            int surgeon_idx = -1;
            for (int i = 0; i < (int)in.surgeons.size(); ++i) {
                if (in.surgeons[i].id == p.surgeon_id) { surgeon_idx = i; break; }
            }
            if (surgeon_idx >= 0) {
                int limit = in.surgeons[surgeon_idx].max_daily_time;
                if (!in.surgeons[surgeon_idx].daily_max_time.empty() && day < (int)in.surgeons[surgeon_idx].daily_max_time.size()) {
                    limit = in.surgeons[surgeon_idx].daily_max_time[day];
                }
                int used = 0;
                if (surgeon_idx < (int)surgeon_minutes_used.size() && day < (int)surgeon_minutes_used[surgeon_idx].size()) {
                    used = surgeon_minutes_used[surgeon_idx][day];
                }
                if (used + p.surgery_time > limit) return false;
            }
        }

        return true;
    }

    void assignPatient(int patient_id, int day, int room_idx, int ot_idx, const IHTC_Input &in) {
        admitted[patient_id] = true;
        admit_day[patient_id] = day;
        room_assigned_idx[patient_id] = room_idx;
        ot_assigned_idx[patient_id] = ot_idx;
        int los = std::max(1, in.patients[patient_id].length_of_stay);
        int days = room_occupancy.empty() ? 0 : (int)room_occupancy[0].size();
        for (int dd = 0; dd < los; ++dd) {
            int dd_idx = day + dd;
            if (dd_idx >= 0 && dd_idx < days) {
                room_occupancy[room_idx][dd_idx] += 1;
                if (!in.patients[patient_id].sex.empty() && room_gender[room_idx][dd_idx].empty()) {
                    room_gender[room_idx][dd_idx] = in.patients[patient_id].sex;
                }
            }
        }
        if (ot_idx >= 0 && ot_idx < (int)ot_minutes_used.size()) {
            int days_ot = (int)ot_minutes_used[0].size();
            if (day >= 0 && day < days_ot) ot_minutes_used[ot_idx][day] += in.patients[patient_id].surgery_time;
        }

        if (!in.patients[patient_id].surgeon_id.empty()) {
            int surgeon_idx = -1;
            for (int i = 0; i < (int)in.surgeons.size(); ++i) {
                if (in.surgeons[i].id == in.patients[patient_id].surgeon_id) { surgeon_idx = i; break; }
            }
            if (surgeon_idx >= 0) {
                if (surgeon_minutes_used.size() != in.surgeons.size()) {
                    surgeon_minutes_used.assign(in.surgeons.size(), std::vector<int>(days, 0));
                }
                if (day >= 0 && day < days) surgeon_minutes_used[surgeon_idx][day] += in.patients[patient_id].surgery_time;
            }
        }
    }

    int getRoomOccupancy(int room_idx, int day) const {
        if (room_idx < 0 || room_idx >= (int)room_occupancy.size()) return 0;
        if (day < 0 || day >= (int)room_occupancy[0].size()) return 0;
        return room_occupancy[room_idx][day];
    }

    int getOtMinutesUsed(int ot_idx, int day) const {
        if (ot_idx < 0 || ot_idx >= (int)ot_minutes_used.size()) return 0;
        if (day < 0 || day >= (int)ot_minutes_used[0].size()) return 0;
        return ot_minutes_used[ot_idx][day];
    }
};

using IHTC_Data = IHTC_Input;

#endif // IHTC_DATA_HH
