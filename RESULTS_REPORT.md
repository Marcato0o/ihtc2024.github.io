# IHTC 2024 — Greedy Solver · Performance Report

*Generated: 2026-03-20 &nbsp;·&nbsp; Solver: Single-Pass Greedy (MRV + minimum marginal cost) &nbsp;·&nbsp; Instances: 40 (10 test + 30 competition)*

---

## Overview

This report benchmarks the greedy solver against the official IHTC 2024 reference solutions. The solver runs in a **single pass**: patients are scheduled greedily (most-constrained first) to a `(day, room, OT)` triple minimising marginal soft costs; nurses are then assigned greedily to every occupied `(day, shift, room)` slot. No backtracking is performed. Each section shows the full cost breakdown (8 soft constraints S1–S8) with the weighted cost used in scoring, the raw violation count, and the delta versus the reference solution.

**Legend**: ⬆ worse than reference · ⬇ better than reference · ═ equal · ✅ feasible · ❌ hard constraint violation

---

## Global Summary

| Instance | D | Rooms | Patients (M+O) | Admitted | Hard | Our Total | Ref Total | Δ | Ratio | Time |
|:---|:---:|:---:|:---:|:---:|:---:|---:|---:|---:|:---:|---:|
| test01 | 21 | 5 | 42 (11+31) | 29 | ✅ | 4,521 | 3,177 | +1,344 ⬆ | ×1.42 | 50 ms |
| test02 | 14 | 6 | 37 (12+25) | 34 | ✅ | 2,597 | 1,583 | +1,014 ⬆ | ×1.64 | 51 ms |
| test03 | 14 | 6 | 45 (12+33) | 16 | ✅ | 11,616 | 10,184 | +1,432 ⬆ | ×1.14 | 49 ms |
| test04 | 14 | 8 | 54 (32+22) | 52 | ✅ | 3,538 | 2,332 | +1,206 ⬆ | ×1.52 | 56 ms |
| test05 | 14 | 6 | 62 (19+43) | 22 | ✅ | 17,517 | 15,713 | +1,804 ⬆ | ×1.11 | 50 ms |
| test06 | 14 | 9 | 111 (50+61) | 69 | ✅ | 26,799 | 18,558 | +8,241 ⬆ | ×1.44 | 91 ms |
| test07 | 21 | 9 | 113 (55+58) | 66 | ✅ | 19,946 | 17,048 | +2,898 ⬆ | ×1.17 | 100 ms |
| test08 | 21 | 9 | 173 (39+134) | 63 | ✅ | 31,399 | 24,947 | +6,452 ⬆ | ×1.26 | 155 ms |
| test09 | 21 | 14 | 146 (88+58) | 97 | ✅ | 26,851 | 20,492 | +6,359 ⬆ | ×1.31 | 132 ms |
| test10 | 21 | 46 | 525 (349+176) | 446 | ✅ | 76,660 | 49,444 | +27,216 ⬆ | ×1.55 | 11570 ms |
| i01 | 14 | 4 | 28 (0+28) | 16 | ✅ | 5,524 | 3,842 | +1,682 ⬆ | ×1.44 | 42 ms |
| i02 | 14 | 5 | 37 (12+25) | 36 | ✅ | 2,651 | 1,264 | +1,387 ⬆ | ×2.10 | 48 ms |
| i03 | 14 | 4 | 45 (7+38) | 16 | ✅ | 13,485 | 10,490 | +2,995 ⬆ | ×1.29 | 52 ms |
| i04 | 14 | 9 | 54 (36+18) | 54 | ✅ | 4,449 | 1,884 | +2,565 ⬆ | ×2.36 | 57 ms |
| i05 | 21 | 10 | 93 (6+87) | 43 | ✅ | 16,540 | 12,760 | +3,780 ⬆ | ×1.30 | 99 ms |
| i06 | 21 | 7 | 105 (28+77) | 32 | ✅ | 12,228 | 10,671 | +1,557 ⬆ | ×1.15 | 77 ms |
| i07 | 14 | 13 | 79 (25+54) | 79 | ✅ | 8,713 | 4,985 | +3,728 ⬆ | ×1.75 | 90 ms |
| i08 | 28 | 14 | 174 (138+36) | 174 | ✅ | 10,165 | 6,249 | +3,916 ⬆ | ×1.63 | 378 ms |
| i09 | 14 | 12 | 96 (7+89) | 87 | ✅ | 14,206 | 6,611 | +7,595 ⬆ | ×2.15 | 102 ms |
| i10 | 21 | 8 | 156 (49+107) | 84 | ✅ | 34,005 | 20,705 | +13,300 ⬆ | ×1.64 | 163 ms |
| i11 | 14 | 15 | 112 (28+84) | 48 | ✅ | 35,147 | 25,938 | +9,209 ⬆ | ×1.36 | 118 ms |
| i12 | 14 | 12 | 121 (74+47) | 87 | ✅ | 17,647 | 12,375 | +5,272 ⬆ | ×1.43 | 118 ms |
| i13 | 14 | 10 | 129 (27+102) | 88 | ✅ | 31,804 | 17,328 | +14,476 ⬆ | ×1.84 | 173 ms |
| i14 | 14 | 21 | 138 (79+59) | 120 | ✅ | 17,802 | 9,591 | +8,211 ⬆ | ×1.86 | 202 ms |
| i15 | 14 | 16 | 146 (44+102) | 128 | ✅ | 25,566 | 12,486 | +13,080 ⬆ | ×2.05 | 309 ms |
| i16 | 14 | 17 | 154 (125+29) | 125 | ❌ 6 | 15,808 | 10,139 | +5,669 ⬆ | ×1.56 | 130 ms |
| i17 | 28 | 18 | 325 (22+303) | 269 | ✅ | 80,615 | 40,535 | +40,080 ⬆ | ×1.99 | 1935 ms |
| i18 | 21 | 19 | 257 (61+196) | 109 | ✅ | 50,472 | 37,660 | +12,812 ⬆ | ×1.34 | 774 ms |
| i19 | 28 | 25 | 359 (67+292) | 330 | ✅ | 80,028 | 43,857 | +36,171 ⬆ | ×1.82 | 5957 ms |
| i20 | 14 | 15 | 188 (108+80) | 124 | ✅ | 45,091 | 29,098 | +15,993 ⬆ | ×1.55 | 268 ms |
| i21 | 21 | 21 | 294 (37+257) | 221 | ✅ | 47,414 | 24,526 | +22,888 ⬆ | ×1.93 | 1402 ms |
| i22 | 28 | 20 | 409 (199+210) | 279 | ✅ | 106,940 | 47,861 | +59,079 ⬆ | ×2.23 | 3564 ms |
| i23 | 28 | 26 | 426 (157+269) | 321 | ✅ | 65,124 | 37,550 | +27,574 ⬆ | ×1.73 | 4620 ms |
| i24 | 28 | 33 | 443 (306+137) | 335 | ✅ | 46,526 | 33,221 | +13,305 ⬆ | ×1.40 | 4021 ms |
| i25 | 14 | 29 | 230 (19+211) | 205 | ✅ | 21,381 | 11,517 | +9,864 ⬆ | ×1.86 | 1121 ms |
| i26 | 28 | 26 | 476 (195+281) | 354 | ✅ | 122,724 | 64,352 | +58,372 ⬆ | ×1.91 | 8694 ms |
| i27 | 28 | 26 | 493 (123+370) | 364 | ✅ | 111,837 | 50,976 | +60,861 ⬆ | ×2.19 | 9624 ms |
| i28 | 21 | 26 | 383 (192+191) | 217 | ✅ | 93,743 | 75,172 | +18,571 ⬆ | ×1.25 | 1964 ms |
| i29 | 14 | 28 | 264 (56+208) | 208 | ✅ | 26,931 | 12,199 | +14,732 ⬆ | ×2.21 | 2102 ms |
| i30 | 21 | 25 | 408 (212+196) | 287 | ✅ | 59,076 | 37,387 | +21,689 ⬆ | ×1.58 | 3308 ms |

---

## Analysis

### Key Metrics

| Metric | Value |
|:---|---:|
| Instances with zero hard violations | 39 / 40 |
| Sum of our costs (40 instances) | 1,445,086 |
| Sum of reference costs (40 instances) | 876,707 |
| Overall cost ratio | ×1.65 |

### Average Cost Ratio per Soft Constraint

