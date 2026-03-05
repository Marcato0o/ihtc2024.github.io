#include "IHTC_Data.hh"
#include "json/io.hh"
#include <algorithm>
#include <iostream>
#include <set>
#include <unordered_map>

using namespace std;

IHTC_Input::IHTC_Input() = default;

IHTC_Input::IHTC_Input(const std::string &file_name) {
    loadInstance(file_name);
}

const std::string &IHTC_Input::getRawJsonText() const {
    return raw_json_text;
}

IHTC_Output::IHTC_Output(const IHTC_Input &in) {
    bound_input = &in;
    int days = in.D > 0 ? in.D : 1;
    init(in.patients.size(), in.rooms.size(), in.ots.size(), days);
}

void IHTC_Output::init(size_t num_patients, size_t num_rooms, size_t num_ots, int days) {
    admitted.assign(num_patients, false);
    admit_day.assign(num_patients, -1);
    room_assigned_idx.assign(num_patients, -1);
    ot_assigned_idx.assign(num_patients, -1);
    nurse_assignments.clear();
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

void IHTC_Output::seedOccupantStay(int room_idx, int admission_day, int length_of_stay, const std::string &sex) {
    if (room_idx < 0 || room_idx >= (int)room_occupancy.size()) return;
    if (room_occupancy.empty() || room_occupancy[room_idx].empty()) return;
    int days = (int)room_occupancy[room_idx].size();
    int los = std::max(1, length_of_stay);
    int start = std::max(0, admission_day);
    for (int dd = 0; dd < los; ++dd) {
        int d = start + dd;
        if (d < 0 || d >= days) continue;
        room_occupancy[room_idx][d] += 1;
        if (!sex.empty() && room_gender[room_idx][d].empty()) room_gender[room_idx][d] = sex;
    }
}

void IHTC_Output::clearNurseAssignments() {
    nurse_assignments.clear();
}

void IHTC_Output::addNurseAssignment(int nurse_idx, int day, int shift, int room_idx) {
    nurse_assignments.push_back({nurse_idx, day, shift, room_idx});
}

bool IHTC_Output::isAdmitted(int patient_id) const {
    return patient_id >= 0 && patient_id < (int)admitted.size() && admitted[patient_id];
}

int IHTC_Output::getAdmitDay(int patient_id) const {
    if (patient_id < 0 || patient_id >= (int)admit_day.size()) return -1;
    return admit_day[patient_id];
}

int IHTC_Output::getRoomAssignedIdx(int patient_id) const {
    if (patient_id < 0 || patient_id >= (int)room_assigned_idx.size()) return -1;
    return room_assigned_idx[patient_id];
}

int IHTC_Output::getOtAssignedIdx(int patient_id) const {
    if (patient_id < 0 || patient_id >= (int)ot_assigned_idx.size()) return -1;
    return ot_assigned_idx[patient_id];
}

std::vector<std::tuple<int, int, int, int>> IHTC_Output::getNurseAssignmentTuples() const {
    std::vector<std::tuple<int, int, int, int>> tuples;
    tuples.reserve(nurse_assignments.size());
    for (const auto &na : nurse_assignments) {
        tuples.emplace_back(na.nurse_idx, na.day, na.shift, na.room_idx);
    }
    return tuples;
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
        bool is_admitted = out.isAdmitted((int)pid);
        if (!is_admitted) {
            if (!in.patients[pid].mandatory) ctx.unscheduled_optional_count++;
            continue;
        }

        int admit_day = out.getAdmitDay((int)pid);
        const auto &p = in.patients[pid];
        int los = std::max(1, p.length_of_stay);
        int room_idx = out.getRoomAssignedIdx((int)pid);
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

        int ot_idx = out.getOtAssignedIdx((int)pid);
        if (admit_day >= 0 && admit_day < ctx.days && ot_idx >= 0 && ot_idx < (int)in.ots.size()) {
            ctx.ot_by_day[admit_day].insert(in.ots[ot_idx].id);
        }
    }

    int nurse_count = (int)in.nurses.size();
    ctx.nurse_level.assign(nurse_count, 0);
    ctx.nurse_max_load.assign(nurse_count, 9999);
    for (int i = 0; i < nurse_count; ++i) {
        ctx.nurse_level[i] = in.nurses[i].level;
        ctx.nurse_max_load[i] = in.nurses[i].max_load;
    }

    ctx.nurse_load_by_shift.assign(nurse_count, std::vector<int>(ctx.days * ctx.shifts, 0));
    ctx.room_shift_nurses.assign(ctx.days, std::vector<std::vector<std::vector<int>>>(ctx.shifts, std::vector<std::vector<int>>(in.rooms.size())));

    std::vector<std::tuple<int, int, int, int>> nurse_assignments = out.getNurseAssignmentTuples();
    for (const auto &na : nurse_assignments) {
        int n = std::get<0>(na);
        int d = std::get<1>(na);
        int sh = std::get<2>(na);
        int r = std::get<3>(na);
        if (n < 0 || n >= nurse_count) continue;
        if (d < 0 || d >= ctx.days) continue;
        if (sh < 0 || sh >= ctx.shifts) continue;
        if (r < 0 || r >= (int)in.rooms.size()) continue;
        ctx.room_shift_nurses[d][sh][r].push_back(n);
        ctx.nurse_load_by_shift[n][d * ctx.shifts + sh] += ctx.room_shift_load[d][sh][r];
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

void IHTC_Output::printCosts() const {
    int age_mix_weighted = ComputeCostRoomMixedAge();
    int skill_weighted = ComputeCostRoomNurseSkill();
    int continuity_weighted = ComputeCostContinuityOfCare();
    int excess_weighted = ComputeCostNurseExcessiveWorkload();
    int open_ot_cost = ComputeCostOpenOperatingTheater();
    int surgeon_transfer_weighted = ComputeCostSurgeonTransfer();
    int delay_cost = ComputeCostPatientDelay();
    int unscheduled_cost = ComputeCostUnscheduledOptional();
    int total_cost = ComputeCostTotal();

    std::cout << "Cost: " << total_cost
              << ", Unscheduled: " << unscheduled_cost
              << ",  Delay: " << delay_cost
              << ",  OpenOT: " << open_ot_cost
              << ",  AgeMix: " << age_mix_weighted
              << ",  Skill: " << skill_weighted
              << ",  Excess: " << excess_weighted
              << ",  Continuity: " << continuity_weighted
              << ",  SurgeonTransfer: " << surgeon_transfer_weighted
              << std::endl;
}

void IHTC_Output::writeJSON(const std::string& filename) const {
    if (!bound_input) {
        std::cerr << "writeJSON: input is not bound to output state." << std::endl;
        return;
    }
    jsonio::write_solution(*bound_input, *this, filename);
}

bool IHTC_Input::loadInstance(const std::string &path) {
    return jsonio::load_instance(*this, path);
}
