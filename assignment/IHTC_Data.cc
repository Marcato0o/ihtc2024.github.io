#include "IHTC_Data.hh"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

static std::string try_get_string(const json &j, const std::vector<std::string> &candidates, const std::string &def="") {
    for (auto &k : candidates) if (j.contains(k) && !j[k].is_null()) return j[k].get<std::string>();
    return def;
}

static int try_get_int(const json &j, const std::vector<std::string> &candidates, int def=0) {
    for (auto &k : candidates) if (j.contains(k) && !j[k].is_null()) return j[k].get<int>();
    return def;
}

static bool try_get_bool(const json &j, const std::vector<std::string> &candidates, bool def=false) {
    for (auto &k : candidates) if (j.contains(k) && !j[k].is_null()) return j[k].get<bool>();
    return def;
}

bool IHTC_Data::loadInstance(const std::string &path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Failed to open instance: " << path << "\n";
        return false;
    }

    json j;
    try {
        in >> j;
    } catch (const std::exception &e) {
        std::cerr << "JSON parse error: " << e.what() << "\n";
        return false;
    }

    patients.clear();
    rooms.clear();
    nurses.clear();
    surgeons.clear();

    // tolerant parsing: look for common keys and multiple naming conventions
    if (j.contains("patients") && j["patients"].is_array()) {
        for (const auto &jp : j["patients"]) {
            Patient p;
            p.id = try_get_string(jp, {"id","patient_id","patientId"}, "");
            p.mandatory = try_get_bool(jp, {"mandatory","isMandatory"}, false);
            p.release_date = try_get_int(jp, {"releaseDate","release_date","release"}, 0);
            p.due_date = try_get_int(jp, {"dueDate","due_date","due"}, 0);
            p.length_of_stay = try_get_int(jp, {"lengthOfStay","length_of_stay","los","stay"}, 1);
            patients.push_back(std::move(p));
        }
    }

    if (j.contains("rooms") && j["rooms"].is_array()) {
        for (const auto &jr : j["rooms"]) {
            Room r;
            r.id = try_get_string(jr, {"id","room_id","roomId"}, "");
            r.capacity = try_get_int(jr, {"capacity","beds","size"}, 1);
            rooms.push_back(std::move(r));
        }
    }

    if (j.contains("nurses") && j["nurses"].is_array()) {
        for (const auto &jn : j["nurses"]) {
            Nurse n;
            n.id = try_get_string(jn, {"id","nurse_id","nurseId","name"}, "");
            nurses.push_back(std::move(n));
        }
    }

    if (j.contains("surgeons") && j["surgeons"].is_array()) {
        for (const auto &js : j["surgeons"]) {
            Surgeon s;
            s.id = try_get_string(js, {"id","surgeon_id","surgeonId","name"}, "");
            surgeons.push_back(std::move(s));
        }
    }

    return true;
}

bool IHTC_Data::writeSolution(const std::string &path) const {
    std::ofstream out(path);
    if (!out) {
        std::cerr << "Failed to write solution: " << path << "\n";
        return false;
    }
    // Minimal JSON output: list admitted patient ids (placeholder)
    json outj;
    outj["admitted"] = json::array();
    for (const auto &p : patients) {
        outj["admitted"].push_back({{"id", p.id}});
    }
    out << outj.dump(2) << std::endl;
    return true;
}

bool IHTC_Data::runGreedyTodo() {
    std::cout << "runGreedyTodo(): not implemented. This is a placeholder.\n";
    // TODO: implement the greedy constructive algorithm here.
    return true;
}

