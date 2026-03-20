#include "io.hh"

#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include "../nlohmann/json.hpp"

namespace { // internal helpers

using json = nlohmann::json;

std::string try_get_string(const json &j, const std::vector<std::string> &candidates, const std::string &def="") {
    for (auto &k : candidates) {
        if (!j.contains(k) || j[k].is_null()) continue;
        if (j[k].is_string()) return j[k].get<std::string>();
        try {
            return j[k].dump();
        } catch (...) {
            return def;
        }
    }
    return def;
}

int try_get_int(const json &j, const std::vector<std::string> &candidates, int def=0) {
    for (auto &k : candidates) {
        if (!j.contains(k) || j[k].is_null()) continue;
        const json &v = j[k];
        if (v.is_number_integer()) return v.get<int>();
        if (v.is_number_float()) return static_cast<int>(v.get<double>());
        if (v.is_string()) {
            try {
                return std::stoi(v.get<std::string>());
            } catch (...) {}
        }
    }
    return def;
}

bool try_get_bool(const json &j, const std::vector<std::string> &candidates, bool def=false) {
    for (auto &k : candidates) {
        if (!j.contains(k) || j[k].is_null()) continue;
        const json &v = j[k];
        if (v.is_boolean()) return v.get<bool>();
        if (v.is_number_integer()) return v.get<int>() != 0;
        if (v.is_string()) {
            std::string s = v.get<std::string>();
            for (auto &c: s) c = (char)std::tolower((unsigned char)c);
            if (s == "true" || s == "1" || s == "yes") return true;
            if (s == "false" || s == "0" || s == "no") return false;
        }
    }
    return def;
}

int parse_age_group(const json &j, const std::vector<std::string> &candidates,
                    std::unordered_map<std::string, int> &age_group_idx_by_key,
                    int &next_age_group_idx, int def=-1) {
    for (const auto &k : candidates) {
        if (!j.contains(k) || j[k].is_null()) continue;
        const json &v = j[k];

        std::string key;
        if (v.is_number_integer()) {
            key = std::to_string(v.get<int>());
        } else if (v.is_number_float()) {
            key = std::to_string(static_cast<int>(v.get<double>()));
        } else if (v.is_string()) {
            key = v.get<std::string>();
        } else {
            continue;
        }

        auto it = age_group_idx_by_key.find(key);
        if (it != age_group_idx_by_key.end()) return it->second;

        int idx = next_age_group_idx++;
        age_group_idx_by_key[key] = idx;
        return idx;
    }
    return def;
}

Gender parse_gender(const std::string &value) {
    if (value == "A") return Gender::A;
    if (value == "B") return Gender::B;
    return Gender::NONE;
}

} // namespace

