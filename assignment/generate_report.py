#!/usr/bin/env python3
"""
generate_report.py
Runs the IHTC greedy solver on all 40 instances, validates each solution,
and produces REPORT.md at the repository root.
"""

import json
import re
import subprocess
import sys
import time
from pathlib import Path

# ---------------------------------------------------------------------------
# Paths (WSL style — this script runs inside WSL)
# ---------------------------------------------------------------------------
REPO_ROOT = Path(__file__).parent.parent.resolve()
CODE_DIR  = REPO_ROOT / 'assignment'
ASSETS    = REPO_ROOT / 'assets/files'

SOLVER    = CODE_DIR / 'IHTC_Test.exe'
VALIDATOR = ASSETS / 'IHTP_Validator'
SOL_FILE  = CODE_DIR / 'solution.json'

TEST_INST_DIR  = ASSETS / 'test'
TEST_SOL_DIR   = ASSETS / 'test_sol'
COMP_INST_DIR  = ASSETS / 'instances'
COMP_SOL_DIR   = ASSETS / 'solutions'

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def parse_instance_meta(path: Path) -> dict:
    with open(path) as f:
        d = json.load(f)
    patients   = d.get('patients', [])
    n_mand     = sum(1 for p in patients if p.get('mandatory', False))
    ots_key    = 'operating_theaters' if 'operating_theaters' in d else 'operatingTheaters'
    return {
        'days'      : d.get('days', '?'),
        'rooms'     : len(d.get('rooms', [])),
        'nurses'    : len(d.get('nurses', [])),
        'ots'       : len(d.get(ots_key, [])),
        'surgeons'  : len(d.get('surgeons', [])),
        'n_patients': len(patients),
        'n_mandatory': n_mand,
        'n_optional' : len(patients) - n_mand,
    }


def run_solver(instance_path: Path) -> tuple:
    """Returns (stdout+stderr string, elapsed_ms, admitted_count, mandatory_failed)."""
    t0 = time.perf_counter()
    r  = subprocess.run(
        [str(SOLVER), str(instance_path)],
        capture_output=True, text=True, cwd=str(CODE_DIR)
    )
    elapsed_ms = round((time.perf_counter() - t0) * 1000)
    out = r.stdout + r.stderr

    admitted = 0
    mand_failed = 0
    m = re.search(r'Patients admitted:\s*(\d+)/', out)
    if m:
        admitted = int(m.group(1))
    m2 = re.search(r'\[H5 VIOLATION\] Mandatory patients not admitted:\s*(\d+)', out)
    if m2:
        mand_failed = int(m2.group(1))

    return out, elapsed_ms, admitted, mand_failed


def parse_validator_output(text: str) -> dict:
    """Parse validator stdout into a structured dict."""
    result = {
        'hard_total': 0,
        'hard_detail': {},
        's1': (0, 0, 0), 's2': (0, 0, 0), 's3': (0, 0, 0),
        's4': (0, 0, 0), 's5': (0, 0, 0), 's6': (0, 0, 0),
        's7': (0, 0, 0), 's8': (0, 0, 0),
        'total': 0,
    }

    hard_keys = [
        'RoomGenderMix', 'PatientRoomCompatibility', 'SurgeonOvertime',
        'OperatingTheaterOvertime', 'MandatoryUnscheduledPatients',
        'AdmissionDay', 'RoomCapacity', 'NursePresence', 'UncoveredRoom',
    ]
    for key in hard_keys:
        m = re.search(rf'{key}\.+(\d+)', text)
        result['hard_detail'][key] = int(m.group(1)) if m else 0

    m = re.search(r'Total violations\s*=\s*(\d+)', text)
    if m:
        result['hard_total'] = int(m.group(1))

    # Cost lines: name.....weighted (weight X raw)
    cost_map = {
        'RoomAgeMix'                : 's1',
        'RoomSkillLevel'            : 's2',
        'ContinuityOfCare'          : 's3',
        'ExcessiveNurseWorkload'    : 's4',
        'OpenOperatingTheater'      : 's5',
        'SurgeonTransfer'           : 's6',
        'PatientDelay'              : 's7',
        'ElectiveUnscheduledPatients': 's8',
    }
    for name, key in cost_map.items():
        m = re.search(rf'{name}\.+\s*(\d+)\s*\(\s*(\d+)\s*X\s*(\d+)\)', text)
        if m:
            result[key] = (int(m.group(1)), int(m.group(2)), int(m.group(3)))

    m = re.search(r'Total cost\s*=\s*(\d+)', text)
    if m:
        result['total'] = int(m.group(1))

    return result


