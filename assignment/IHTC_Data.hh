// Minimal IHTC data structures and IO stubs
#ifndef IHTC_DATA_HH
#define IHTC_DATA_HH

#include <string>
#include <vector>
#include <optional>
#include <map>
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
    std::optional<std::string> room_assigned;
    std::optional<std::string> ot_assigned;
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
};

class IHTC_Data {
public:
    IHTC_Data() = default;
    // Load instance JSON (stub - implement JSON parsing later)
    bool loadInstance(const std::string &path);
    // Write solution JSON (stub - implement solution format later)
    bool writeSolution(const std::string &path) const;
    // Run the integrated greedy solver
    bool runGreedySolver();

    // Basic data containers (expand to match instance schema)
    std::vector<Patient> patients;
    std::vector<Room> rooms;
    std::vector<Nurse> nurses;
    std::vector<Surgeon> surgeons;

    // Scheduling horizon and structure
    int D = 0; // days
    int shifts_per_day = 3;

    // Operating theatres
    struct OT {
        std::string id;
        int daily_capacity = 0; // minutes
        std::vector<int> unavailable_days; // day indices
    };
    std::vector<OT> ots;

    // Weights for soft constraints
    std::map<std::string,int> weights;

    // Raw JSON text (keeps full instance for later)
    std::string raw_json_text;
};

#endif // IHTC_DATA_HH
