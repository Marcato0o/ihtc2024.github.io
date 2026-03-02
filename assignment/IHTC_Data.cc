#include "IHTC_Data.hh"
#include <fstream>
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

static std::string try_get_string(const json &j, const std::vector<std::string> &candidates, const std::string &def="") {
    for (auto &k : candidates) if (j.contains(k) && !j[k].is_null()) {
        if (j[k].is_string()) return j[k].get<std::string>();
        try {
            return j[k].dump();
        } catch(...) { return def; }
    }
    return def;
}

static int try_get_int(const json &j, const std::vector<std::string> &candidates, int def=0) {
    for (auto &k : candidates) if (j.contains(k) && !j[k].is_null()) {
        const json &v = j[k];
        if (v.is_number_integer()) return v.get<int>();
        if (v.is_number_float()) return static_cast<int>(v.get<double>());
        if (v.is_string()) {
            try { return std::stoi(v.get<std::string>()); } catch(...) {}
        }
    }
    return def;
}

static bool try_get_bool(const json &j, const std::vector<std::string> &candidates, bool def=false) {
    for (auto &k : candidates) if (j.contains(k) && !j[k].is_null()) {
        const json &v = j[k];
        if (v.is_boolean()) return v.get<bool>();
        if (v.is_number_integer()) return v.get<int>() != 0;
        if (v.is_string()) {
            std::string s = v.get<std::string>();
            for (auto &c: s) c = tolower(c);
            if (s=="true" || s=="1" || s=="yes") return true;
            if (s=="false" || s=="0" || s=="no") return false;
        }
    }
    return def;
}

