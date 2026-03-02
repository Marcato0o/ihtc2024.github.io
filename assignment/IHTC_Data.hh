// Minimal IHTC data structures and IO stubs
#ifndef IHTC_DATA_HH
#define IHTC_DATA_HH

#include <string>
#include <vector>
#include <optional>

struct Patient {
    std::string id;
    bool mandatory = false;
    int release_date = 0;
    int due_date = 0;
    int length_of_stay = 0;
};

struct Room {
    std::string id;
    int capacity = 0;
};

struct Nurse {
    std::string id;
};

struct Surgeon {
    std::string id;
};

class IHTC_Data {
public:
    IHTC_Data() = default;
    // Load instance JSON (stub - implement JSON parsing later)
    bool loadInstance(const std::string &path);
    // Write solution JSON (stub - implement solution format later)
    bool writeSolution(const std::string &path) const;
    // Placeholder for greedy solver (todo)
    bool runGreedyTodo();

    // Basic data containers (expand to match instance schema)
    std::vector<Patient> patients;
    std::vector<Room> rooms;
    std::vector<Nurse> nurses;
    std::vector<Surgeon> surgeons;
};

#endif // IHTC_DATA_HH