| Constraint | Avg Ours | Avg Ref | Avg Δ | Avg Ratio |
|:---|---:|---:|---:|:---:|
| S1 · Room Age Mix | 126 | 92 | +34 ⬆ | ×1.36 |
| S2 · Room Skill Level | 100 | 631 | -531 ⬇ | ×0.16 |
| S3 · Continuity of Care | 6,002 | 3,123 | +2,879 ⬆ | ×1.92 |
| S4 · Excess Workload | 4,212 | 268 | +3,944 ⬆ | ×15.72 |
| S5 · Open Operating Theater | 1,106 | 1,039 | +67 ⬆ | ×1.06 |
| S6 · Surgeon Transfer | 221 | 43 | +178 ⬆ | ×5.24 |
| S7 · Patient Delay | 5,094 | 3,894 | +1,201 ⬆ | ×1.31 |
| S8 · Unscheduled Patients | 19,266 | 12,828 | +6,439 ⬆ | ×1.50 |

### Best Performing Instances (closest to reference)

| Instance | Our Total | Ref Total | Ratio |
|:---|---:|---:|:---:|
| test05 | 17,517 | 15,713 | ×1.11 |
| test03 | 11,616 | 10,184 | ×1.14 |
| i06 | 12,228 | 10,671 | ×1.15 |

### Furthest from Reference

| Instance | Our Total | Ref Total | Ratio |
|:---|---:|---:|:---:|
| i04 | 4,449 | 1,884 | ×2.36 |
| i22 | 106,940 | 47,861 | ×2.23 |
| i29 | 26,931 | 12,199 | ×2.21 |

---

## Test Instances

---

### test01

> **Days** 21 &nbsp;|&nbsp; **Rooms** 5 &nbsp;|&nbsp; **Nurses** 13 &nbsp;|&nbsp; **OTs** 2 &nbsp;|&nbsp; **Surgeons** 1

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 42 | 11 | 31 | 29 | 0 ✅ | 50 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 5 | 130 | 26 | 35 | 7 | +95 ⬆ | ×3.71 |
| S2 · Room Skill Level | 1 | 4 | 4 | 43 | 43 | -39 ⬇ | ×0.09 |
| S3 · Continuity of Care | 5 | 1,375 | 275 | 885 | 177 | +490 ⬆ | ×1.55 |
| S4 · Excess Workload | 1 | 36 | 36 | 24 | 24 | +12 ⬆ | ×1.50 |
| S5 · Open Operating Theater | 30 | 360 | 12 | 330 | 11 | +30 ⬆ | ×1.09 |
| S6 · Surgeon Transfer | 1 | 1 | 1 | 0 | 0 | +1 ⬆ | — |
| S7 · Patient Delay | 5 | 665 | 133 | 660 | 132 | +5 ⬆ | ×1.01 |
| S8 · Unscheduled Patients | 150 | 1,950 | 13 | 1,200 | 8 | +750 ⬆ | ×1.62 |
| **TOTAL** | | **4,521** | | **3,177** | | **+1,344 ⬆** | **×1.42** |

---

### test02

> **Days** 14 &nbsp;|&nbsp; **Rooms** 6 &nbsp;|&nbsp; **Nurses** 17 &nbsp;|&nbsp; **OTs** 3 &nbsp;|&nbsp; **Surgeons** 2

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 37 | 12 | 25 | 34 | 0 ✅ | 51 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 5 | 115 | 23 | 45 | 9 | +70 ⬆ | ×2.56 |
| S2 · Room Skill Level | 1 | 39 | 39 | 118 | 118 | -79 ⬇ | ×0.33 |
| S3 · Continuity of Care | 1 | 305 | 305 | 221 | 221 | +84 ⬆ | ×1.38 |
| S4 · Excess Workload | 1 | 203 | 203 | 9 | 9 | +194 ⬆ | ×22.56 |
| S5 · Open Operating Theater | 10 | 160 | 16 | 140 | 14 | +20 ⬆ | ×1.14 |
| S6 · Surgeon Transfer | 10 | 30 | 3 | 0 | 0 | +30 ⬆ | — |
| S7 · Patient Delay | 5 | 695 | 139 | 700 | 140 | -5 ⬇ | ×0.99 |
| S8 · Unscheduled Patients | 350 | 1,050 | 3 | 350 | 1 | +700 ⬆ | ×3.00 |
| **TOTAL** | | **2,597** | | **1,583** | | **+1,014 ⬆** | **×1.64** |

---

### test03

> **Days** 14 &nbsp;|&nbsp; **Rooms** 6 &nbsp;|&nbsp; **Nurses** 14 &nbsp;|&nbsp; **OTs** 2 &nbsp;|&nbsp; **Surgeons** 1

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 45 | 12 | 33 | 16 | 0 ✅ | 49 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 1 | 8 | 8 | 9 | 9 | -1 ⬇ | ×0.89 |
| S2 · Room Skill Level | 1 | 3 | 3 | 35 | 35 | -32 ⬇ | ×0.09 |
| S3 · Continuity of Care | 5 | 805 | 161 | 570 | 114 | +235 ⬆ | ×1.41 |
| S4 · Excess Workload | 10 | 30 | 3 | 0 | 0 | +30 ⬆ | — |
| S5 · Open Operating Theater | 10 | 50 | 5 | 50 | 5 | **= 0** ═ | ×1.00 |
| S6 · Surgeon Transfer | 5 | 0 | 0 | 0 | 0 | **= 0** ═ | — |
| S7 · Patient Delay | 15 | 570 | 38 | 420 | 28 | +150 ⬆ | ×1.36 |
| S8 · Unscheduled Patients | 350 | 10,150 | 29 | 9,100 | 26 | +1,050 ⬆ | ×1.12 |
| **TOTAL** | | **11,616** | | **10,184** | | **+1,432 ⬆** | **×1.14** |

---

### test04

> **Days** 14 &nbsp;|&nbsp; **Rooms** 8 &nbsp;|&nbsp; **Nurses** 19 &nbsp;|&nbsp; **OTs** 3 &nbsp;|&nbsp; **Surgeons** 2

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 54 | 32 | 22 | 52 | 0 ✅ | 56 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 1 | 10 | 10 | 22 | 22 | -12 ⬇ | ×0.45 |
| S2 · Room Skill Level | 5 | 0 | 0 | 195 | 39 | -195 ⬇ | ×0.00 |
| S3 · Continuity of Care | 1 | 471 | 471 | 350 | 350 | +121 ⬆ | ×1.35 |
| S4 · Excess Workload | 5 | 1,290 | 258 | 25 | 5 | +1,265 ⬆ | ×51.60 |
| S5 · Open Operating Theater | 10 | 150 | 15 | 150 | 15 | **= 0** ═ | ×1.00 |
| S6 · Surgeon Transfer | 1 | 7 | 7 | 0 | 0 | +7 ⬆ | — |
| S7 · Patient Delay | 10 | 1,110 | 111 | 1,090 | 109 | +20 ⬆ | ×1.02 |
| S8 · Unscheduled Patients | 250 | 500 | 2 | 500 | 2 | **= 0** ═ | ×1.00 |
| **TOTAL** | | **3,538** | | **2,332** | | **+1,206 ⬆** | **×1.52** |

---

### test05

> **Days** 14 &nbsp;|&nbsp; **Rooms** 6 &nbsp;|&nbsp; **Nurses** 15 &nbsp;|&nbsp; **OTs** 2 &nbsp;|&nbsp; **Surgeons** 1

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 62 | 19 | 43 | 22 | 0 ✅ | 50 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 5 | 5 | 1 | 5 | 1 | **= 0** ═ | ×1.00 |
| S2 · Room Skill Level | 1 | 5 | 5 | 32 | 32 | -27 ⬇ | ×0.16 |
| S3 · Continuity of Care | 5 | 1,060 | 212 | 725 | 145 | +335 ⬆ | ×1.46 |
| S4 · Excess Workload | 1 | 17 | 17 | 6 | 6 | +11 ⬆ | ×2.83 |
| S5 · Open Operating Theater | 30 | 270 | 9 | 270 | 9 | **= 0** ═ | ×1.00 |
| S6 · Surgeon Transfer | 10 | 0 | 0 | 0 | 0 | **= 0** ═ | — |
| S7 · Patient Delay | 5 | 160 | 32 | 275 | 55 | -115 ⬇ | ×0.58 |
| S8 · Unscheduled Patients | 400 | 16,000 | 40 | 14,400 | 36 | +1,600 ⬆ | ×1.11 |
| **TOTAL** | | **17,517** | | **15,713** | | **+1,804 ⬆** | **×1.11** |

---

### test06