def is_valid_json(path: Path) -> bool:
    try:
        with open(path) as f:
            json.load(f)
        return True
    except Exception:
        return False


def run_validator(instance_path: Path, solution_path: Path) -> dict:
    r = subprocess.run(
        [str(VALIDATOR), str(instance_path), str(solution_path)],
        capture_output=True, text=True, cwd=str(CODE_DIR)
    )
    return parse_validator_output(r.stdout + r.stderr)


EMPTY_COSTS = dict(
    hard_total=None, hard_detail={},
    s1=(None,None,None), s2=(None,None,None), s3=(None,None,None),
    s4=(None,None,None), s5=(None,None,None), s6=(None,None,None),
    s7=(None,None,None), s8=(None,None,None),
    total=None,
)


# ---------------------------------------------------------------------------
# Per-instance extra content (appended after the cost table)
# ---------------------------------------------------------------------------

INSTANCE_EXTRA = {
    'i16': """\

#### Why does i16 fail H5? — Root Cause Analysis

i16 is the **only instance in the entire benchmark where mandatory patients cannot all be admitted**. This section explains why the greedy fails here and what makes each of the 6 patients infeasible.

##### Instance Capacity Context

| Metric | Value |
|:---|---:|
| Planning horizon | 14 days |
| Rooms / total beds | 17 rooms / 45 beds |
| Total bed-days available | 630 |
| Mandatory patients | 125 |
| Mandatory bed-days needed | 567 |
| **Global utilisation** | **90.0%** |

The instance is extremely tight. At 90% utilisation, the greedy has virtually no slack — any sequence of placement decisions that isn't near-optimal for global packing will leave some mandatory patients with no room.

##### Gender A Bottleneck

The hard constraint H1 (no gender mixing per room per day) splits capacity by gender. Occupants already lock 9 rooms to specific genders from day 0:

| Gender | Rooms locked to it at day 0 | Beds potentially available | Mandatory bed-days needed | Utilisation |
|:---:|:---:|:---:|:---:|:---:|
| A | r00, r04, r07, r09, r11 | 336 | 291 | **86.6%** |
| B | r01, r02, r05, r06, r12, r13, r15, r16 | 434 | 276 | 63.6% |

Gender A capacity is at **86.6%** — well above any safe margin for a greedy algorithm without backtracking. Gender B has comfortable headroom (63.6%). Accordingly, 5 of the 6 unscheduled mandatory patients are gender A.

##### Why the Greedy Cannot Recover

The greedy's dynamic MRV heuristic (always scheduling the most constrained patient first) mitigates but cannot eliminate the problem at this utilisation level. Every placement is irrevocable. When earlier patients occupy gender-A rooms, the room is gender-locked for their entire stay. A later patient — even one with a wide admission window — finds all compatible rooms either full or gender-locked, with no way to undo past decisions.

##### The 6 Unscheduled Mandatory Patients

---

**p019** — Gender A · Adult · 5-day stay · Window days 0–9 (effective 10 days) · 90 min surgery · Surgeon s2 · 1 incompatible room (r05)

At the end of the greedy pass, every room in the hospital is blocked on the critical days of p019's window:
- 15 rooms are **full** (bed capacity exhausted) on day 9
- 1 room (r06) has a **gender B conflict** on day 9
- 1 room (r05) is **incompatible** for this patient

p019 had a wide admission window (10 effective days) and only 1 incompatible room — it should have been easy to place. The failure reveals that the overall gender-A capacity was exhausted by earlier placements, not that p019 was individually constrained.

---

**p027** — Gender B · Elderly · 5-day stay · Window days 2–7 (6 days) · **240 min surgery** · Surgeon s0 · 3 incompatible rooms (r00, r03, r08)

p027 has the **largest surgery duration** of the 6 (240 min) and **3 incompatible rooms** — the most exclusions. Its admission window is only 6 days. By day 7, every gender-B-compatible room is full, and gender-A rooms are locked. With 3 rooms excluded and a tight window, the feasibility space was always narrow. The large surgery time (240 min) also limits which OT + surgeon day combinations can accept it, making the (day, room, OT) search space even smaller. This is the most inherently constrained patient of the six.

---

**p041** — Gender A · Elderly · 3-day stay · Window days 1–11 (effective 11 days) · 90 min surgery · Surgeon s3 · **3 incompatible rooms** (r10, r11, r13)

p041 looks easy: only 3 days to accommodate and an 11-day window. But 3 incompatible rooms (r10, r11, r13) together remove 8 beds (3+3+2) from the gender-A pool. Combined with the 86.6% gender-A utilisation, by day 11 every remaining A-compatible non-excluded room is at capacity. The wide window paradoxically hurts: because p041 appeared less urgent to the MRV heuristic than patients with tighter windows, it was deferred — and by the time it is selected, all viable slots are gone.

---

**p042** — Gender A · Adult · **6-day stay** · Window days 4–8 (effective 5 days, due to D−los = 14−6 = 8) · 120 min surgery · Surgeon s1 · 2 incompatible rooms (r00, r06)

The 6-day stay restricts the effective admission window to [4, 8] — only 5 days. Every room is full on the blocking day within this window (day 8 for most; day 9 for r04). r07 has a gender-B conflict from day 8 onward. The 2 incompatible rooms further narrow options. The long stay combined with mid-to-late release date makes this patient very sensitive to capacity consumed by earlier placements.

---

**p055** — Gender A · Elderly · **9-day stay** · Window days 3–5 (effective **3 days** only, due to D−los = 14−9 = 5) · 120 min surgery · Surgeon s1 · 1 incompatible room (r03)

p055 has the **tightest effective window** of all: only 3 possible start days (3, 4, 5). A 9-day stay with due_day=9 means the patient must start no later than day 5 to finish within the 14-day horizon. Despite having only 1 incompatible room, the combination of an extremely long stay and a tiny admission window makes this patient one of the hardest to place globally. The gender-A rooms are already crowded during days 3–13, and the greedy placed other patients into those slots first.

---

**p082** — Gender A · Elderly · 4-day stay · Window days 3–8 (6 days) · 120 min surgery · Surgeon s3 · 1 incompatible room (r15)

p082 appears straightforward: 4-day stay, 6-day window, only 1 incompatible room. However, in the final state all rooms are blocked on their critical days:
- 11 rooms: **full** (capacity exhausted) on day 8
- 4 rooms (r00, r06, r07, r13): **gender B conflict**
- 1 room (r15): **incompatible**

The blocking day for most rooms is day 8, meaning the greedy saturated gender-A capacity in the day 3–8 range — exactly this patient's window. This is the clearest illustration of the cascade effect: p082 is not individually unusual, but it arrives last in the greedy queue when all slots are taken.

---

##### Summary Table

| Patient | Gender | LoS | Eff. window | Rooms excluded | Primary blocker |
|:---:|:---:|:---:|:---:|:---:|:---|
| p019 | A | 5 | 10 days | 1 incompatible | All A-rooms full by day 9 |
| p027 | B | 5 | 6 days | 3 incompatible | Largest surgery (240 min) + 3 exclusions + tight window |
| p041 | A | 3 | 11 days | 3 incompatible | Wide window → deprioritised by MRV; A-rooms saturated by day 11 |
| p042 | A | 6 | 5 days | 2 incompatible | Long stay + mid-release; all rooms full by day 8 |
| p055 | A | 9 | **3 days** | 1 incompatible | Tightest window of all; 9-day stay must start ≤ day 5 |
| p082 | A | 4 | 6 days | 1 incompatible | No individual anomaly; last patient in saturated A-pool |

**Conclusion**: i16's failure is a structural consequence of operating at 90% mandatory utilisation with hard gender-separation constraints. At this density, a single-pass greedy without backtracking cannot guarantee a feasible schedule — it would require either global search or a backtracking repair phase to resolve the 6 displaced patients.
""",
}


