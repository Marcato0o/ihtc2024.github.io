#include "io.hh"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include "../nlohmann/json.hpp"

namespace {

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
    if (!file) {
        std::cerr << "Failed to open instance: " << path << "\n";
        return false;
    }

    std::stringstream ss;
    ss << file.rdbuf();
    in.raw_json_text = ss.str();

    if (in.raw_json_text.empty()) {
        std::cerr << "Instance file is empty: " << path << "\n";
        return false;
    }

    json j;
    try {
        j = json::parse(in.raw_json_text);
    } catch (const std::exception &e) {
        std::cerr << "JSON parse error: " << e.what() << "\n";
        return false;
    }

    in.patients.clear();
    in.rooms.clear();
    in.nurses.clear();
    in.surgeons.clear();
    in.occupants.clear();
    in.ots.clear();
    in.D = 0;

    if (j.contains("D") && j["D"].is_number_integer()) in.D = j["D"].get<int>();
    else if (j.contains("days") && j["days"].is_number_integer()) in.D = j["days"].get<int>();
    else if (j.contains("horizon") && j["horizon"].is_number_integer()) in.D = j["horizon"].get<int>();
    else if (j.contains("HORIZON") && j["HORIZON"].is_object()) {
        const auto &h = j["HORIZON"];
        if (h.contains("D") && h["D"].is_number_integer()) in.D = h["D"].get<int>();
        else if (h.contains("days") && h["days"].is_number_integer()) in.D = h["days"].get<int>();
    }

    if (j.contains("shifts_per_day")) in.shifts_per_day = try_get_int(j, {"shifts_per_day","shiftsPerDay","S"}, in.shifts_per_day);
    else if (j.contains("shift_types") && j["shift_types"].is_array()) in.shifts_per_day = (int)j["shift_types"].size();

    if (j.contains("ots") && j["ots"].is_array()) {
        // Fallback backward-compatibility / fallback block not needed, but we'll try to adapt if exists
        for (const auto &jo : j["ots"]) {
            OT o;
            o.id = try_get_string(jo, {"id","ot_id","otId","name"}, "");
            if (jo.contains("availability") && jo["availability"].is_array()) {
                o.availability.assign(jo["availability"].size(), 0);
                for (size_t d = 0; d < jo["availability"].size(); ++d) {
                    o.availability[d] = jo["availability"][d].is_number_integer() ? jo["availability"][d].get<int>() : 0;
                }
            } else {
                int def_cap = try_get_int(jo, {"daily_capacity","capacity","cap","dailyCap"}, 0);
                if (in.D > 0) o.availability.assign(in.D, def_cap);
            }
            in.ots.push_back(std::move(o));
        }
    } else if (j.contains("operating_theaters") && j["operating_theaters"].is_array()) {
        for (const auto &jo : j["operating_theaters"]) {
            OT o;
            o.id = try_get_string(jo, {"id","ot_id","otId","name"}, "");
            if (jo.contains("availability") && jo["availability"].is_array()) {
                o.availability.assign(jo["availability"].size(), 0);
                for (size_t d = 0; d < jo["availability"].size(); ++d) {
                    o.availability[d] = jo["availability"][d].is_number_integer() ? jo["availability"][d].get<int>() : 0;
                }
                if (in.D == 0) in.D = (int)o.availability.size();
            } else {
                int def_cap = try_get_int(jo, {"daily_capacity","capacity","cap","dailyCap"}, 0);
                if (in.D > 0) o.availability.assign(in.D, def_cap);
            }
            in.ots.push_back(std::move(o));
        }
    }

    if (j.contains("rooms") && j["rooms"].is_array()) {
        for (const auto &jr : j["rooms"]) {
            Room r;
            r.id = try_get_string(jr, {"id","room_id","roomId","name"}, "");
            r.capacity = try_get_int(jr, {"capacity","beds","size"}, 1);
            in.rooms.push_back(std::move(r));
        }
    }

    std::unordered_map<std::string, int> room_idx_by_id;
    room_idx_by_id.reserve(in.rooms.size());
    for (int i = 0; i < (int)in.rooms.size(); ++i) room_idx_by_id[in.rooms[i].id] = i;

