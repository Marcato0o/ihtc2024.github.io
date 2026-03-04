#include "IHTC_Data.hh"
#include <fstream>
#include <iostream>
#include <sstream>
#include <set>
#include <unordered_map>
#include "nlohmann/json.hpp"

using namespace std;
using json = nlohmann::json;

IHTC_Input::IHTC_Input(const std::string &file_name) {
    loadInstance(file_name);
}

IHTC_Output::IHTC_Output(const IHTC_Input &in) {
    BindInput(in);
    int days = in.D > 0 ? in.D : 1;
    init(in.patients.size(), in.rooms.size(), in.ots.size(), days);
}

void IHTC_Output::BindInput(const IHTC_Input &in) {
    bound_input = &in;
}

void IHTC_Output::init(size_t num_patients, size_t num_rooms, size_t num_ots, int days) {
    admitted.assign(num_patients, false);
    admit_day.assign(num_patients, -1);
    room_assigned_idx.assign(num_patients, -1);
    ot_assigned_idx.assign(num_patients, -1);
    room_occupancy.assign(num_rooms, std::vector<int>(days, 0));
    ot_minutes_used.assign(num_ots, std::vector<int>(days, 0));
    surgeon_minutes_used.assign(0, std::vector<int>());
    room_gender.assign(num_rooms, std::vector<std::string>(days, ""));
}