> **Days** 14 &nbsp;|&nbsp; **Rooms** 9 &nbsp;|&nbsp; **Nurses** 20 &nbsp;|&nbsp; **OTs** 3 &nbsp;|&nbsp; **Surgeons** 3

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 111 | 50 | 61 | 69 | 0 ✅ | 91 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 5 | 35 | 7 | 65 | 13 | -30 ⬇ | ×0.54 |
| S2 · Room Skill Level | 1 | 0 | 0 | 59 | 59 | -59 ⬇ | ×0.00 |
| S3 · Continuity of Care | 5 | 2,880 | 576 | 1,885 | 377 | +995 ⬆ | ×1.53 |
| S4 · Excess Workload | 1 | 64 | 64 | 19 | 19 | +45 ⬆ | ×3.37 |
| S5 · Open Operating Theater | 30 | 570 | 19 | 570 | 19 | **= 0** ═ | ×1.00 |
| S6 · Surgeon Transfer | 10 | 90 | 9 | 30 | 3 | +60 ⬆ | ×3.00 |
| S7 · Patient Delay | 10 | 2,160 | 216 | 2,930 | 293 | -770 ⬇ | ×0.74 |
| S8 · Unscheduled Patients | 500 | 21,000 | 42 | 13,000 | 26 | +8,000 ⬆ | ×1.62 |
| **TOTAL** | | **26,799** | | **18,558** | | **+8,241 ⬆** | **×1.44** |

---

### test07

> **Days** 21 &nbsp;|&nbsp; **Rooms** 9 &nbsp;|&nbsp; **Nurses** 22 &nbsp;|&nbsp; **OTs** 3 &nbsp;|&nbsp; **Surgeons** 2

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 113 | 55 | 58 | 66 | 0 ✅ | 100 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 5 | 435 | 87 | 185 | 37 | +250 ⬆ | ×2.35 |
| S2 · Room Skill Level | 10 | 1,880 | 188 | 1,940 | 194 | -60 ⬇ | ×0.97 |
| S3 · Continuity of Care | 5 | 3,555 | 711 | 2,360 | 472 | +1,195 ⬆ | ×1.51 |
| S4 · Excess Workload | 1 | 451 | 451 | 403 | 403 | +48 ⬆ | ×1.12 |
| S5 · Open Operating Theater | 50 | 1,150 | 23 | 1,150 | 23 | **= 0** ═ | ×1.00 |
| S6 · Surgeon Transfer | 10 | 110 | 11 | 20 | 2 | +90 ⬆ | ×5.50 |
| S7 · Patient Delay | 5 | 615 | 123 | 740 | 148 | -125 ⬇ | ×0.83 |
| S8 · Unscheduled Patients | 250 | 11,750 | 47 | 10,250 | 41 | +1,500 ⬆ | ×1.15 |
| **TOTAL** | | **19,946** | | **17,048** | | **+2,898 ⬆** | **×1.17** |

---

### test08

> **Days** 21 &nbsp;|&nbsp; **Rooms** 9 &nbsp;|&nbsp; **Nurses** 21 &nbsp;|&nbsp; **OTs** 3 &nbsp;|&nbsp; **Surgeons** 2

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 173 | 39 | 134 | 63 | 0 ✅ | 155 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 1 | 43 | 43 | 44 | 44 | -1 ⬇ | ×0.98 |
| S2 · Room Skill Level | 5 | 0 | 0 | 30 | 6 | -30 ⬇ | ×0.00 |
| S3 · Continuity of Care | 1 | 665 | 665 | 529 | 529 | +136 ⬆ | ×1.26 |
| S4 · Excess Workload | 1 | 233 | 233 | 154 | 154 | +79 ⬆ | ×1.51 |
| S5 · Open Operating Theater | 40 | 960 | 24 | 920 | 23 | +40 ⬆ | ×1.04 |
| S6 · Surgeon Transfer | 1 | 8 | 8 | 0 | 0 | +8 ⬆ | — |
| S7 · Patient Delay | 10 | 1,990 | 199 | 2,270 | 227 | -280 ⬇ | ×0.88 |
| S8 · Unscheduled Patients | 250 | 27,500 | 110 | 21,000 | 84 | +6,500 ⬆ | ×1.31 |
| **TOTAL** | | **31,399** | | **24,947** | | **+6,452 ⬆** | **×1.26** |

---

### test09

> **Days** 21 &nbsp;|&nbsp; **Rooms** 14 &nbsp;|&nbsp; **Nurses** 29 &nbsp;|&nbsp; **OTs** 2 &nbsp;|&nbsp; **Surgeons** 2

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 146 | 88 | 58 | 97 | 0 ✅ | 132 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 1 | 6 | 6 | 22 | 22 | -16 ⬇ | ×0.27 |
| S2 · Room Skill Level | 5 | 0 | 0 | 360 | 72 | -360 ⬇ | ×0.00 |
| S3 · Continuity of Care | 5 | 6,155 | 1231 | 3,380 | 676 | +2,775 ⬆ | ×1.82 |
| S4 · Excess Workload | 5 | 1,010 | 202 | 60 | 12 | +950 ⬆ | ×16.83 |
| S5 · Open Operating Theater | 20 | 640 | 32 | 660 | 33 | -20 ⬇ | ×0.97 |
| S6 · Surgeon Transfer | 5 | 100 | 20 | 0 | 0 | +100 ⬆ | — |
| S7 · Patient Delay | 10 | 1,790 | 179 | 1,660 | 166 | +130 ⬆ | ×1.08 |
| S8 · Unscheduled Patients | 350 | 17,150 | 49 | 14,350 | 41 | +2,800 ⬆ | ×1.20 |
| **TOTAL** | | **26,851** | | **20,492** | | **+6,359 ⬆** | **×1.31** |

---

### test10

> **Days** 21 &nbsp;|&nbsp; **Rooms** 46 &nbsp;|&nbsp; **Nurses** 83 &nbsp;|&nbsp; **OTs** 11 &nbsp;|&nbsp; **Surgeons** 10

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 525 | 349 | 176 | 446 | 0 ✅ | 11570 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 1 | 80 | 80 | 274 | 274 | -194 ⬇ | ×0.29 |
| S2 · Room Skill Level | 10 | 0 | 0 | 1,220 | 122 | -1,220 ⬇ | ×0.00 |
| S3 · Continuity of Care | 5 | 27,820 | 5564 | 13,965 | 2793 | +13,855 ⬆ | ×1.99 |
| S4 · Excess Workload | 5 | 9,170 | 1834 | 605 | 121 | +8,565 ⬆ | ×15.16 |
| S5 · Open Operating Theater | 50 | 4,950 | 99 | 4,650 | 93 | +300 ⬆ | ×1.06 |
| S6 · Surgeon Transfer | 10 | 1,930 | 193 | 490 | 49 | +1,440 ⬆ | ×3.94 |
| S7 · Patient Delay | 10 | 9,010 | 901 | 10,240 | 1024 | -1,230 ⬇ | ×0.88 |
| S8 · Unscheduled Patients | 300 | 23,700 | 79 | 18,000 | 60 | +5,700 ⬆ | ×1.32 |
| **TOTAL** | | **76,660** | | **49,444** | | **+27,216 ⬆** | **×1.55** |


---

## Competition Instances

---

### i01

> **Days** 14 &nbsp;|&nbsp; **Rooms** 4 &nbsp;|&nbsp; **Nurses** 12 &nbsp;|&nbsp; **OTs** 2 &nbsp;|&nbsp; **Surgeons** 1

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 28 | 0 | 28 | 16 | 0 ✅ | 42 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 5 | 20 | 4 | 15 | 3 | +5 ⬆ | ×1.33 |
| S2 · Room Skill Level | 10 | 250 | 25 | 190 | 19 | +60 ⬆ | ×1.32 |
| S3 · Continuity of Care | 1 | 144 | 144 | 127 | 127 | +17 ⬆ | ×1.13 |
| S4 · Excess Workload | 10 | 260 | 26 | 0 | 0 | +260 ⬆ | — |
| S5 · Open Operating Theater | 30 | 240 | 8 | 240 | 8 | **= 0** ═ | ×1.00 |
| S6 · Surgeon Transfer | 10 | 0 | 0 | 0 | 0 | **= 0** ═ | — |
| S7 · Patient Delay | 10 | 410 | 41 | 470 | 47 | -60 ⬇ | ×0.87 |
| S8 · Unscheduled Patients | 350 | 4,200 | 12 | 2,800 | 8 | +1,400 ⬆ | ×1.50 |
| **TOTAL** | | **5,524** | | **3,842** | | **+1,682 ⬆** | **×1.44** |