    if (j.contains("nurses") && j["nurses"].is_array()) {
        for (const auto &jn : j["nurses"]) {
            Nurse n;
            n.id = try_get_string(jn, {"id","nurse_id","nurseId","name"}, "");
            n.level = try_get_int(jn, {"level","competence","skillLevel","skill_level"}, 0);
            if (jn.contains("working_shifts") && jn["working_shifts"].is_array()) {
                for (const auto &ws : jn["working_shifts"]) {
                    WorkingShift w;
                    w.day = (ws.contains("day") && ws["day"].is_number_integer()) ? ws["day"].get<int>() : 0;
                    std::string sname = (ws.contains("shift") && ws["shift"].is_string()) ? ws["shift"].get<std::string>() : "early";
                    if (sname == "late") w.shift = 1;
                    else if (sname == "night") w.shift = 2;
                    else w.shift = 0;
                    w.max_load = (ws.contains("max_load") && ws["max_load"].is_number_integer()) ? ws["max_load"].get<int>() : 9999;
                    n.working_shifts.push_back(w);
                }
            }
            in.nurses.push_back(std::move(n));
        }
    }

    if (j.contains("surgeons") && j["surgeons"].is_array()) {
        for (const auto &js : j["surgeons"]) {
            Surgeon s;
            s.id = try_get_string(js, {"id","surgeon_id","surgeonId","name"}, "");
            if (js.contains("max_surgery_time") && js["max_surgery_time"].is_array()) {
                s.max_surgery_time.assign(js["max_surgery_time"].size(), 0);
                for (size_t d = 0; d < js["max_surgery_time"].size(); ++d) {
                    const auto &v = js["max_surgery_time"][d];
                    if (v.is_number_integer()) s.max_surgery_time[d] = v.get<int>();
                }
            } else {
                int def_time = try_get_int(js, {"max_daily_time","max","daily_time"}, 0);
                if (in.D > 0) s.max_surgery_time.assign(in.D, def_time);
            }
            in.surgeons.push_back(std::move(s));
        }
    }

    std::unordered_map<std::string, int> surgeon_idx_by_id;
    surgeon_idx_by_id.reserve(in.surgeons.size());
    for (int i = 0; i < (int)in.surgeons.size(); ++i) surgeon_idx_by_id[in.surgeons[i].id] = i;

    std::unordered_map<std::string, int> age_group_idx_by_key;
    int next_age_group_idx = 0;

