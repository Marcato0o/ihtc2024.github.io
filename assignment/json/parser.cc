#include "io.hh"

#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include "../nlohmann/json.hpp"

namespace {

using json = nlohmann::json;

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

    // Clear all collections before filling.
    in.patients.clear();
    in.rooms.clear();
    in.nurses.clear();
    in.surgeons.clear();
    in.occupants.clear();
    in.ots.clear();
    in.shift_types.clear();
    in.age_groups.clear();

    // -----------------------------------------------------------------------
    // 1. Scalars
    // -----------------------------------------------------------------------
    in.D            = j["days"].get<int>();
    in.skill_levels = j.value("skill_levels", 0);

    // -----------------------------------------------------------------------
    // 2. shift_types → canonical shift index map
    //    The integer index of each name in this array IS the shift integer used
    //    everywhere in the solver (0 = first shift, 1 = second shift, …).
    // -----------------------------------------------------------------------
    for (const auto &s : j["shift_types"])
        in.shift_types.push_back(s.get<std::string>());
    in.shifts_per_day = (int)in.shift_types.size();

    std::unordered_map<std::string, int> shift_idx;
    for (int i = 0; i < (int)in.shift_types.size(); ++i)
        shift_idx[in.shift_types[i]] = i;

    // -----------------------------------------------------------------------
    // 3. age_groups → canonical age-group index map
    //    The integer index of each name in this array IS the age_group integer
    //    stored in Patient::age_group and Occupant::age_group.
    //    This ordering must match what the validator uses for S1 (AgeMix).
    // -----------------------------------------------------------------------
    for (const auto &ag : j["age_groups"])
        in.age_groups.push_back(ag.get<std::string>());

    std::unordered_map<std::string, int> age_group_idx;
    for (int i = 0; i < (int)in.age_groups.size(); ++i)
        age_group_idx[in.age_groups[i]] = i;

    // -----------------------------------------------------------------------
    // 4. Weights (always present; nlohmann key lookup is order-independent)
    // -----------------------------------------------------------------------
    {
        const auto &w = j["weights"];
        auto readW = [&](const char *key, int &dest) {
            if (w.contains(key) && w[key].is_number()) dest = w[key].get<int>();
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

    // -----------------------------------------------------------------------
    // 5. Operating theaters
    // -----------------------------------------------------------------------
    for (const auto &jo : j["operating_theaters"]) {
        OT o;
        o.id = jo["id"].get<std::string>();
        for (const auto &v : jo["availability"])
            o.availability.push_back(v.get<int>());
        in.ots.push_back(std::move(o));
    }

    // -----------------------------------------------------------------------
    // 6. Rooms
    // -----------------------------------------------------------------------
    for (const auto &jr : j["rooms"]) {
        Room r;
        r.id       = jr["id"].get<std::string>();
        r.capacity = jr["capacity"].get<int>();
        in.rooms.push_back(std::move(r));
    }

    // Build room id → index map (used by patients and occupants below).
    std::unordered_map<std::string, int> room_idx_by_id;
    room_idx_by_id.reserve(in.rooms.size());
    for (int i = 0; i < (int)in.rooms.size(); ++i)
        room_idx_by_id[in.rooms[i].id] = i;

    // -----------------------------------------------------------------------
    // 7. Nurses
    // -----------------------------------------------------------------------
    for (const auto &jn : j["nurses"]) {
        Nurse n;
        n.id    = jn["id"].get<std::string>();
        n.level = jn["skill_level"].get<int>();
        for (const auto &ws : jn["working_shifts"]) {
            WorkingShift w;
            w.day      = ws["day"].get<int>();
            w.shift    = shift_idx.at(ws["shift"].get<std::string>());
            w.max_load = ws["max_load"].get<int>();
            n.working_shifts.push_back(w);
        }
        in.nurses.push_back(std::move(n));
    }

    // -----------------------------------------------------------------------
    // 8. Surgeons
    // -----------------------------------------------------------------------
    for (const auto &js : j["surgeons"]) {
        Surgeon s;
        s.id = js["id"].get<std::string>();
        for (const auto &v : js["max_surgery_time"])
            s.max_surgery_time.push_back(v.get<int>());
        in.surgeons.push_back(std::move(s));
    }

    // Build surgeon id → index map (used by patients below).
    std::unordered_map<std::string, int> surgeon_idx_by_id;
    surgeon_idx_by_id.reserve(in.surgeons.size());
    for (int i = 0; i < (int)in.surgeons.size(); ++i)
        surgeon_idx_by_id[in.surgeons[i].id] = i;

    // -----------------------------------------------------------------------
    // 9. Patients
    // -----------------------------------------------------------------------
    for (const auto &jp : j["patients"]) {
        Patient p;
        p.id            = jp["id"].get<std::string>();
        p.mandatory     = jp["mandatory"].get<bool>();
        p.sex           = parse_gender(jp["gender"].get<std::string>());
        p.age_group     = age_group_idx.at(jp["age_group"].get<std::string>());
        p.length_of_stay= jp["length_of_stay"].get<int>();
        p.release_date  = jp["surgery_release_day"].get<int>();
        // surgery_due_day is only present for mandatory patients; optional patients
        // have no hard deadline so they default to the last day of the horizon.
        p.due_date      = jp.value("surgery_due_day", in.D - 1);
        p.surgery_time  = jp["surgery_duration"].get<int>();

        const std::string &sid = jp["surgeon_id"].get<std::string>();
        auto it = surgeon_idx_by_id.find(sid);
        p.surgeon_idx = (it != surgeon_idx_by_id.end()) ? it->second : -1;

        for (const auto &x : jp["incompatible_room_ids"]) {
            auto rit = room_idx_by_id.find(x.get<std::string>());
            if (rit != room_idx_by_id.end())
                p.incompatible_room_idxs.push_back(rit->second);
        }

        for (const auto &v : jp["workload_produced"])
            p.nurse_load_per_shift.push_back(v.get<int>());

        for (const auto &v : jp["skill_level_required"])
            p.skill_level_required_per_shift.push_back(v.get<int>());

        in.patients.push_back(std::move(p));
    }

    // -----------------------------------------------------------------------
    // 10. Occupants
    // -----------------------------------------------------------------------
    for (const auto &jo : j["occupants"]) {
        Occupant o;
        o.id            = jo["id"].get<std::string>();
        o.sex           = parse_gender(jo["gender"].get<std::string>());
        o.age_group     = age_group_idx.at(jo["age_group"].get<std::string>());
        o.length_of_stay= jo["length_of_stay"].get<int>();

        const std::string &rid = jo["room_id"].get<std::string>();
        auto rit = room_idx_by_id.find(rid);
        o.room_idx = (rit != room_idx_by_id.end()) ? rit->second : -1;

        for (const auto &v : jo["workload_produced"])
            o.nurse_load_per_shift.push_back(v.get<int>());

        for (const auto &v : jo["skill_level_required"])
            o.skill_level_required_per_shift.push_back(v.get<int>());

        in.occupants.push_back(std::move(o));
    }

    return true;
}

} // namespace jsonio