bool IHTC_Data::loadInstance(const std::string &path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Failed to open instance: " << path << "\n";
        return false;
    }

    // read whole file
    std::stringstream ss;
    ss << in.rdbuf();
    raw_json_text = ss.str();

    if (raw_json_text.empty()) {
        std::cerr << "Instance file is empty: " << path << "\n";
        return false;
    }

    json j;
    try {
        j = json::parse(raw_json_text);
    } catch (const std::exception &e) {
        std::cerr << "JSON parse error: " << e.what() << "\n";
        return false;
    }

    // clear previous
    patients.clear();
    rooms.clear();
    nurses.clear();
    surgeons.clear();
    ots.clear();
    weights.clear();
    D = 0;

    // Horizon / meta
    if (j.contains("D") && j["D"].is_number_integer()) D = j["D"].get<int>();
    else if (j.contains("horizon") && j["horizon"].is_number_integer()) D = j["horizon"].get<int>();
    else if (j.contains("HORIZON") && j["HORIZON"].is_object()) {
        const auto &h = j["HORIZON"];
        if (h.contains("D") && h["D"].is_number_integer()) D = h["D"].get<int>();
        else if (h.contains("days") && h["days"].is_number_integer()) D = h["days"].get<int>();
    }

    if (j.contains("shifts_per_day")) shifts_per_day = try_get_int(j, {"shifts_per_day","shiftsPerDay","S"}, shifts_per_day);

    // Operating theatres (OT)
    if (j.contains("ots") && j["ots"].is_array()) {
        for (const auto &jo : j["ots"]) {
            OT o;
            o.id = try_get_string(jo, {"id","ot_id","otId","name"}, "");
            o.daily_capacity = try_get_int(jo, {"daily_capacity","capacity","cap","dailyCap"}, 0);
            if (jo.contains("unavailable_days") && jo["unavailable_days"].is_array()) {
                for (auto &d : jo["unavailable_days"]) if (d.is_number_integer()) o.unavailable_days.push_back(d.get<int>());
            }
            ots.push_back(std::move(o));
        }
    }

    // Rooms
    if (j.contains("rooms") && j["rooms"].is_array()) {
        for (const auto &jr : j["rooms"]) {
            Room r;
            r.id = try_get_string(jr, {"id","room_id","roomId","name"}, "");
            r.capacity = try_get_int(jr, {"capacity","beds","size"}, 1);
            if (jr.contains("incompatible") && jr["incompatible"].is_array()) {
                for (auto &x : jr["incompatible"]) if (x.is_string()) r.incompatible_patients.push_back(x.get<std::string>());
            }
            if (jr.contains("unavailable_days") && jr["unavailable_days"].is_array()) {
                for (auto &d : jr["unavailable_days"]) if (d.is_number_integer()) r.unavailable_days.push_back(d.get<int>());
            }
            rooms.push_back(std::move(r));
        }
    }

    // Nurses
    if (j.contains("nurses") && j["nurses"].is_array()) {
        for (const auto &jn : j["nurses"]) {
            Nurse n;
            n.id = try_get_string(jn, {"id","nurse_id","nurseId","name"}, "");
            n.level = try_get_int(jn, {"level","competence","skillLevel"}, 0);
            n.max_load = try_get_int(jn, {"max_load","maxLoad","max"}, 0);
            if (jn.contains("roster") && jn["roster"].is_array()) {
                for (auto &r : jn["roster"]) n.roster.push_back(r.is_number() ? r.get<int>() : 0);
            }
            nurses.push_back(std::move(n));
        }
    }

    // Surgeons
    if (j.contains("surgeons") && j["surgeons"].is_array()) {
        for (const auto &js : j["surgeons"]) {
            Surgeon s;
            s.id = try_get_string(js, {"id","surgeon_id","surgeonId","name"}, "");
            s.max_daily_time = try_get_int(js, {"max_daily_time","max","daily_time"}, 0);
            surgeons.push_back(std::move(s));
        }
    }

    // Patients (core fields)
    if (j.contains("patients") && j["patients"].is_array()) {
        for (const auto &jp : j["patients"]) {
            Patient p;
            p.raw_json = jp;
            p.id = try_get_string(jp, {"id","patient_id","patientId","name"}, "");
            p.mandatory = try_get_bool(jp, {"mandatory","isMandatory"}, false);
            // some instances use "optional" instead
            p.optional = try_get_bool(jp, {"optional","isOptional"}, false);
            p.release_date = try_get_int(jp, {"releaseDate","release_date","release","earliest"}, 0);
            p.due_date = try_get_int(jp, {"dueDate","due_date","due","latest"}, 0);
            p.length_of_stay = try_get_int(jp, {"lengthOfStay","length_of_stay","los","stay"}, 1);
            p.age_group = try_get_int(jp, {"age_group","ageGroup","age"}, -1);
            p.sex = try_get_string(jp, {"sex","gender"}, "");
            p.surgery_time = try_get_int(jp, {"surgery_time","surgeryTime","surgery","operating_time","operatingTime"}, 0);
            p.surgeon_id = try_get_string(jp, {"surgeon","surgeon_id","surgeonId","assigned_surgeon"}, "");
            p.min_nurse_level = try_get_int(jp, {"min_nurse_level","minLevel","required_nurse_level"}, 0);

            // nurse load per shift/turn may be named variously
            if (jp.contains("nurse_load") && jp["nurse_load"].is_array()) {
                for (auto &v : jp["nurse_load"]) if (v.is_number()) p.nurse_load_per_shift.push_back(v.get<int>());
            } else if (jp.contains("loads") && jp["loads"].is_array()) {
                for (auto &v : jp["loads"]) if (v.is_number()) p.nurse_load_per_shift.push_back(v.get<int>());
            } else if (jp.contains("workload") && jp["workload"].is_array()) {
                for (auto &v : jp["workload"]) if (v.is_number()) p.nurse_load_per_shift.push_back(v.get<int>());
            }

            if (jp.contains("incompatible_rooms") && jp["incompatible_rooms"].is_array()) {
                for (auto &x : jp["incompatible_rooms"]) if (x.is_string()) p.incompatible_rooms.push_back(x.get<std::string>());
            } else if (jp.contains("incompatible") && jp["incompatible"].is_array()) {
                for (auto &x : jp["incompatible"]) if (x.is_string()) p.incompatible_rooms.push_back(x.get<std::string>());
            }

            patients.push_back(std::move(p));
        }
    }

    // Weights
    if (j.contains("weights") && j["weights"].is_object()) {
        for (auto it = j["weights"].begin(); it != j["weights"].end(); ++it) {
            if (it.value().is_number_integer()) weights[it.key()] = it.value().get<int>();
            else if (it.value().is_number()) weights[it.key()] = static_cast<int>(it.value().get<double>());
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