---

### i02

> **Days** 14 &nbsp;|&nbsp; **Rooms** 5 &nbsp;|&nbsp; **Nurses** 13 &nbsp;|&nbsp; **OTs** 2 &nbsp;|&nbsp; **Surgeons** 1

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 37 | 12 | 25 | 36 | 0 ✅ | 48 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 5 | 25 | 5 | 5 | 1 | +20 ⬆ | ×5.00 |
| S2 · Room Skill Level | 5 | 160 | 32 | 165 | 33 | -5 ⬇ | ×0.97 |
| S3 · Continuity of Care | 1 | 291 | 291 | 229 | 229 | +62 ⬆ | ×1.27 |
| S4 · Excess Workload | 10 | 1,130 | 113 | 10 | 1 | +1,120 ⬆ | ×113.00 |
| S5 · Open Operating Theater | 30 | 240 | 8 | 240 | 8 | **= 0** ═ | ×1.00 |
| S6 · Surgeon Transfer | 10 | 0 | 0 | 0 | 0 | **= 0** ═ | — |
| S7 · Patient Delay | 5 | 655 | 131 | 615 | 123 | +40 ⬆ | ×1.07 |
| S8 · Unscheduled Patients | 150 | 150 | 1 | 0 | 0 | +150 ⬆ | — |
| **TOTAL** | | **2,651** | | **1,264** | | **+1,387 ⬆** | **×2.10** |

---

### i03

> **Days** 14 &nbsp;|&nbsp; **Rooms** 4 &nbsp;|&nbsp; **Nurses** 10 &nbsp;|&nbsp; **OTs** 2 &nbsp;|&nbsp; **Surgeons** 1

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 45 | 7 | 38 | 16 | 0 ✅ | 52 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 5 | 50 | 10 | 0 | 0 | +50 ⬆ | — |
| S2 · Room Skill Level | 5 | 90 | 18 | 120 | 24 | -30 ⬇ | ×0.75 |
| S3 · Continuity of Care | 5 | 615 | 123 | 640 | 128 | -25 ⬇ | ×0.96 |
| S4 · Excess Workload | 5 | 55 | 11 | 5 | 1 | +50 ⬆ | ×11.00 |
| S5 · Open Operating Theater | 50 | 400 | 8 | 400 | 8 | **= 0** ═ | ×1.00 |
| S6 · Surgeon Transfer | 1 | 0 | 0 | 0 | 0 | **= 0** ═ | — |
| S7 · Patient Delay | 15 | 675 | 45 | 1,725 | 115 | -1,050 ⬇ | ×0.39 |
| S8 · Unscheduled Patients | 400 | 11,600 | 29 | 7,600 | 19 | +4,000 ⬆ | ×1.53 |
| **TOTAL** | | **13,485** | | **10,490** | | **+2,995 ⬆** | **×1.29** |

---

### i04

> **Days** 14 &nbsp;|&nbsp; **Rooms** 9 &nbsp;|&nbsp; **Nurses** 21 &nbsp;|&nbsp; **OTs** 2 &nbsp;|&nbsp; **Surgeons** 2

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 54 | 36 | 18 | 54 | 0 ✅ | 57 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 5 | 230 | 46 | 20 | 4 | +210 ⬆ | ×11.50 |
| S2 · Room Skill Level | 1 | 37 | 37 | 189 | 189 | -152 ⬇ | ×0.20 |
| S3 · Continuity of Care | 1 | 513 | 513 | 355 | 355 | +158 ⬆ | ×1.45 |
| S4 · Excess Workload | 5 | 1,760 | 352 | 80 | 16 | +1,680 ⬆ | ×22.00 |
| S5 · Open Operating Theater | 20 | 300 | 15 | 280 | 14 | +20 ⬆ | ×1.07 |
| S6 · Surgeon Transfer | 1 | 4 | 4 | 0 | 0 | +4 ⬆ | — |
| S7 · Patient Delay | 15 | 1,605 | 107 | 960 | 64 | +645 ⬆ | ×1.67 |
| S8 · Unscheduled Patients | 300 | 0 | 0 | 0 | 0 | **= 0** ═ | — |
| **TOTAL** | | **4,449** | | **1,884** | | **+2,565 ⬆** | **×2.36** |

---

### i05

> **Days** 21 &nbsp;|&nbsp; **Rooms** 10 &nbsp;|&nbsp; **Nurses** 25 &nbsp;|&nbsp; **OTs** 2 &nbsp;|&nbsp; **Surgeons** 1

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 93 | 6 | 87 | 43 | 0 ✅ | 99 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 5 | 5 | 1 | 5 | 1 | **= 0** ═ | ×1.00 |
| S2 · Room Skill Level | 10 | 30 | 3 | 20 | 2 | +10 ⬆ | ×1.50 |
| S3 · Continuity of Care | 1 | 475 | 475 | 335 | 335 | +140 ⬆ | ×1.42 |
| S4 · Excess Workload | 10 | 130 | 13 | 0 | 0 | +130 ⬆ | — |
| S5 · Open Operating Theater | 40 | 480 | 12 | 480 | 12 | **= 0** ═ | ×1.00 |
| S6 · Surgeon Transfer | 5 | 0 | 0 | 0 | 0 | **= 0** ═ | — |
| S7 · Patient Delay | 10 | 2,920 | 292 | 3,420 | 342 | -500 ⬇ | ×0.85 |
| S8 · Unscheduled Patients | 250 | 12,500 | 50 | 8,500 | 34 | +4,000 ⬆ | ×1.47 |
| **TOTAL** | | **16,540** | | **12,760** | | **+3,780 ⬆** | **×1.30** |

---

### i06

> **Days** 21 &nbsp;|&nbsp; **Rooms** 7 &nbsp;|&nbsp; **Nurses** 16 &nbsp;|&nbsp; **OTs** 2 &nbsp;|&nbsp; **Surgeons** 1

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 105 | 28 | 77 | 32 | 0 ✅ | 77 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 1 | 19 | 19 | 12 | 12 | +7 ⬆ | ×1.58 |
| S2 · Room Skill Level | 1 | 16 | 16 | 34 | 34 | -18 ⬇ | ×0.47 |
| S3 · Continuity of Care | 1 | 323 | 323 | 235 | 235 | +88 ⬆ | ×1.37 |
| S4 · Excess Workload | 5 | 290 | 58 | 0 | 0 | +290 ⬆ | — |
| S5 · Open Operating Theater | 20 | 240 | 12 | 240 | 12 | **= 0** ═ | ×1.00 |
| S6 · Surgeon Transfer | 10 | 0 | 0 | 0 | 0 | **= 0** ═ | — |
| S7 · Patient Delay | 5 | 390 | 78 | 250 | 50 | +140 ⬆ | ×1.56 |
| S8 · Unscheduled Patients | 150 | 10,950 | 73 | 9,900 | 66 | +1,050 ⬆ | ×1.11 |
| **TOTAL** | | **12,228** | | **10,671** | | **+1,557 ⬆** | **×1.15** |

---

### i07

> **Days** 14 &nbsp;|&nbsp; **Rooms** 13 &nbsp;|&nbsp; **Nurses** 29 &nbsp;|&nbsp; **OTs** 4 &nbsp;|&nbsp; **Surgeons** 3

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 79 | 25 | 54 | 79 | 0 ✅ | 90 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 5 | 100 | 20 | 70 | 14 | +30 ⬆ | ×1.43 |
| S2 · Room Skill Level | 10 | 320 | 32 | 450 | 45 | -130 ⬇ | ×0.71 |
| S3 · Continuity of Care | 5 | 4,135 | 827 | 2,225 | 445 | +1,910 ⬆ | ×1.86 |
| S4 · Excess Workload | 5 | 1,795 | 359 | 45 | 9 | +1,750 ⬆ | ×39.89 |
| S5 · Open Operating Theater | 20 | 400 | 20 | 340 | 17 | +60 ⬆ | ×1.18 |
| S6 · Surgeon Transfer | 1 | 13 | 13 | 5 | 5 | +8 ⬆ | ×2.60 |
| S7 · Patient Delay | 10 | 1,950 | 195 | 1,850 | 185 | +100 ⬆ | ×1.05 |
| S8 · Unscheduled Patients | 450 | 0 | 0 | 0 | 0 | **= 0** ═ | — |
| **TOTAL** | | **8,713** | | **4,985** | | **+3,728 ⬆** | **×1.75** |

---

### i08