# ---------------------------------------------------------------------------
# Report formatting helpers
# ---------------------------------------------------------------------------

COST_NAMES = {
    's1': 'S1 · Room Age Mix',
    's2': 'S2 · Room Skill Level',
    's3': 'S3 · Continuity of Care',
    's4': 'S4 · Excess Workload',
    's5': 'S5 · Open Operating Theater',
    's6': 'S6 · Surgeon Transfer',
    's7': 'S7 · Patient Delay',
    's8': 'S8 · Unscheduled Patients',
}

HARD_LABELS = {
    'RoomGenderMix'              : 'H1 · Room Gender Mix',
    'PatientRoomCompatibility'   : 'H2 · Patient Room Compatibility',
    'SurgeonOvertime'            : 'H3 · Surgeon Overtime',
    'OperatingTheaterOvertime'   : 'H4 · Operating Theater Overtime',
    'MandatoryUnscheduledPatients': 'H5 · Mandatory Unscheduled Patients',
    'AdmissionDay'               : 'H6 · Admission Day',
    'RoomCapacity'               : 'H7 · Room Capacity',
    'NursePresence'              : 'H8 · Nurse Presence',
    'UncoveredRoom'              : 'H9 · Uncovered Room',
}


def delta_str(our: int, ref) -> str:
    if ref is None:
        return '—'
    d = our - ref
    if d == 0:
        return '**= 0** ═'
    elif d > 0:
        return f'+{d:,} ⬆'
    else:
        return f'{d:,} ⬇'