    if (j.contains("patients") && j["patients"].is_array()) {
        for (const auto &jp : j["patients"]) {
            Patient p;
            p.id = try_get_string(jp, {"id","patient_id","patientId","name"}, "");
            p.mandatory = try_get_bool(jp, {"mandatory","isMandatory"}, false);
            p.release_date = try_get_int(jp, {"releaseDate","release_date","release","earliest","surgery_release_day"}, 0);
            p.due_date = try_get_int(jp, {"dueDate","due_date","due","latest","surgery_due_day"}, in.D > 0 ? (in.D - 1) : 0);
            p.length_of_stay = try_get_int(jp, {"lengthOfStay","length_of_stay","los","stay"}, 1);
            p.age_group = parse_age_group(jp, {"age_group","ageGroup","age"}, age_group_idx_by_key, next_age_group_idx, -1);
            p.sex = parse_gender(try_get_string(jp, {"sex","gender"}, ""));
            p.surgery_time = try_get_int(jp, {"surgery_time","surgeryTime","surgery","operating_time","operatingTime","surgery_duration"}, 0);
            std::string surgeon_id = try_get_string(jp, {"surgeon","surgeon_id","surgeonId","assigned_surgeon"}, "");
            auto surgeon_it = surgeon_idx_by_id.find(surgeon_id);
            p.surgeon_idx = (surgeon_it != surgeon_idx_by_id.end()) ? surgeon_it->second : -1;
            p.min_nurse_level = try_get_int(jp, {"min_nurse_level","minLevel","required_nurse_level"}, 0);

            if (jp.contains("nurse_load") && jp["nurse_load"].is_array()) {
                for (auto &v : jp["nurse_load"]) if (v.is_number()) p.nurse_load_per_shift.push_back(v.get<int>());
            } else if (jp.contains("loads") && jp["loads"].is_array()) {
                for (auto &v : jp["loads"]) if (v.is_number()) p.nurse_load_per_shift.push_back(v.get<int>());
            } else if (jp.contains("workload") && jp["workload"].is_array()) {
                for (auto &v : jp["workload"]) if (v.is_number()) p.nurse_load_per_shift.push_back(v.get<int>());
            } else if (jp.contains("workload_produced") && jp["workload_produced"].is_array()) {
                for (auto &v : jp["workload_produced"]) if (v.is_number()) p.nurse_load_per_shift.push_back(v.get<int>());
            }

            if (jp.contains("incompatible_rooms") && jp["incompatible_rooms"].is_array()) {
                for (auto &x : jp["incompatible_rooms"]) {
                    if (!x.is_string()) continue;
                    auto it = room_idx_by_id.find(x.get<std::string>());
                    if (it != room_idx_by_id.end()) p.incompatible_room_idxs.push_back(it->second);
                }
            } else if (jp.contains("incompatible") && jp["incompatible"].is_array()) {
                for (auto &x : jp["incompatible"]) {
                    if (!x.is_string()) continue;
                    auto it = room_idx_by_id.find(x.get<std::string>());
                    if (it != room_idx_by_id.end()) p.incompatible_room_idxs.push_back(it->second);
                }
            } else if (jp.contains("incompatible_room_ids") && jp["incompatible_room_ids"].is_array()) {
                for (auto &x : jp["incompatible_room_ids"]) {
                    if (!x.is_string()) continue;
                    auto it = room_idx_by_id.find(x.get<std::string>());
                    if (it != room_idx_by_id.end()) p.incompatible_room_idxs.push_back(it->second);
                }
            }

            if (p.min_nurse_level == 0 && jp.contains("skill_level_required") && jp["skill_level_required"].is_array()) {
                int req = 0;
                for (const auto &v : jp["skill_level_required"]) if (v.is_number_integer()) req = std::max(req, v.get<int>());
                p.min_nurse_level = req;
            }

            in.patients.push_back(std::move(p));
        }
    }

    if (j.contains("occupants") && j["occupants"].is_array()) {
        for (const auto &jo : j["occupants"]) {
            Occupant o;
            o.id = try_get_string(jo, {"id","occupant_id","name"}, "");
            std::string room_id = try_get_string(jo, {"room_id","roomId","room"}, "");
            auto room_it = room_idx_by_id.find(room_id);
            o.room_idx = (room_it != room_idx_by_id.end()) ? room_it->second : -1;
            o.sex = parse_gender(try_get_string(jo, {"sex","gender"}, ""));
            o.admission_day = try_get_int(jo, {"admission_day","admissionDay","day"}, 0);
            o.length_of_stay = try_get_int(jo, {"lengthOfStay","length_of_stay","los","stay"}, 1);

            if (jo.contains("workload_produced") && jo["workload_produced"].is_array()) {
                for (const auto &v : jo["workload_produced"]) if (v.is_number()) o.nurse_load_per_shift.push_back(v.get<int>());
            }
            if (jo.contains("skill_level_required") && jo["skill_level_required"].is_array()) {
                int req = 0;
                for (const auto &v : jo["skill_level_required"]) if (v.is_number_integer()) req = std::max(req, v.get<int>());
                o.min_nurse_level = req;
            }
            in.occupants.push_back(std::move(o));
        }
    }

    if (j.contains("weights") && j["weights"].is_object()) {
        auto& wj = j["weights"];
        auto readW = [&](const char* key, int& dest) {
            if (wj.contains(key) && wj[key].is_number()) dest = wj[key].get<int>();
        };
        readW("room_mixed_age",          in.w_room_mixed_age);
        readW("open_operating_theater",  in.w_open_operating_theater);
        readW("patient_delay",           in.w_patient_delay);
        readW("unscheduled_optional",    in.w_unscheduled_optional);
        readW("room_nurse_skill",        in.w_room_nurse_skill);
        readW("continuity_of_care",      in.w_continuity_of_care);
        readW("nurse_eccessive_workload",in.w_nurse_eccessive_workload);
        readW("surgeon_transfer",        in.w_surgeon_transfer);
    }

    return true;
}

} // namespace jsonio