> **Days** 28 &nbsp;|&nbsp; **Rooms** 14 &nbsp;|&nbsp; **Nurses** 32 &nbsp;|&nbsp; **OTs** 3 &nbsp;|&nbsp; **Surgeons** 4

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 174 | 138 | 36 | 174 | 0 ✅ | 378 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 5 | 205 | 41 | 105 | 21 | +100 ⬆ | ×1.95 |
| S2 · Room Skill Level | 10 | 0 | 0 | 0 | 0 | **= 0** ═ | — |
| S3 · Continuity of Care | 1 | 1,918 | 1918 | 1,158 | 1158 | +760 ⬆ | ×1.66 |
| S4 · Excess Workload | 1 | 712 | 712 | 616 | 616 | +96 ⬆ | ×1.16 |
| S5 · Open Operating Theater | 40 | 1,920 | 48 | 1,600 | 40 | +320 ⬆ | ×1.20 |
| S6 · Surgeon Transfer | 10 | 310 | 31 | 70 | 7 | +240 ⬆ | ×4.43 |
| S7 · Patient Delay | 10 | 5,100 | 510 | 2,300 | 230 | +2,800 ⬆ | ×2.22 |
| S8 · Unscheduled Patients | 200 | 0 | 0 | 400 | 2 | -400 ⬇ | ×0.00 |
| **TOTAL** | | **10,165** | | **6,249** | | **+3,916 ⬆** | **×1.63** |

---

### i09

> **Days** 14 &nbsp;|&nbsp; **Rooms** 12 &nbsp;|&nbsp; **Nurses** 26 &nbsp;|&nbsp; **OTs** 3 &nbsp;|&nbsp; **Surgeons** 4

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 96 | 7 | 89 | 87 | 0 ✅ | 102 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 5 | 330 | 66 | 80 | 16 | +250 ⬆ | ×4.12 |
| S2 · Room Skill Level | 1 | 9 | 9 | 152 | 152 | -143 ⬇ | ×0.06 |
| S3 · Continuity of Care | 5 | 4,080 | 816 | 2,350 | 470 | +1,730 ⬆ | ×1.74 |
| S4 · Excess Workload | 1 | 342 | 342 | 174 | 174 | +168 ⬆ | ×1.97 |
| S5 · Open Operating Theater | 20 | 440 | 22 | 440 | 22 | **= 0** ═ | ×1.00 |
| S6 · Surgeon Transfer | 10 | 140 | 14 | 10 | 1 | +130 ⬆ | ×14.00 |
| S7 · Patient Delay | 15 | 4,365 | 291 | 3,405 | 227 | +960 ⬆ | ×1.28 |
| S8 · Unscheduled Patients | 500 | 4,500 | 9 | 0 | 0 | +4,500 ⬆ | — |
| **TOTAL** | | **14,206** | | **6,611** | | **+7,595 ⬆** | **×2.15** |

---

### i10

> **Days** 21 &nbsp;|&nbsp; **Rooms** 8 &nbsp;|&nbsp; **Nurses** 20 &nbsp;|&nbsp; **OTs** 3 &nbsp;|&nbsp; **Surgeons** 2

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 156 | 49 | 107 | 84 | 0 ✅ | 163 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 5 | 215 | 43 | 90 | 18 | +125 ⬆ | ×2.39 |
| S2 · Room Skill Level | 5 | 480 | 96 | 1,250 | 250 | -770 ⬇ | ×0.38 |
| S3 · Continuity of Care | 5 | 3,515 | 703 | 2,815 | 563 | +700 ⬆ | ×1.25 |
| S4 · Excess Workload | 10 | 6,060 | 606 | 10 | 1 | +6,050 ⬆ | ×606.00 |
| S5 · Open Operating Theater | 20 | 440 | 22 | 460 | 23 | -20 ⬇ | ×0.96 |
| S6 · Surgeon Transfer | 5 | 45 | 9 | 0 | 0 | +45 ⬆ | — |
| S7 · Patient Delay | 5 | 1,650 | 330 | 2,580 | 516 | -930 ⬇ | ×0.64 |
| S8 · Unscheduled Patients | 300 | 21,600 | 72 | 13,500 | 45 | +8,100 ⬆ | ×1.60 |
| **TOTAL** | | **34,005** | | **20,705** | | **+13,300 ⬆** | **×1.64** |

---

### i11

> **Days** 14 &nbsp;|&nbsp; **Rooms** 15 &nbsp;|&nbsp; **Nurses** 34 &nbsp;|&nbsp; **OTs** 3 &nbsp;|&nbsp; **Surgeons** 2

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 112 | 28 | 84 | 48 | 0 ✅ | 118 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 1 | 2 | 2 | 2 | 2 | **= 0** ═ | ×1.00 |
| S2 · Room Skill Level | 1 | 25 | 25 | 54 | 54 | -29 ⬇ | ×0.46 |
| S3 · Continuity of Care | 1 | 535 | 535 | 327 | 327 | +208 ⬆ | ×1.64 |
| S4 · Excess Workload | 10 | 1,390 | 139 | 0 | 0 | +1,390 ⬆ | — |
| S5 · Open Operating Theater | 30 | 570 | 19 | 570 | 19 | **= 0** ═ | ×1.00 |
| S6 · Surgeon Transfer | 10 | 80 | 8 | 0 | 0 | +80 ⬆ | — |
| S7 · Patient Delay | 5 | 545 | 109 | 485 | 97 | +60 ⬆ | ×1.12 |
| S8 · Unscheduled Patients | 500 | 32,000 | 64 | 24,500 | 49 | +7,500 ⬆ | ×1.31 |
| **TOTAL** | | **35,147** | | **25,938** | | **+9,209 ⬆** | **×1.36** |

---

### i12

> **Days** 14 &nbsp;|&nbsp; **Rooms** 12 &nbsp;|&nbsp; **Nurses** 29 &nbsp;|&nbsp; **OTs** 5 &nbsp;|&nbsp; **Surgeons** 3

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 121 | 74 | 47 | 87 | 0 ✅ | 118 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 1 | 52 | 52 | 75 | 75 | -23 ⬇ | ×0.69 |
| S2 · Room Skill Level | 10 | 130 | 13 | 1,150 | 115 | -1,020 ⬇ | ×0.11 |
| S3 · Continuity of Care | 5 | 4,265 | 853 | 3,005 | 601 | +1,260 ⬆ | ×1.42 |
| S4 · Excess Workload | 5 | 3,655 | 731 | 665 | 133 | +2,990 ⬆ | ×5.50 |
| S5 · Open Operating Theater | 30 | 870 | 29 | 840 | 28 | +30 ⬆ | ×1.04 |
| S6 · Surgeon Transfer | 5 | 120 | 24 | 20 | 4 | +100 ⬆ | ×6.00 |
| S7 · Patient Delay | 15 | 1,755 | 117 | 1,020 | 68 | +735 ⬆ | ×1.72 |
| S8 · Unscheduled Patients | 200 | 6,800 | 34 | 5,600 | 28 | +1,200 ⬆ | ×1.21 |
| **TOTAL** | | **17,647** | | **12,375** | | **+5,272 ⬆** | **×1.43** |

---

### i13

> **Days** 14 &nbsp;|&nbsp; **Rooms** 10 &nbsp;|&nbsp; **Nurses** 24 &nbsp;|&nbsp; **OTs** 8 &nbsp;|&nbsp; **Surgeons** 5

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 129 | 27 | 102 | 88 | 0 ✅ | 173 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 5 | 265 | 53 | 200 | 40 | +65 ⬆ | ×1.32 |
| S2 · Room Skill Level | 1 | 26 | 26 | 318 | 318 | -292 ⬇ | ×0.08 |
| S3 · Continuity of Care | 5 | 4,060 | 812 | 2,865 | 573 | +1,195 ⬆ | ×1.42 |
| S4 · Excess Workload | 1 | 798 | 798 | 290 | 290 | +508 ⬆ | ×2.75 |
| S5 · Open Operating Theater | 10 | 310 | 31 | 300 | 30 | +10 ⬆ | ×1.03 |
| S6 · Surgeon Transfer | 10 | 130 | 13 | 40 | 4 | +90 ⬆ | ×3.25 |
| S7 · Patient Delay | 15 | 5,715 | 381 | 7,815 | 521 | -2,100 ⬇ | ×0.73 |
| S8 · Unscheduled Patients | 500 | 20,500 | 41 | 5,500 | 11 | +15,000 ⬆ | ×3.73 |
| **TOTAL** | | **31,804** | | **17,328** | | **+14,476 ⬆** | **×1.84** |

