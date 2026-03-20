# Final Assignment - **Integrated Healthcare Timetabling Problem (IHTP)**

## What is the problem?

The **Integrated Healthcare Timetabling Problem (IHTP)** is a combinatorial optimization problem that integrates three hospital subproblems:

1. **PAS** – Patient Admission Scheduling (when and where to admit patients)
2. **NRA** – Nurse-to-Room Assignment (which nurse is assigned to which room for each shift)
3. **SCP** – Surgical Case Planning (which operating theater is assigned to each patient)

The goal is to **minimize a cost function** that penalizes soft constraint violations, while always respecting all hard constraints.

---

## The planning horizon

- The planning period lasts **D days** (a multiple of 7: 14, 21, or 28 days)
- Each day has **3 shifts**: early (morning), late (afternoon), night
- In total, there are **3D shifts**, numbered from 0 to 3D−1

---

## Entities involved

**Infrastructure resources:**

- **Operating Theaters (OT):** have a maximum daily capacity in minutes; some may be unavailable on certain days
- **Rooms:** have a number of beds (capacity); some rooms are incompatible with certain patients

**Human resources:**

- **Nurses:** have a skill level (from 0 to L−1), a fixed roster of working shifts, and a maximum workload per shift
- **Surgeons:** have a maximum surgery time per day (0 = unavailable)

**Patients:**

- Can be **mandatory** (must be admitted during the period) or **optional** (can be postponed)
- For each patient, the following are known:
    - Release date (earliest possible admission)
    - Due date (only for mandatory patients)
    - Age group
    - Gender
    - Length of stay
    - Incompatible rooms
    - Assigned surgeon
    - Surgery duration
    - For each shift of the stay: workload generated and minimum required nurse skill level
- **Occupants:** patients already present at the start of the period, with fixed room and admission date

---

## The solution to produce

For each instance, we must determine 4 things:

1. **Admission date** for each patient (or postponement for optional ones)
2. **Room** assigned to each admitted patient (for the entire stay, no transfers)
3. **Nurse** assigned to each occupied room for each shift
4. **Operating theater** assigned to each patient for the day of surgery (which coincides with the admission day)

---

## Hard Constraints (H) — must always be respected

| Code | Description |
| --- | --- |
| H1 | No gender mix in the same room on the same day |
| H2 | Patient must be in a compatible room |
| H3 | Surgeon cannot exceed their daily maximum surgery time |
| H4 | Operating theater cannot exceed its daily capacity |
| H5 | All mandatory patients must be admitted |
| H6 | Patient can only be admitted between their release and due dates |
| H7 | Number of patients in a room cannot exceed its capacity |
| H8 | Every occupied room must have an assigned nurse on duty |

---

## Soft Constraints (S) — contribute to the cost to minimize

| Code | Description | Weight (example) |
| --- | --- | --- |
| S1 | Minimize the difference in age groups in the same room | 5 |
| S2 | Assigned nurse must have sufficient skill for the patients | 10 |
| S3 | Minimize the number of distinct nurses caring for a patient (continuity of care) | 5 |
| S4 | Nurse should not exceed their maximum workload | 10 |
| S5 | Minimize the number of operating theaters opened each day | 20 |
| S6 | Minimize the number of different theaters a surgeon operates in on the same day | 1 |
| S7 | Minimize admission delay relative to release date | 5 |
| S8 | Minimize the number of optional patients not admitted | 350 |

Weights are **instance-specific** and are included in the input JSON file.

---

## Our approach (student perspective)

The goal is to implement a **greedy solution** — that is, a constructive approach, without local search or advanced optimization. The greedy approach means making decisions one at a time in a "reasonable" way (e.g., mandatory patients first, then optional; assign the most compatible rooms first, etc.), without backtracking to improve.

A possible greedy approach, broken down into phases:

1. **Order the patients** (e.g., mandatory first by urgency, then optional)
2. **Assign admission date and room** respecting H1, H2, H6, H7
3. **Assign the operating theater** respecting H3, H4 (e.g., the one with the most residual capacity)
4. **Assign nurses** to rooms for each shift, respecting H8 and trying to minimize S2, S3, S4

---

## Repository structure and workflow

- All code and data are organized in this repository.
- **Dataset:** 30 public instances (`i01`–`i30`) in JSON format, plus 10 test instances (`test01`–`test10`) with example solutions, used for testing.
- **Validator:** provided as C++ source code, to be compiled with g++. It takes the instance and solution files as input and returns both hard constraint violations and the total cost.
- **File format:** described in Appendix A of the paper — both input and output are in **JSON**.

Typical workflow:
1. Select a JSON instance →
2. Run your greedy algorithm on it →
3. Obtain a solution JSON →
4. Pass it to the validator to see the violations and total cost →
5. Compare your cost with the upper bounds reported in Table 4 of the paper.

---