bool IHTC_Output::canAssignPatient(int patient_id, int day, int room_idx, int ot_idx, const IHTC_Input &in) const {
    if (room_idx < 0 || room_idx >= (int)room_occupancy.size()) return false;
    if (day < 0 || day >= (int)room_occupancy[0].size()) return false;
    const Patient &p = in.patients[patient_id];
    const Room &r = in.rooms[room_idx];

    if (day < p.release_date) return false;
    if (p.mandatory && day > p.due_date) return false;

    int los = std::max(1, p.length_of_stay);
    int days = (int)room_occupancy[0].size();

    for (int dd = 0; dd < los; ++dd) {
        int d_idx = day + dd;
        if (d_idx < 0 || d_idx >= days) break;
        if (room_occupancy[room_idx][d_idx] >= r.capacity) return false;
        if (!p.sex.empty()) {
            const std::string &g = room_gender[room_idx][d_idx];
            if (!g.empty() && g != p.sex) return false;
        }
    }

    for (const auto &bad : p.incompatible_rooms) {
        if (bad == r.id) return false;
    }

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

    if (!p.surgeon_id.empty()) {
        int surgeon_idx = -1;
        for (int i = 0; i < (int)in.surgeons.size(); ++i) {
            if (in.surgeons[i].id == p.surgeon_id) {
                surgeon_idx = i;
                break;
            }
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

void IHTC_Output::assignPatient(int patient_id, int day, int room_idx, int ot_idx, const IHTC_Input &in) {
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
        if (day >= 0 && day < days_ot) {
            ot_minutes_used[ot_idx][day] += in.patients[patient_id].surgery_time;
        }
    }

    if (!in.patients[patient_id].surgeon_id.empty()) {
        int surgeon_idx = -1;
        for (int i = 0; i < (int)in.surgeons.size(); ++i) {
            if (in.surgeons[i].id == in.patients[patient_id].surgeon_id) {
                surgeon_idx = i;
                break;
            }
        }
        if (surgeon_idx >= 0) {
            if (surgeon_minutes_used.size() != in.surgeons.size()) {
                surgeon_minutes_used.assign(in.surgeons.size(), std::vector<int>(days, 0));
            }
            if (day >= 0 && day < days) {
                surgeon_minutes_used[surgeon_idx][day] += in.patients[patient_id].surgery_time;
            }
        }
    }
}

int IHTC_Output::getRoomOccupancy(int room_idx, int day) const {
    if (room_idx < 0 || room_idx >= (int)room_occupancy.size()) return 0;
    if (day < 0 || day >= (int)room_occupancy[0].size()) return 0;
    return room_occupancy[room_idx][day];
}

int IHTC_Output::getOtMinutesUsed(int ot_idx, int day) const {
    if (ot_idx < 0 || ot_idx >= (int)ot_minutes_used.size()) return 0;
    if (day < 0 || day >= (int)ot_minutes_used[0].size()) return 0;
    return ot_minutes_used[ot_idx][day];
}

namespace {

static std::string shiftNameFromIndex(int idx) {
    if (idx == 0) return "early";
    if (idx == 1) return "late";
    return "night";
}

static int shiftIndexFromName(const std::string &name) {
    if (name == "early") return 0;
    if (name == "late") return 1;
    if (name == "night") return 2;
    return 0;
}

static std::string ageGroupKey(const Patient &p) {
    if (p.raw_json.contains("age_group") && !p.raw_json["age_group"].is_null()) {
        if (p.raw_json["age_group"].is_string()) return p.raw_json["age_group"].get<std::string>();
        if (p.raw_json["age_group"].is_number_integer()) return std::to_string(p.raw_json["age_group"].get<int>());
    }
    if (p.age_group >= 0) return std::to_string(p.age_group);
    return "unknown";
}

static int getWeight(const IHTC_Input &in, const std::string &named_key, const std::string &short_key, int fallback) {
    auto it_named = in.weights.find(named_key);
    if (it_named != in.weights.end()) return it_named->second;
    auto it_short = in.weights.find(short_key);
    if (it_short != in.weights.end()) return it_short->second;
    return fallback;
}

struct SoftCostContext {
    int days = 1;
    int shifts = 1;
    int total_delay = 0;
    int unscheduled_optional_count = 0;
    std::vector<std::set<std::string>> occupied_rooms_by_day;
    std::vector<std::set<std::string>> ot_by_day;
    std::vector<std::vector<std::vector<int>>> room_shift_load;
    std::vector<std::vector<std::vector<int>>> room_shift_skill;
    std::vector<std::vector<std::vector<int>>> patients_in_room_day;
    std::vector<int> nurse_level;
    std::vector<int> nurse_max_load;
    std::vector<std::vector<int>> nurse_load_by_shift;
    std::vector<std::vector<std::vector<std::vector<int>>>> room_shift_nurses;
};

static SoftCostContext buildSoftCostContext(const IHTC_Input &in, const IHTC_Output &out) {
    SoftCostContext ctx;
    ctx.days = in.D > 0 ? in.D : 1;
    ctx.shifts = std::max(1, in.shifts_per_day);

    ctx.occupied_rooms_by_day.assign(ctx.days, {});
    ctx.ot_by_day.assign(ctx.days, {});
    ctx.room_shift_load.assign(ctx.days, std::vector<std::vector<int>>(ctx.shifts, std::vector<int>(in.rooms.size(), 0)));
    ctx.room_shift_skill.assign(ctx.days, std::vector<std::vector<int>>(ctx.shifts, std::vector<int>(in.rooms.size(), 0)));
    ctx.patients_in_room_day.assign(in.rooms.size(), std::vector<std::vector<int>>(ctx.days));

    std::unordered_map<std::string, int> room_name_to_idx;
    for (int i = 0; i < (int)in.rooms.size(); ++i) room_name_to_idx[in.rooms[i].id] = i;

    for (const auto &f : in.occupants) {
        auto it = room_name_to_idx.find(f.room_id);
        if (it == room_name_to_idx.end()) continue;
        int ridx = it->second;
        int start = std::max(0, f.admission_day);
        int los = std::max(1, f.length_of_stay);
        for (int dd = 0; dd < los; ++dd) {
            int d = start + dd;
            if (d < 0 || d >= ctx.days) continue;
            ctx.occupied_rooms_by_day[d].insert(in.rooms[ridx].id);
            for (int sh = 0; sh < ctx.shifts; ++sh) {
                int idx = dd * ctx.shifts + sh;
                int load = 1;
                if (!f.nurse_load_per_shift.empty()) {
                    if (idx < (int)f.nurse_load_per_shift.size()) load = f.nurse_load_per_shift[idx];
                    else load = f.nurse_load_per_shift.back();
                }
                ctx.room_shift_load[d][sh][ridx] += load;
                ctx.room_shift_skill[d][sh][ridx] = std::max(ctx.room_shift_skill[d][sh][ridx], f.min_nurse_level);
            }
        }
    }

    for (size_t pid = 0; pid < in.patients.size(); ++pid) {
        bool is_admitted = (pid < out.admitted.size() && out.admitted[pid]);
        if (!is_admitted) {
            if (!in.patients[pid].mandatory) ctx.unscheduled_optional_count++;
            continue;
        }

        int admit_day = out.admit_day[pid];
        const auto &p = in.patients[pid];
        int los = std::max(1, p.length_of_stay);
        int room_idx = out.room_assigned_idx[pid];
        if (room_idx >= 0 && room_idx < (int)in.rooms.size()) {
            for (int d = admit_day; d < admit_day + los && d < ctx.days; ++d) {
                if (d >= 0) {
                    ctx.occupied_rooms_by_day[d].insert(in.rooms[room_idx].id);
                    ctx.patients_in_room_day[room_idx][d].push_back((int)pid);
                }
            }

            for (int dd = 0; dd < los; ++dd) {
                int d = admit_day + dd;
                if (d < 0 || d >= ctx.days) continue;
                for (int sh = 0; sh < ctx.shifts; ++sh) {
                    int idx = dd * ctx.shifts + sh;
                    int load = 1;
                    if (!p.nurse_load_per_shift.empty()) {
                        if (idx < (int)p.nurse_load_per_shift.size()) load = p.nurse_load_per_shift[idx];
                        else load = p.nurse_load_per_shift.back();
                    }
                    ctx.room_shift_load[d][sh][room_idx] += load;
                    ctx.room_shift_skill[d][sh][room_idx] = std::max(ctx.room_shift_skill[d][sh][room_idx], p.min_nurse_level);
                }
            }
        }

        if (admit_day >= 0) ctx.total_delay += std::max(0, admit_day - p.release_date);

        int ot_idx = out.ot_assigned_idx[pid];
        if (admit_day >= 0 && admit_day < ctx.days && ot_idx >= 0 && ot_idx < (int)in.ots.size()) {
            ctx.ot_by_day[admit_day].insert(in.ots[ot_idx].id);
        }
    }

    json raw;
    try {
        raw = json::parse(in.raw_json_text);
    } catch (...) {
        raw = json::object();
    }

    int nurse_count = 0;
    if (raw.contains("nurses") && raw["nurses"].is_array()) nurse_count = (int)raw["nurses"].size();
    ctx.nurse_level.assign(nurse_count, 0);
    ctx.nurse_max_load.assign(nurse_count, 9999);
    for (int i = 0; i < nurse_count && i < (int)in.nurses.size(); ++i) {
        ctx.nurse_level[i] = in.nurses[i].level;
        ctx.nurse_max_load[i] = in.nurses[i].max_load;
    }

    ctx.nurse_load_by_shift.assign(nurse_count, std::vector<int>(ctx.days * ctx.shifts, 0));
    ctx.room_shift_nurses.assign(ctx.days, std::vector<std::vector<std::vector<int>>>(ctx.shifts, std::vector<std::vector<int>>(in.rooms.size())));

    if (raw.contains("nurses") && raw["nurses"].is_array()) {
        int nurse_idx = 0;
        for (const auto &jn : raw["nurses"]) {
            if (jn.contains("working_shifts") && jn["working_shifts"].is_array()) {
                for (const auto &ws : jn["working_shifts"]) {
                    int day = (ws.contains("day") && ws["day"].is_number_integer()) ? ws["day"].get<int>() : -1;
                    std::string shift = (ws.contains("shift") && ws["shift"].is_string()) ? ws["shift"].get<std::string>() : "early";
                    int sh_idx = shiftIndexFromName(shift);

                    if (day >= 0 && day < ctx.days && !ctx.occupied_rooms_by_day[day].empty()) {
                        std::vector<std::string> room_names(ctx.occupied_rooms_by_day[day].begin(), ctx.occupied_rooms_by_day[day].end());
                        for (const auto &chosen : room_names) {
                            auto it = room_name_to_idx.find(chosen);
                            if (it != room_name_to_idx.end() && sh_idx >= 0 && sh_idx < ctx.shifts) {
                                int ridx = it->second;
                                ctx.room_shift_nurses[day][sh_idx][ridx].push_back(nurse_idx);
                                if (nurse_idx >= 0 && nurse_idx < nurse_count) {
                                    ctx.nurse_load_by_shift[nurse_idx][day * ctx.shifts + sh_idx] += ctx.room_shift_load[day][sh_idx][ridx];
                                }
                            }
                        }
                    }
                }
            } else if (nurse_idx < (int)in.nurses.size() && !in.nurses[nurse_idx].roster.empty()) {
                const auto &roster = in.nurses[nurse_idx].roster;
                for (int g = 0; g < (int)roster.size(); ++g) {
                    if (roster[g] <= 0) continue;
                    int day = g / std::max(1, in.shifts_per_day);
                    int sh = g % std::max(1, in.shifts_per_day);
                    if (day >= 0 && day < ctx.days && !ctx.occupied_rooms_by_day[day].empty()) {
                        std::vector<std::string> room_names(ctx.occupied_rooms_by_day[day].begin(), ctx.occupied_rooms_by_day[day].end());
                        for (const auto &chosen : room_names) {
                            auto it = room_name_to_idx.find(chosen);
                            if (it != room_name_to_idx.end() && sh >= 0 && sh < ctx.shifts) {
                                int ridx = it->second;
                                ctx.room_shift_nurses[day][sh][ridx].push_back(nurse_idx);
                                ctx.nurse_load_by_shift[nurse_idx][day * ctx.shifts + sh] += ctx.room_shift_load[day][sh][ridx];
                            }
                        }
                    }
                }
            }
            nurse_idx++;
        }
    }

    return ctx;
}

} // namespace

int IHTC_Output::ComputeCostRoomMixedAge() const {
    if (!bound_input) return 0;
    const IHTC_Input &in = *bound_input;
    SoftCostContext ctx = buildSoftCostContext(in, *this);
    int raw_cost = 0;
    for (int r = 0; r < (int)in.rooms.size(); ++r) {
        for (int d = 0; d < ctx.days; ++d) {
            const auto &plist = ctx.patients_in_room_day[r][d];
            if (plist.size() < 2) continue;
            std::unordered_map<std::string, int> age_counts;
            for (int pid : plist) age_counts[ageGroupKey(in.patients[pid])]++;
            long long n = (long long)plist.size();
            long long total_pairs = (n * (n - 1)) / 2;
            long long same_pairs = 0;
            for (const auto &kv : age_counts) {
                long long c = kv.second;
                same_pairs += (c * (c - 1)) / 2;
            }
            raw_cost += (int)std::max(0LL, total_pairs - same_pairs);
        }
    }
    return raw_cost * getWeight(in, "room_mixed_age", "S1", 5);
}

int IHTC_Output::ComputeCostRoomNurseSkill() const {
    if (!bound_input) return 0;
    const IHTC_Input &in = *bound_input;
    SoftCostContext ctx = buildSoftCostContext(in, *this);
    int raw_cost = 0;
    for (int d = 0; d < ctx.days; ++d) {
        for (int sh = 0; sh < ctx.shifts; ++sh) {
            for (int r = 0; r < (int)in.rooms.size(); ++r) {
                int req = ctx.room_shift_skill[d][sh][r];
                if (req <= 0) continue;
                for (int nidx : ctx.room_shift_nurses[d][sh][r]) {
                    if (nidx >= 0 && nidx < (int)ctx.nurse_level.size() && ctx.nurse_level[nidx] < req) {
                        raw_cost += (req - ctx.nurse_level[nidx]);
                    }
                }
            }
        }
    }
    return raw_cost * getWeight(in, "room_nurse_skill", "S2", 1);
}

int IHTC_Output::ComputeCostContinuityOfCare() const {
    if (!bound_input) return 0;
    const IHTC_Input &in = *bound_input;
    SoftCostContext ctx = buildSoftCostContext(in, *this);
    int raw_cost = 0;
    for (size_t pid = 0; pid < in.patients.size(); ++pid) {
        if (!(pid < admitted.size() && admitted[pid])) continue;
        int day0 = admit_day[pid];
        int ridx = room_assigned_idx[pid];
        if (ridx < 0 || ridx >= (int)in.rooms.size()) continue;
        int los = std::max(1, in.patients[pid].length_of_stay);
        std::set<int> seen_nurses;
        for (int dd = 0; dd < los; ++dd) {
            int d = day0 + dd;
            if (d < 0 || d >= ctx.days) continue;
            for (int sh = 0; sh < ctx.shifts; ++sh) {
                for (int nidx : ctx.room_shift_nurses[d][sh][ridx]) seen_nurses.insert(nidx);
            }
        }
        if (!seen_nurses.empty()) raw_cost += std::max(0, (int)seen_nurses.size() - 1);
    }
    return raw_cost * getWeight(in, "continuity_of_care", "S3", 1);
}

int IHTC_Output::ComputeCostNurseExcessiveWorkload() const {
    if (!bound_input) return 0;
    const IHTC_Input &in = *bound_input;
    SoftCostContext ctx = buildSoftCostContext(in, *this);
    int raw_cost = 0;
    for (int nidx = 0; nidx < (int)ctx.nurse_level.size(); ++nidx) {
        int cap = (nidx < (int)ctx.nurse_max_load.size()) ? ctx.nurse_max_load[nidx] : 9999;
        for (int t = 0; t < ctx.days * ctx.shifts; ++t) {
            int over = ctx.nurse_load_by_shift[nidx][t] - cap;
            if (over > 0) raw_cost += over;
        }
    }
    return raw_cost * getWeight(in, "nurse_eccessive_workload", "S4", 1);
}

int IHTC_Output::ComputeCostOpenOperatingTheater() const {
    if (!bound_input) return 0;
    const IHTC_Input &in = *bound_input;
    SoftCostContext ctx = buildSoftCostContext(in, *this);
    int raw_cost = 0;
    for (int d = 0; d < ctx.days; ++d) {
        if (!ctx.ot_by_day[d].empty()) raw_cost += (int)ctx.ot_by_day[d].size();
    }
    return raw_cost * getWeight(in, "open_operating_theater", "S5", 10);
}

int IHTC_Output::ComputeCostSurgeonTransfer() const {
    if (!bound_input) return 0;
    const IHTC_Input &in = *bound_input;
    SoftCostContext ctx = buildSoftCostContext(in, *this);
    int raw_cost = 0;
    std::unordered_map<std::string, std::vector<std::set<std::string>>> surgeon_ot_by_day;
    for (size_t pid = 0; pid < in.patients.size(); ++pid) {
        if (!(pid < admitted.size() && admitted[pid])) continue;
        int d = admit_day[pid];
        int ot_idx = ot_assigned_idx[pid];
        if (d < 0 || d >= ctx.days || ot_idx < 0 || ot_idx >= (int)in.ots.size()) continue;
        const std::string &sid = in.patients[pid].surgeon_id;
        if (sid.empty()) continue;
        if (!surgeon_ot_by_day.count(sid)) surgeon_ot_by_day[sid] = std::vector<std::set<std::string>>(ctx.days);
        surgeon_ot_by_day[sid][d].insert(in.ots[ot_idx].id);
    }
    for (const auto &kv : surgeon_ot_by_day) {
        for (int d = 0; d < ctx.days; ++d) {
            int sz = (int)kv.second[d].size();
            if (sz > 1) raw_cost += (sz - 1);
        }
    }
    return raw_cost * getWeight(in, "surgeon_transfer", "S6", 1);
}

int IHTC_Output::ComputeCostPatientDelay() const {
    if (!bound_input) return 0;
    const IHTC_Input &in = *bound_input;
    SoftCostContext ctx = buildSoftCostContext(in, *this);
    return ctx.total_delay * getWeight(in, "patient_delay", "S7", 5);
}

int IHTC_Output::ComputeCostUnscheduledOptional() const {
    if (!bound_input) return 0;
    const IHTC_Input &in = *bound_input;
    SoftCostContext ctx = buildSoftCostContext(in, *this);
    return ctx.unscheduled_optional_count * getWeight(in, "unscheduled_optional", "S8", 100);
}

int IHTC_Output::ComputeCostTotal() const {
    return ComputeCostRoomMixedAge()
        + ComputeCostRoomNurseSkill()
        + ComputeCostContinuityOfCare()
        + ComputeCostNurseExcessiveWorkload()
        + ComputeCostOpenOperatingTheater()
        + ComputeCostSurgeonTransfer()
        + ComputeCostPatientDelay()
        + ComputeCostUnscheduledOptional();
}

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

bool IHTC_Input::loadInstance(const std::string &path) {
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
    occupants.clear();
    ots.clear();
    weights.clear();
    D = 0;

    // Horizon / meta
    if (j.contains("D") && j["D"].is_number_integer()) D = j["D"].get<int>();
    else if (j.contains("days") && j["days"].is_number_integer()) D = j["days"].get<int>();
    else if (j.contains("horizon") && j["horizon"].is_number_integer()) D = j["horizon"].get<int>();
    else if (j.contains("HORIZON") && j["HORIZON"].is_object()) {
        const auto &h = j["HORIZON"];
        if (h.contains("D") && h["D"].is_number_integer()) D = h["D"].get<int>();
        else if (h.contains("days") && h["days"].is_number_integer()) D = h["days"].get<int>();
    }

    if (j.contains("shifts_per_day")) shifts_per_day = try_get_int(j, {"shifts_per_day","shiftsPerDay","S"}, shifts_per_day);
    else if (j.contains("shift_types") && j["shift_types"].is_array()) shifts_per_day = (int)j["shift_types"].size();

    // Operating theatres (OT)
    if (j.contains("ots") && j["ots"].is_array()) {
        for (const auto &jo : j["ots"]) {
            OT o;
            o.id = try_get_string(jo, {"id","ot_id","otId","name"}, "");
            o.daily_capacity = try_get_int(jo, {"daily_capacity","capacity","cap","dailyCap"}, 0);
            if (D > 0) o.daily_capacity_by_day.assign(D, o.daily_capacity);
            if (jo.contains("unavailable_days") && jo["unavailable_days"].is_array()) {
                for (auto &d : jo["unavailable_days"]) {
                    if (d.is_number_integer()) {
                        int day = d.get<int>();
                        o.unavailable_days.push_back(day);
                        if (day >= 0 && day < (int)o.daily_capacity_by_day.size()) o.daily_capacity_by_day[day] = 0;
                    }
                }
            }
            ots.push_back(std::move(o));
        }
    }
    // Alternative key used in test instances
    else if (j.contains("operating_theaters") && j["operating_theaters"].is_array()) {
        for (const auto &jo : j["operating_theaters"]) {
            OT o;
            o.id = try_get_string(jo, {"id","ot_id","otId","name"}, "");
            if (jo.contains("availability") && jo["availability"].is_array()) {
                int max_cap = 0;
                o.daily_capacity_by_day.assign(jo["availability"].size(), 0);
                for (size_t d = 0; d < jo["availability"].size(); ++d) {
                    int cap = jo["availability"][d].is_number_integer() ? jo["availability"][d].get<int>() : 0;
                    o.daily_capacity_by_day[d] = cap;
                    if (cap > max_cap) max_cap = cap;
                    if (cap <= 0) o.unavailable_days.push_back((int)d);
                }
                if (D == 0) D = (int)o.daily_capacity_by_day.size();
                o.daily_capacity = max_cap;
            } else {
                o.daily_capacity = try_get_int(jo, {"daily_capacity","capacity","cap","dailyCap"}, 0);
                if (D > 0) o.daily_capacity_by_day.assign(D, o.daily_capacity);
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
            n.level = try_get_int(jn, {"level","competence","skillLevel","skill_level"}, 0);
            n.max_load = try_get_int(jn, {"max_load","maxLoad","max"}, 0);
            if (jn.contains("roster") && jn["roster"].is_array()) {
                for (auto &r : jn["roster"]) n.roster.push_back(r.is_number() ? r.get<int>() : 0);
            }
            if (n.max_load == 0) n.max_load = 9999;
            nurses.push_back(std::move(n));
        }
    }

    // Surgeons
    if (j.contains("surgeons") && j["surgeons"].is_array()) {
        for (const auto &js : j["surgeons"]) {
            Surgeon s;
            s.id = try_get_string(js, {"id","surgeon_id","surgeonId","name"}, "");
            s.max_daily_time = try_get_int(js, {"max_daily_time","max","daily_time"}, 0);
            if (s.max_daily_time == 0 && js.contains("max_surgery_time") && js["max_surgery_time"].is_array()) {
                int mx = 0;
                s.daily_max_time.assign(js["max_surgery_time"].size(), 0);
                for (size_t d = 0; d < js["max_surgery_time"].size(); ++d) {
                    const auto &v = js["max_surgery_time"][d];
                    if (v.is_number_integer()) {
                        int val = v.get<int>();
                        s.daily_max_time[d] = val;
                        mx = std::max(mx, val);
                    }
                }
                s.max_daily_time = mx;
            }
            if (s.daily_max_time.empty() && D > 0) s.daily_max_time.assign(D, s.max_daily_time);
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
            p.release_date = try_get_int(jp, {"releaseDate","release_date","release","earliest","surgery_release_day"}, 0);
            p.due_date = try_get_int(jp, {"dueDate","due_date","due","latest","surgery_due_day"}, D > 0 ? (D - 1) : 0);
            p.length_of_stay = try_get_int(jp, {"lengthOfStay","length_of_stay","los","stay"}, 1);
            p.age_group = try_get_int(jp, {"age_group","ageGroup","age"}, -1);
            p.sex = try_get_string(jp, {"sex","gender"}, "");
            p.surgery_time = try_get_int(jp, {"surgery_time","surgeryTime","surgery","operating_time","operatingTime","surgery_duration"}, 0);
            p.surgeon_id = try_get_string(jp, {"surgeon","surgeon_id","surgeonId","assigned_surgeon"}, "");
            p.min_nurse_level = try_get_int(jp, {"min_nurse_level","minLevel","required_nurse_level"}, 0);

            // nurse load per shift/turn may be named variously
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
                for (auto &x : jp["incompatible_rooms"]) if (x.is_string()) p.incompatible_rooms.push_back(x.get<std::string>());
            } else if (jp.contains("incompatible") && jp["incompatible"].is_array()) {
                for (auto &x : jp["incompatible"]) if (x.is_string()) p.incompatible_rooms.push_back(x.get<std::string>());
            } else if (jp.contains("incompatible_room_ids") && jp["incompatible_room_ids"].is_array()) {
                for (auto &x : jp["incompatible_room_ids"]) if (x.is_string()) p.incompatible_rooms.push_back(x.get<std::string>());
            }

            if (p.min_nurse_level == 0 && jp.contains("skill_level_required") && jp["skill_level_required"].is_array()) {
                int req = 0;
                for (const auto &v : jp["skill_level_required"]) if (v.is_number_integer()) req = std::max(req, v.get<int>());
                p.min_nurse_level = req;
            }

            patients.push_back(std::move(p));
        }
    }

    // Occupants already in hospital at start horizon
    if (j.contains("occupants") && j["occupants"].is_array()) {
        for (const auto &jo : j["occupants"]) {
            Occupant o;
            o.id = try_get_string(jo, {"id","occupant_id","name"}, "");
            o.room_id = try_get_string(jo, {"room_id","roomId","room"}, "");
            o.sex = try_get_string(jo, {"sex","gender"}, "");
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
            occupants.push_back(std::move(o));
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