---

### i14

> **Days** 14 &nbsp;|&nbsp; **Rooms** 21 &nbsp;|&nbsp; **Nurses** 44 &nbsp;|&nbsp; **OTs** 4 &nbsp;|&nbsp; **Surgeons** 3

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 138 | 79 | 59 | 120 | 0 ✅ | 202 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 1 | 14 | 14 | 45 | 45 | -31 ⬇ | ×0.31 |
| S2 · Room Skill Level | 1 | 0 | 0 | 274 | 274 | -274 ⬇ | ×0.00 |
| S3 · Continuity of Care | 5 | 7,040 | 1408 | 3,070 | 614 | +3,970 ⬆ | ×2.29 |
| S4 · Excess Workload | 5 | 1,830 | 366 | 5 | 1 | +1,825 ⬆ | ×366.00 |
| S5 · Open Operating Theater | 50 | 1,250 | 25 | 1,150 | 23 | +100 ⬆ | ×1.09 |
| S6 · Surgeon Transfer | 1 | 23 | 23 | 12 | 12 | +11 ⬆ | ×1.92 |
| S7 · Patient Delay | 5 | 1,345 | 269 | 1,185 | 237 | +160 ⬆ | ×1.14 |
| S8 · Unscheduled Patients | 350 | 6,300 | 18 | 3,850 | 11 | +2,450 ⬆ | ×1.64 |
| **TOTAL** | | **17,802** | | **9,591** | | **+8,211 ⬆** | **×1.86** |

---

### i15

> **Days** 14 &nbsp;|&nbsp; **Rooms** 16 &nbsp;|&nbsp; **Nurses** 34 &nbsp;|&nbsp; **OTs** 7 &nbsp;|&nbsp; **Surgeons** 5

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 146 | 44 | 102 | 128 | 0 ✅ | 309 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 1 | 81 | 81 | 91 | 91 | -10 ⬇ | ×0.89 |
| S2 · Room Skill Level | 5 | 0 | 0 | 635 | 127 | -635 ⬇ | ×0.00 |
| S3 · Continuity of Care | 5 | 6,855 | 1371 | 3,765 | 753 | +3,090 ⬆ | ×1.82 |
| S4 · Excess Workload | 10 | 6,840 | 684 | 50 | 5 | +6,790 ⬆ | ×136.80 |
| S5 · Open Operating Theater | 10 | 360 | 36 | 360 | 36 | **= 0** ═ | ×1.00 |
| S6 · Surgeon Transfer | 5 | 170 | 34 | 15 | 3 | +155 ⬆ | ×11.33 |
| S7 · Patient Delay | 10 | 4,960 | 496 | 5,120 | 512 | -160 ⬇ | ×0.97 |
| S8 · Unscheduled Patients | 350 | 6,300 | 18 | 2,450 | 7 | +3,850 ⬆ | ×2.57 |
| **TOTAL** | | **25,566** | | **12,486** | | **+13,080 ⬆** | **×2.05** |

---

### i16

> **Days** 14 &nbsp;|&nbsp; **Rooms** 17 &nbsp;|&nbsp; **Nurses** 36 &nbsp;|&nbsp; **OTs** 5 &nbsp;|&nbsp; **Surgeons** 5

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 154 | 125 | 29 | 125 | 6 ❌ | 130 ms |

**Hard constraints**: ❌ Violations: H5 · Mandatory Unscheduled Patients: **6**

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 1 | 56 | 56 | 120 | 120 | -64 ⬇ | ×0.47 |
| S2 · Room Skill Level | 10 | 270 | 27 | 370 | 37 | -100 ⬇ | ×0.73 |
| S3 · Continuity of Care | 1 | 1,256 | 1256 | 885 | 885 | +371 ⬆ | ×1.42 |
| S4 · Excess Workload | 1 | 671 | 671 | 899 | 899 | -228 ⬇ | ×0.75 |
| S5 · Open Operating Theater | 40 | 1,320 | 33 | 1,400 | 35 | -80 ⬇ | ×0.94 |
| S6 · Surgeon Transfer | 5 | 175 | 35 | 30 | 6 | +145 ⬆ | ×5.83 |
| S7 · Patient Delay | 15 | 1,710 | 114 | 2,835 | 189 | -1,125 ⬇ | ×0.60 |
| S8 · Unscheduled Patients | 450 | 10,350 | 23 | 3,600 | 8 | +6,750 ⬆ | ×2.88 |
| **TOTAL** | | **15,808** | | **10,139** | | **+5,669 ⬆** | **×1.56** |
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

---

### i17

> **Days** 28 &nbsp;|&nbsp; **Rooms** 18 &nbsp;|&nbsp; **Nurses** 37 &nbsp;|&nbsp; **OTs** 5 &nbsp;|&nbsp; **Surgeons** 6

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 325 | 22 | 303 | 269 | 0 ✅ | 1935 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 5 | 515 | 103 | 310 | 62 | +205 ⬆ | ×1.66 |
| S2 · Room Skill Level | 10 | 210 | 21 | 3,040 | 304 | -2,830 ⬇ | ×0.07 |
| S3 · Continuity of Care | 5 | 14,200 | 2840 | 8,540 | 1708 | +5,660 ⬆ | ×1.66 |
| S4 · Excess Workload | 10 | 22,790 | 2279 | 270 | 27 | +22,520 ⬆ | ×84.41 |
| S5 · Open Operating Theater | 30 | 2,280 | 76 | 2,220 | 74 | +60 ⬆ | ×1.03 |
| S6 · Surgeon Transfer | 5 | 280 | 56 | 115 | 23 | +165 ⬆ | ×2.43 |
| S7 · Patient Delay | 5 | 12,340 | 2468 | 12,040 | 2408 | +300 ⬆ | ×1.02 |
| S8 · Unscheduled Patients | 500 | 28,000 | 56 | 14,000 | 28 | +14,000 ⬆ | ×2.00 |
| **TOTAL** | | **80,615** | | **40,535** | | **+40,080 ⬆** | **×1.99** |

---

### i18

> **Days** 21 &nbsp;|&nbsp; **Rooms** 19 &nbsp;|&nbsp; **Nurses** 41 &nbsp;|&nbsp; **OTs** 4 &nbsp;|&nbsp; **Surgeons** 3

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 257 | 61 | 196 | 109 | 0 ✅ | 774 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 5 | 5 | 1 | 5 | 1 | **= 0** ═ | ×1.00 |
| S2 · Room Skill Level | 1 | 0 | 0 | 112 | 112 | -112 ⬇ | ×0.00 |
| S3 · Continuity of Care | 1 | 1,264 | 1264 | 841 | 841 | +423 ⬆ | ×1.50 |
| S4 · Excess Workload | 10 | 1,390 | 139 | 0 | 0 | +1,390 ⬆ | — |
| S5 · Open Operating Theater | 30 | 780 | 26 | 750 | 25 | +30 ⬆ | ×1.04 |
| S6 · Surgeon Transfer | 1 | 23 | 23 | 2 | 2 | +21 ⬆ | ×11.50 |
| S7 · Patient Delay | 10 | 2,610 | 261 | 2,650 | 265 | -40 ⬇ | ×0.98 |
| S8 · Unscheduled Patients | 300 | 44,400 | 148 | 33,300 | 111 | +11,100 ⬆ | ×1.33 |
| **TOTAL** | | **50,472** | | **37,660** | | **+12,812 ⬆** | **×1.34** |

---

### i19

> **Days** 28 &nbsp;|&nbsp; **Rooms** 25 &nbsp;|&nbsp; **Nurses** 52 &nbsp;|&nbsp; **OTs** 8 &nbsp;|&nbsp; **Surgeons** 7

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 359 | 67 | 292 | 330 | 0 ✅ | 5957 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 1 | 128 | 128 | 192 | 192 | -64 ⬇ | ×0.67 |
| S2 · Room Skill Level | 10 | 0 | 0 | 760 | 76 | -760 ⬇ | ×0.00 |
| S3 · Continuity of Care | 5 | 20,265 | 4053 | 7,380 | 1476 | +12,885 ⬆ | ×2.75 |
| S4 · Excess Workload | 5 | 8,780 | 1756 | 610 | 122 | +8,170 ⬆ | ×14.39 |
| S5 · Open Operating Theater | 30 | 2,850 | 95 | 1,860 | 62 | +990 ⬆ | ×1.53 |
| S6 · Surgeon Transfer | 10 | 960 | 96 | 130 | 13 | +830 ⬆ | ×7.38 |
| S7 · Patient Delay | 15 | 39,795 | 2653 | 6,675 | 445 | +33,120 ⬆ | ×5.96 |
| S8 · Unscheduled Patients | 250 | 7,250 | 29 | 26,250 | 105 | -19,000 ⬇ | ×0.28 |
| **TOTAL** | | **80,028** | | **43,857** | | **+36,171 ⬆** | **×1.82** |