def ratio_str(our: int, ref) -> str:
    if ref is None or ref == 0:
        return '—'
    r = our / ref
    return f'×{r:.2f}'


def hard_status(v: dict) -> str:
    if v['hard_total'] == 0:
        return '✅ All satisfied'
    parts = [f"{HARD_LABELS.get(k, k)}: **{c}**"
             for k, c in v['hard_detail'].items() if c > 0]
    return '❌ Violations: ' + ', '.join(parts)


def instance_section(name: str, meta: dict, elapsed_ms: int,
                     admitted: int, mand_failed: int,
                     ours: dict, ref: dict) -> str:
    lines = []

    # Header
    lines.append(f'---\n')
    lines.append(f'### {name}\n')

    # Metadata grid
    lines.append(
        f'> **Days** {meta["days"]} &nbsp;|&nbsp; '
        f'**Rooms** {meta["rooms"]} &nbsp;|&nbsp; '
        f'**Nurses** {meta["nurses"]} &nbsp;|&nbsp; '
        f'**OTs** {meta["ots"]} &nbsp;|&nbsp; '
        f'**Surgeons** {meta["surgeons"]}\n'
    )

    # Patient and admission summary
    mand_ok   = meta['n_mandatory'] - mand_failed
    opt_admitted = admitted - mand_ok
    opt_unsched  = meta['n_optional'] - opt_admitted
    mand_icon = '✅' if mand_failed == 0 else '❌'

    lines.append(f'| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |')
    lines.append(f'|:---:|:---:|:---:|:---:|:---:|:---:|')
    lines.append(
        f'| {meta["n_patients"]} '
        f'| {meta["n_mandatory"]} '
        f'| {meta["n_optional"]} '
        f'| {admitted} '
        f'| {mand_failed} {mand_icon} '
        f'| {elapsed_ms} ms |\n'
    )

    # Hard constraints
    lines.append(f'**Hard constraints**: {hard_status(ours)}\n')

    # Cost breakdown table
    lines.append('| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |')
    lines.append('|:---|:---:|---:|---:|---:|---:|---:|:---:|')

    ref_avail = ref['total'] is not None
    for key in ['s1','s2','s3','s4','s5','s6','s7','s8']:
        label = COST_NAMES[key]
        o_w, o_wt, o_r = ours[key]
        if ref_avail:
            r_w, _, r_r = ref[key]
            r_w_str = f'{r_w:,}'
            r_r_str = str(r_r)
        else:
            r_w = None
            r_w_str = '—'
            r_r_str = '—'
        d_str = delta_str(o_w, r_w)
        rat   = ratio_str(o_w, r_w)
        lines.append(f'| {label} | {o_wt} | {o_w:,} | {o_r} | {r_w_str} | {r_r_str} | {d_str} | {rat} |')

    # Total row
    o_tot = ours['total']
    r_tot = ref['total']
    r_tot_str = f'**{r_tot:,}**' if r_tot is not None else '—'
    lines.append(
        f'| **TOTAL** | | **{o_tot:,}** | | {r_tot_str} | | '
        f'**{delta_str(o_tot, r_tot)}** | **{ratio_str(o_tot, r_tot)}** |'
    )
    lines.append('')

    return '\n'.join(lines)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    # Build the list of (name, instance_path, ref_sol_path) in order
    instances = []
    for i in range(1, 11):
        name = f'test{i:02d}'
        inst = TEST_INST_DIR / f'{name}.json'
        ref  = TEST_SOL_DIR  / f'sol_{name}.json'
        instances.append((name, inst, ref, 'test'))

    for i in range(1, 31):
        name = f'i{i:02d}'
        inst = COMP_INST_DIR / f'{name}.json'
        ref  = COMP_SOL_DIR  / f'sol_{name}.json'
        instances.append((name, inst, ref, 'comp'))

    # Collect data
    rows    = []  # for summary table
    sections = {'test': [], 'comp': []}

    print(f'Running {len(instances)} instances...', flush=True)
    for name, inst_path, ref_path, kind in instances:
        print(f'  {name}...', end=' ', flush=True)

        meta = parse_instance_meta(inst_path)
        solver_out, elapsed_ms, admitted, mand_failed = run_solver(inst_path)
        ours = run_validator(inst_path, SOL_FILE)
        if is_valid_json(ref_path):
            ref = run_validator(inst_path, ref_path)
        else:
            ref = dict(EMPTY_COSTS)

        ref_label = f'{ref["total"]:,}' if ref['total'] is not None else 'N/A'
        print(f'done ({elapsed_ms} ms, cost={ours["total"]:,} vs ref={ref_label})')

        rows.append({
            'name'       : name,
            'kind'       : kind,
            'meta'       : meta,
            'elapsed_ms' : elapsed_ms,
            'admitted'   : admitted,
            'mand_failed': mand_failed,
            'ours'       : ours,
            'ref'        : ref,
        })
        sec = instance_section(name, meta, elapsed_ms, admitted, mand_failed, ours, ref)
        if name in INSTANCE_EXTRA:
            sec = sec.rstrip('\n') + INSTANCE_EXTRA[name]
        sections[kind].append(sec)

    # ------------------------------------------------------------------
    # Build REPORT.md
    # ------------------------------------------------------------------
    lines = []

    # ── Title
    lines.append('# IHTC 2024 — Greedy Solver · Performance Report\n')
    lines.append(
        '*Generated: 2026-03-20 &nbsp;·&nbsp; '
        'Solver: Single-Pass Greedy (MRV + minimum marginal cost) &nbsp;·&nbsp; '
        'Instances: 40 (10 test + 30 competition)*\n'
    )
    lines.append('---\n')

    # ── Overview
    lines.append('## Overview\n')
    lines.append(
        'This report benchmarks the greedy solver against the official IHTC 2024 '
        'reference solutions. The solver runs in a **single pass**: patients are '
        'scheduled greedily (most-constrained first) to a `(day, room, OT)` triple '
        'minimising marginal soft costs; nurses are then assigned greedily to every '
        'occupied `(day, shift, room)` slot. No backtracking is performed. '
        'Each section shows the full cost breakdown (8 soft constraints S1–S8) '
        'with the weighted cost used in scoring, the raw violation count, '
        'and the delta versus the reference solution.\n'
    )
    lines.append(
        '**Legend**: ⬆ worse than reference · ⬇ better than reference · ═ equal · '
        '✅ feasible · ❌ hard constraint violation\n'
    )
    lines.append('---\n')

    # ── Global Summary Table
    lines.append('## Global Summary\n')
    lines.append('| Instance | D | Rooms | Patients (M+O) | Admitted | Hard | Our Total | Ref Total | Δ | Ratio | Time |')
    lines.append('|:---|:---:|:---:|:---:|:---:|:---:|---:|---:|---:|:---:|---:|')

    for r in rows:
        m    = r['meta']
        o    = r['ours']
        rf   = r['ref']
        icon    = '✅' if o['hard_total'] == 0 else f'❌ {o["hard_total"]}'
        rf_tot  = rf["total"]
        rf_str  = f'{rf_tot:,}' if rf_tot is not None else '—'
        lines.append(
            f'| {r["name"]} '
            f'| {m["days"]} '
            f'| {m["rooms"]} '
            f'| {m["n_patients"]} ({m["n_mandatory"]}+{m["n_optional"]}) '
            f'| {r["admitted"]} '
            f'| {icon} '
            f'| {o["total"]:,} '
            f'| {rf_str} '
            f'| {delta_str(o["total"], rf_tot)} '
            f'| {ratio_str(o["total"], rf_tot)} '
            f'| {r["elapsed_ms"]} ms |'
        )

    lines.append('')

    # ── Analysis
    lines.append('---\n')
    lines.append('## Analysis\n')

    # Cost component averages (all 40 instances now have references)
    total_ours = sum(r['ours']['total'] for r in rows)
    total_ref  = sum(r['ref']['total'] for r in rows)
    feasible   = sum(1 for r in rows if r['ours']['hard_total'] == 0)

    lines.append(f'### Key Metrics\n')
    lines.append(f'| Metric | Value |')
    lines.append(f'|:---|---:|')
    lines.append(f'| Instances with zero hard violations | {feasible} / {len(rows)} |')
    lines.append(f'| Sum of our costs (40 instances) | {total_ours:,} |')
    lines.append(f'| Sum of reference costs (40 instances) | {total_ref:,} |')
    lines.append(f'| Overall cost ratio | {ratio_str(total_ours, total_ref)} |')

    # Per-component analysis
    lines.append('\n### Average Cost Ratio per Soft Constraint\n')
    lines.append('| Constraint | Avg Ours | Avg Ref | Avg Δ | Avg Ratio |')
    lines.append('|:---|---:|---:|---:|:---:|')
    for key in ['s1','s2','s3','s4','s5','s6','s7','s8']:
        avg_ours = sum(r['ours'][key][0] for r in rows) / len(rows)
        avg_ref  = sum(r['ref'][key][0]  for r in rows) / len(rows)
        avg_d    = avg_ours - avg_ref
        sign     = '⬆' if avg_d > 0 else ('⬇' if avg_d < 0 else '═')
        rat      = ratio_str(int(avg_ours), int(avg_ref)) if avg_ref > 0 else '—'
        lines.append(
            f'| {COST_NAMES[key]} '
            f'| {avg_ours:,.0f} '
            f'| {avg_ref:,.0f} '
            f'| {avg_d:+,.0f} {sign} '
            f'| {rat} |'
        )

    # Best / worst instances by ratio
    ratios = [(r['name'], r['ours']['total'] / r['ref']['total']) for r in rows if r['ref']['total'] > 0]
    ratios_sorted = sorted(ratios, key=lambda x: x[1])
    best3  = ratios_sorted[:3]
    worst3 = ratios_sorted[-3:][::-1]

    lines.append('\n### Best Performing Instances (closest to reference)\n')
    lines.append('| Instance | Our Total | Ref Total | Ratio |')
    lines.append('|:---|---:|---:|:---:|')
    for name, rat in best3:
        row = next(r for r in rows if r['name'] == name)
        lines.append(f'| {name} | {row["ours"]["total"]:,} | {row["ref"]["total"]:,} | ×{rat:.2f} |')

    lines.append('\n### Furthest from Reference\n')
    lines.append('| Instance | Our Total | Ref Total | Ratio |')
    lines.append('|:---|---:|---:|:---:|')
    for name, rat in worst3:
        row = next(r for r in rows if r['name'] == name)
        lines.append(f'| {name} | {row["ours"]["total"]:,} | {row["ref"]["total"]:,} | ×{rat:.2f} |')

    lines.append('\n---\n')

    # ── Test Instances detail
    lines.append('## Test Instances\n')
    for s in sections['test']:
        lines.append(s)

    lines.append('\n---\n')

    # ── Competition Instances detail
    lines.append('## Competition Instances\n')
    for s in sections['comp']:
        lines.append(s)

    # Write file
    out_path = REPO_ROOT / 'REPORT.md'
    out_path.write_text('\n'.join(lines), encoding='utf-8')
    print(f'\nReport written to {out_path}')


if __name__ == '__main__':
    main()