namespace jsonio {

bool load_instance(IHTC_Input &in, const std::string &path) {
    std::ifstream file(path);
    if (!file) return false;
    std::stringstream ss;
    ss << file.rdbuf();
    in.raw_json_text = ss.str();
    if (in.raw_json_text.empty()) return false;
    json j = json::parse(in.raw_json_text);

    in.patients.clear();
    in.rooms.clear();
    in.nurses.clear();
    in.surgeons.clear();
    in.occupants.clear();
    in.ots.clear();

    in.D = j["days"].get<int>();
    in.shifts_per_day = j["shift_types"].size();

    for (const auto &jo : j["operating_theaters"]) {
        OT o;
        o.id = jo["id"].get<std::string>();
        for (const auto &v : jo["availability"]) o.availability.push_back(v.get<int>());
        in.ots.push_back(std::move(o));
    }

    for (const auto &jr : j["rooms"]) {
        Room r;
        r.id = jr["id"].get<std::string>();
        r.capacity = jr["capacity"].get<int>();
        in.rooms.push_back(std::move(r));
    }

    for (const auto &jn : j["nurses"]) {
        Nurse n;
        n.id = jn["id"].get<std::string>();
        n.level = jn["skill_level"].get<int>();
        for (const auto &ws : jn["working_shifts"]) {
            WorkingShift w;
            w.day = ws["day"].get<int>();
            std::string sname = ws["shift"].get<std::string>();
            if (sname == "late") w.shift = 1;
            else if (sname == "night") w.shift = 2;
            else w.shift = 0;
            w.max_load = ws["max_load"].get<int>();
            n.working_shifts.push_back(w);
        }
        in.nurses.push_back(std::move(n));
    }

    for (const auto &js : j["surgeons"]) {
        Surgeon s;
        s.id = js["id"].get<std::string>();
        for (const auto &v : js["max_surgery_time"]) s.max_surgery_time.push_back(v.get<int>());
        in.surgeons.push_back(std::move(s));
    }

    std::vector<std::string> age_groups;
    for (const auto &ag : j["age_groups"]) age_groups.push_back(ag.get<std::string>());

    for (const auto &jp : j["patients"]) {
        Patient p;
        p.id = jp["id"].get<std::string>();
        p.mandatory = jp["mandatory"].get<bool>();
        p.release_date = jp["surgery_release_day"].get<int>();
        p.due_date = jp["surgery_due_day"].get<int>();
        p.length_of_stay = jp["length_of_stay"].get<int>();
        p.age_group = std::distance(age_groups.begin(), std::find(age_groups.begin(), age_groups.end(), jp["age_group"].get<std::string>()));
        std::string sex = jp["gender"].get<std::string>();
        p.sex = (sex == "A") ? Gender::A : (sex == "B") ? Gender::B : Gender::NONE;
        p.surgery_time = jp["surgery_duration"].get<int>();
        std::string surgeon_id = jp["surgeon_id"].get<std::string>();
        p.surgeon_idx = -1;
        for (size_t i = 0; i < in.surgeons.size(); ++i) if (in.surgeons[i].id == surgeon_id) p.surgeon_idx = (int)i;
        for (const auto &v : jp["workload_produced"]) p.nurse_load_per_shift.push_back(v.get<int>());
        for (const auto &v : jp["skill_level_required"]) p.skill_level_required_per_shift.push_back(v.get<int>());
        for (const auto &x : jp["incompatible_room_ids"]) {
            for (size_t i = 0; i < in.rooms.size(); ++i) {
                if (in.rooms[i].id == x.get<std::string>()) p.incompatible_room_idxs.push_back((int)i);
            }
        }
        in.patients.push_back(std::move(p));
    }

    for (const auto &jo : j["occupants"]) {
        Occupant o;
        o.id = jo["id"].get<std::string>();
        std::string room_id = jo["room_id"].get<std::string>();
        o.room_idx = -1;
        for (size_t i = 0; i < in.rooms.size(); ++i) if (in.rooms[i].id == room_id) o.room_idx = (int)i;
        std::string sex = jo["gender"].get<std::string>();
        o.sex = (sex == "A") ? Gender::A : (sex == "B") ? Gender::B : Gender::NONE;
        o.length_of_stay = jo["length_of_stay"].get<int>();
        o.age_group = std::distance(age_groups.begin(), std::find(age_groups.begin(), age_groups.end(), jo["age_group"].get<std::string>()));
        for (const auto &v : jo["workload_produced"]) o.nurse_load_per_shift.push_back(v.get<int>());
        for (const auto &v : jo["skill_level_required"]) o.skill_level_required_per_shift.push_back(v.get<int>());
        in.occupants.push_back(std::move(o));
    }

    const auto &wj = j["weights"];
    in.w_room_mixed_age = wj["room_mixed_age"].get<int>();
    in.w_open_operating_theater = wj["open_operating_theater"].get<int>();
    in.w_patient_delay = wj["patient_delay"].get<int>();
    in.w_unscheduled_optional = wj["unscheduled_optional"].get<int>();
    in.w_room_nurse_skill = wj["room_nurse_skill"].get<int>();
    in.w_continuity_of_care = wj["continuity_of_care"].get<int>();
    in.w_nurse_eccessive_workload = wj["nurse_eccessive_workload"].get<int>();
    in.w_surgeon_transfer = wj["surgeon_transfer"].get<int>();

    return true;
}

} // namespace jsonio