---

### i20

> **Days** 14 &nbsp;|&nbsp; **Rooms** 15 &nbsp;|&nbsp; **Nurses** 33 &nbsp;|&nbsp; **OTs** 7 &nbsp;|&nbsp; **Surgeons** 5

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 188 | 108 | 80 | 124 | 0 ✅ | 268 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 1 | 48 | 48 | 82 | 82 | -34 ⬇ | ×0.59 |
| S2 · Room Skill Level | 10 | 0 | 0 | 2,120 | 212 | -2,120 ⬇ | ×0.00 |
| S3 · Continuity of Care | 1 | 1,147 | 1147 | 961 | 961 | +186 ⬆ | ×1.19 |
| S4 · Excess Workload | 10 | 7,700 | 770 | 550 | 55 | +7,150 ⬆ | ×14.00 |
| S5 · Open Operating Theater | 40 | 1,360 | 34 | 1,400 | 35 | -40 ⬇ | ×0.97 |
| S6 · Surgeon Transfer | 1 | 31 | 31 | 5 | 5 | +26 ⬆ | ×6.20 |
| S7 · Patient Delay | 15 | 2,805 | 187 | 3,480 | 232 | -675 ⬇ | ×0.81 |
| S8 · Unscheduled Patients | 500 | 32,000 | 64 | 20,500 | 41 | +11,500 ⬆ | ×1.56 |
| **TOTAL** | | **45,091** | | **29,098** | | **+15,993 ⬆** | **×1.55** |

---

### i21

> **Days** 21 &nbsp;|&nbsp; **Rooms** 21 &nbsp;|&nbsp; **Nurses** 44 &nbsp;|&nbsp; **OTs** 7 &nbsp;|&nbsp; **Surgeons** 6

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 294 | 37 | 257 | 221 | 0 ✅ | 1402 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 1 | 120 | 120 | 123 | 123 | -3 ⬇ | ×0.98 |
| S2 · Room Skill Level | 5 | 0 | 0 | 1,490 | 298 | -1,490 ⬇ | ×0.00 |
| S3 · Continuity of Care | 5 | 12,390 | 2478 | 7,115 | 1423 | +5,275 ⬆ | ×1.74 |
| S4 · Excess Workload | 5 | 7,595 | 1519 | 235 | 47 | +7,360 ⬆ | ×32.32 |
| S5 · Open Operating Theater | 50 | 3,100 | 62 | 2,800 | 56 | +300 ⬆ | ×1.11 |
| S6 · Surgeon Transfer | 1 | 59 | 59 | 38 | 38 | +21 ⬆ | ×1.55 |
| S7 · Patient Delay | 5 | 5,900 | 1180 | 6,975 | 1395 | -1,075 ⬇ | ×0.85 |
| S8 · Unscheduled Patients | 250 | 18,250 | 73 | 5,750 | 23 | +12,500 ⬆ | ×3.17 |
| **TOTAL** | | **47,414** | | **24,526** | | **+22,888 ⬆** | **×1.93** |

---

### i22

> **Days** 28 &nbsp;|&nbsp; **Rooms** 20 &nbsp;|&nbsp; **Nurses** 44 &nbsp;|&nbsp; **OTs** 7 &nbsp;|&nbsp; **Surgeons** 6

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 409 | 199 | 210 | 279 | 0 ✅ | 3564 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 1 | 108 | 108 | 170 | 170 | -62 ⬇ | ×0.64 |
| S2 · Room Skill Level | 1 | 0 | 0 | 1,472 | 1472 | -1,472 ⬇ | ×0.00 |
| S3 · Continuity of Care | 5 | 15,070 | 3014 | 8,630 | 1726 | +6,440 ⬆ | ×1.75 |
| S4 · Excess Workload | 10 | 25,470 | 2547 | 50 | 5 | +25,420 ⬆ | ×509.40 |
| S5 · Open Operating Theater | 20 | 1,720 | 86 | 1,780 | 89 | -60 ⬇ | ×0.97 |
| S6 · Surgeon Transfer | 1 | 92 | 92 | 89 | 89 | +3 ⬆ | ×1.03 |
| S7 · Patient Delay | 5 | 5,980 | 1196 | 7,320 | 1464 | -1,340 ⬇ | ×0.82 |
| S8 · Unscheduled Patients | 450 | 58,500 | 130 | 28,350 | 63 | +30,150 ⬆ | ×2.06 |
| **TOTAL** | | **106,940** | | **47,861** | | **+59,079 ⬆** | **×2.23** |

---

### i23

> **Days** 28 &nbsp;|&nbsp; **Rooms** 26 &nbsp;|&nbsp; **Nurses** 55 &nbsp;|&nbsp; **OTs** 5 &nbsp;|&nbsp; **Surgeons** 5

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 426 | 157 | 269 | 321 | 0 ✅ | 4620 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 1 | 35 | 35 | 187 | 187 | -152 ⬇ | ×0.19 |
| S2 · Room Skill Level | 10 | 0 | 0 | 50 | 5 | -50 ⬇ | ×0.00 |
| S3 · Continuity of Care | 5 | 18,770 | 3754 | 7,720 | 1544 | +11,050 ⬆ | ×2.43 |
| S4 · Excess Workload | 1 | 806 | 806 | 929 | 929 | -123 ⬇ | ×0.87 |
| S5 · Open Operating Theater | 10 | 800 | 80 | 650 | 65 | +150 ⬆ | ×1.23 |
| S6 · Surgeon Transfer | 1 | 88 | 88 | 9 | 9 | +79 ⬆ | ×9.78 |
| S7 · Patient Delay | 15 | 23,625 | 1575 | 6,405 | 427 | +17,220 ⬆ | ×3.69 |
| S8 · Unscheduled Patients | 200 | 21,000 | 105 | 21,600 | 108 | -600 ⬇ | ×0.97 |
| **TOTAL** | | **65,124** | | **37,550** | | **+27,574 ⬆** | **×1.73** |

---

### i24

> **Days** 28 &nbsp;|&nbsp; **Rooms** 33 &nbsp;|&nbsp; **Nurses** 65 &nbsp;|&nbsp; **OTs** 6 &nbsp;|&nbsp; **Surgeons** 4

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 443 | 306 | 137 | 335 | 0 ✅ | 4021 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 1 | 4 | 4 | 4 | 4 | **= 0** ═ | ×1.00 |
| S2 · Room Skill Level | 1 | 0 | 0 | 405 | 405 | -405 ⬇ | ×0.00 |
| S3 · Continuity of Care | 1 | 3,862 | 3862 | 1,898 | 1898 | +1,964 ⬆ | ×2.03 |
| S4 · Excess Workload | 1 | 1,130 | 1130 | 39 | 39 | +1,091 ⬆ | ×28.97 |
| S5 · Open Operating Theater | 10 | 710 | 71 | 750 | 75 | -40 ⬇ | ×0.95 |
| S6 · Surgeon Transfer | 10 | 840 | 84 | 40 | 4 | +800 ⬆ | ×21.00 |
| S7 · Patient Delay | 5 | 2,180 | 436 | 1,735 | 347 | +445 ⬆ | ×1.26 |
| S8 · Unscheduled Patients | 350 | 37,800 | 108 | 28,350 | 81 | +9,450 ⬆ | ×1.33 |
| **TOTAL** | | **46,526** | | **33,221** | | **+13,305 ⬆** | **×1.40** |

---

### i25

> **Days** 14 &nbsp;|&nbsp; **Rooms** 29 &nbsp;|&nbsp; **Nurses** 57 &nbsp;|&nbsp; **OTs** 7 &nbsp;|&nbsp; **Surgeons** 9

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 230 | 19 | 211 | 205 | 0 ✅ | 1121 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 5 | 315 | 63 | 80 | 16 | +235 ⬆ | ×3.94 |
| S2 · Room Skill Level | 1 | 0 | 0 | 344 | 344 | -344 ⬇ | ×0.00 |
| S3 · Continuity of Care | 5 | 12,000 | 2400 | 4,680 | 936 | +7,320 ⬆ | ×2.56 |
| S4 · Excess Workload | 1 | 896 | 896 | 288 | 288 | +608 ⬆ | ×3.11 |
| S5 · Open Operating Theater | 20 | 1,140 | 57 | 1,040 | 52 | +100 ⬆ | ×1.10 |
| S6 · Surgeon Transfer | 5 | 290 | 58 | 45 | 9 | +245 ⬆ | ×6.44 |
| S7 · Patient Delay | 5 | 2,990 | 598 | 2,490 | 498 | +500 ⬆ | ×1.20 |
| S8 · Unscheduled Patients | 150 | 3,750 | 25 | 2,550 | 17 | +1,200 ⬆ | ×1.47 |
| **TOTAL** | | **21,381** | | **11,517** | | **+9,864 ⬆** | **×1.86** |

---

### i26

> **Days** 28 &nbsp;|&nbsp; **Rooms** 26 &nbsp;|&nbsp; **Nurses** 54 &nbsp;|&nbsp; **OTs** 8 &nbsp;|&nbsp; **Surgeons** 7

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 476 | 195 | 281 | 354 | 0 ✅ | 8694 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 1 | 119 | 119 | 201 | 201 | -82 ⬇ | ×0.59 |
| S2 · Room Skill Level | 1 | 0 | 0 | 1,166 | 1166 | -1,166 ⬇ | ×0.00 |
| S3 · Continuity of Care | 5 | 20,480 | 4096 | 10,115 | 2023 | +10,365 ⬆ | ×2.02 |
| S4 · Excess Workload | 10 | 22,570 | 2257 | 30 | 3 | +22,540 ⬆ | ×752.33 |
| S5 · Open Operating Theater | 10 | 900 | 90 | 880 | 88 | +20 ⬆ | ×1.02 |
| S6 · Surgeon Transfer | 5 | 535 | 107 | 90 | 18 | +445 ⬆ | ×5.94 |
| S7 · Patient Delay | 10 | 17,120 | 1712 | 19,370 | 1937 | -2,250 ⬇ | ×0.88 |
| S8 · Unscheduled Patients | 500 | 61,000 | 122 | 32,500 | 65 | +28,500 ⬆ | ×1.88 |
| **TOTAL** | | **122,724** | | **64,352** | | **+58,372 ⬆** | **×1.91** |

---

### i27

> **Days** 28 &nbsp;|&nbsp; **Rooms** 26 &nbsp;|&nbsp; **Nurses** 55 &nbsp;|&nbsp; **OTs** 8 &nbsp;|&nbsp; **Surgeons** 8

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 493 | 123 | 370 | 364 | 0 ✅ | 9624 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 1 | 160 | 160 | 298 | 298 | -138 ⬇ | ×0.54 |
| S2 · Room Skill Level | 10 | 0 | 0 | 3,930 | 393 | -3,930 ⬇ | ×0.00 |
| S3 · Continuity of Care | 1 | 4,247 | 4247 | 3,518 | 3518 | +729 ⬆ | ×1.21 |
| S4 · Excess Workload | 10 | 24,870 | 2487 | 740 | 74 | +24,130 ⬆ | ×33.61 |
| S5 · Open Operating Theater | 30 | 3,210 | 107 | 3,210 | 107 | **= 0** ═ | ×1.00 |
| S6 · Surgeon Transfer | 10 | 1,100 | 110 | 290 | 29 | +810 ⬆ | ×3.79 |
| S7 · Patient Delay | 5 | 13,750 | 2750 | 16,490 | 3298 | -2,740 ⬇ | ×0.83 |
| S8 · Unscheduled Patients | 500 | 64,500 | 129 | 22,500 | 45 | +42,000 ⬆ | ×2.87 |
| **TOTAL** | | **111,837** | | **50,976** | | **+60,861 ⬆** | **×2.19** |

---

### i28

> **Days** 21 &nbsp;|&nbsp; **Rooms** 26 &nbsp;|&nbsp; **Nurses** 51 &nbsp;|&nbsp; **OTs** 6 &nbsp;|&nbsp; **Surgeons** 5

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 383 | 192 | 191 | 217 | 0 ✅ | 1964 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 1 | 1 | 1 | 68 | 68 | -67 ⬇ | ×0.01 |
| S2 · Room Skill Level | 5 | 0 | 0 | 245 | 49 | -245 ⬇ | ×0.00 |
| S3 · Continuity of Care | 5 | 12,505 | 2501 | 5,360 | 1072 | +7,145 ⬆ | ×2.33 |
| S4 · Excess Workload | 1 | 587 | 587 | 524 | 524 | +63 ⬆ | ×1.12 |
| S5 · Open Operating Theater | 50 | 2,750 | 55 | 2,600 | 52 | +150 ⬆ | ×1.06 |
| S6 · Surgeon Transfer | 5 | 330 | 66 | 45 | 9 | +285 ⬆ | ×7.33 |
| S7 · Patient Delay | 10 | 2,870 | 287 | 2,430 | 243 | +440 ⬆ | ×1.18 |
| S8 · Unscheduled Patients | 450 | 74,700 | 166 | 63,900 | 142 | +10,800 ⬆ | ×1.17 |
| **TOTAL** | | **93,743** | | **75,172** | | **+18,571 ⬆** | **×1.25** |

---

### i29

> **Days** 14 &nbsp;|&nbsp; **Rooms** 28 &nbsp;|&nbsp; **Nurses** 60 &nbsp;|&nbsp; **OTs** 10 &nbsp;|&nbsp; **Surgeons** 10

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 264 | 56 | 208 | 208 | 0 ✅ | 2102 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 5 | 350 | 70 | 145 | 29 | +205 ⬆ | ×2.41 |
| S2 · Room Skill Level | 1 | 0 | 0 | 498 | 498 | -498 ⬇ | ×0.00 |
| S3 · Continuity of Care | 1 | 2,570 | 2570 | 1,452 | 1452 | +1,118 ⬆ | ×1.77 |
| S4 · Excess Workload | 1 | 1,806 | 1806 | 139 | 139 | +1,667 ⬆ | ×12.99 |
| S5 · Open Operating Theater | 10 | 640 | 64 | 720 | 72 | -80 ⬇ | ×0.89 |
| S6 · Surgeon Transfer | 10 | 620 | 62 | 40 | 4 | +580 ⬆ | ×15.50 |
| S7 · Patient Delay | 5 | 4,145 | 829 | 5,005 | 1001 | -860 ⬇ | ×0.83 |
| S8 · Unscheduled Patients | 300 | 16,800 | 56 | 4,200 | 14 | +12,600 ⬆ | ×4.00 |
| **TOTAL** | | **26,931** | | **12,199** | | **+14,732 ⬆** | **×2.21** |

---

### i30

> **Days** 21 &nbsp;|&nbsp; **Rooms** 25 &nbsp;|&nbsp; **Nurses** 50 &nbsp;|&nbsp; **OTs** 7 &nbsp;|&nbsp; **Surgeons** 7

| Patients | Mandatory | Optional | Admitted | Mand. unscheduled | Exec. time |
|:---:|:---:|:---:|:---:|:---:|:---:|
| 408 | 212 | 196 | 287 | 0 ✅ | 3308 ms |

**Hard constraints**: ✅ All satisfied

| Constraint | Wt | Ours | (raw) | Ref | (raw) | Δ | Ratio |
|:---|:---:|---:|---:|---:|---:|---:|:---:|
| S1 · Room Age Mix | 5 | 595 | 119 | 180 | 36 | +415 ⬆ | ×3.31 |
| S2 · Room Skill Level | 10 | 0 | 0 | 200 | 20 | -200 ⬇ | ×0.00 |
| S3 · Continuity of Care | 5 | 16,200 | 3240 | 7,460 | 1492 | +8,740 ⬆ | ×2.17 |
| S4 · Excess Workload | 1 | 1,876 | 1876 | 2,176 | 2176 | -300 ⬇ | ×0.86 |
| S5 · Open Operating Theater | 40 | 2,960 | 74 | 2,680 | 67 | +280 ⬆ | ×1.10 |
| S6 · Surgeon Transfer | 1 | 105 | 105 | 36 | 36 | +69 ⬆ | ×2.92 |
| S7 · Patient Delay | 15 | 13,140 | 876 | 5,655 | 377 | +7,485 ⬆ | ×2.32 |
| S8 · Unscheduled Patients | 200 | 24,200 | 121 | 19,000 | 95 | +5,200 ⬆ | ×1.27 |
| **TOTAL** | | **59,076** | | **37,387** | | **+21,689 ⬆** | **×1.58** |
