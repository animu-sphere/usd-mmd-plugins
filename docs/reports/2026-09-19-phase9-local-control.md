# Phase 9 control evaluation against distributed models and motions (2026-09-19)

Dated evidence from real runs; append-only
([contributing/documentation.md](../contributing/documentation.md)).

## What was run

Two scratch programs, linking the workspace's libraries, over the 25 PMX
models and the two dance motions of the
[Phase 7 report](2026-09-17-phase7-local-motions.md) — distributed files, none
a fixture, none committed or redistributed
([DESIGN_POLICY.md §13](../design/DESIGN_POLICY.md#13-testing-policy)) — on
Windows 11, MSVC, Release.

1. **The rig survey.** `mmdPmx` and `mmdModel` over every model: IK loop
   counts and angle limits, which IK links are limited and how, appends and
   what they append from, external parents, after-physics bones, transform
   layers, and the morph types `mmdControl` evaluates.
2. **Evaluation.** `mmdControl` over every model of more than 60 bones — 12
   characters and one 68-bone partial — with motions A (2810 frames) and C
   (1201 frames) bound by `mmdMotionBinding`, every frame evaluated once in
   order and then again in reverse, twice per frame, comparing the poses bit
   for bit. For every leg chain (`左足ＩＫ`, `右足ＩＫ`), at every frame where
   it is enabled, the distance from the effector (`足首`) to the goal was
   measured, and the goal's distance from the hip compared with the leg's
   rest length.

## The rig survey

Over the 77 IK chains of the 25 models:

| Fact | Found |
| --- | --- |
| loop counts | 2 (26 chains), 3 (18), 10 (1), 20 (2), 40 (30); none above 40 |
| angle limits | 0° (3 chains), 4° (6), 18° (12), 90° (26), 114.59° (24), 229.18° (6) |
| limited links | 34, every one limited about exactly one axis — a *plane* link ([MOTION_CONTRACT.md §11.7](../design/MOTION_CONTRACT.md#117-ik)); 24 of them knees about X, `[0.5°, 180°]` or `[0°, 180°]` after conversion |
| most links in a chain | 2 |

Over the 12 characters' bones:

| Fact | Found |
| --- | --- |
| appends | 12–66 per model; translation appends in 3 models (46, 1 and 1) |
| an append whose source appends too (a chain) | 0–3 per model |
| an append whose source is an IK link (`足D` from `足`) | 6–8 per model |
| a source later in the evaluation order than its bone | none |
| local appends (flag `0x0080`) | none |
| external parents, after-physics bones | none |
| transform layers | 0–3 |
| bone morphs | in 5 models (1–7 each); group morphs with a bone member in 2 (17 each) |
| flip and impulse morphs | none |

So the loop bound of 256 is six times the largest count found, and the rules
no local model exercises — local appends, sources later in the order,
after-physics bones — are fixed by the synthetic rigs of `mmdControl_unit`
alone.

## Evaluation

Every evaluation of every model and motion gave the same bits twice, and
the reverse pass the same bits as the forward one: no pose depends on what
was evaluated before it. No value was non-finite. `Prepare` raised no
diagnostic on any model, and took 4–48 µs.

| Model (bones, chains) | A: µs per frame | A: leg frames within reach | A: residual p50 / p99 / max | C: µs per frame | C: leg frames within reach | C: residual p50 / p99 / max |
| --- | ---: | ---: | --- | ---: | ---: | --- |
| 280, 4 | 171 | 3174 | 0.08 / 9.1 / 20.1 mm | 157 | 1898 | 0.07 / 13.8 / 21.1 mm |
| 516, 4 | 147 | 3018 | 1.1 / 18.7 / 26.9 mm | 147 | 2042 | 7.1 / 25.8 / 27.5 mm |
| 490, 4 | 144 | 3119 | 0.3 / 10.7 / 16.9 mm | 141 | 2060 | 1.1 / 13.5 / 16.9 mm |
| 319, 12 | 172 | 2849 | 0.9 / 14.1 / 19.5 mm | 172 | 2048 | 4.4 / 17.6 / 20.7 mm |
| 319, 11 | 170 | 2849 | 0.9 / 14.1 / 19.5 mm | 165 | 2048 | 4.4 / 17.6 / 20.7 mm |
| 566, 4 | 150 | 3035 | 1.1 / 18.9 / 26.3 mm | 146 | 2047 | 5.2 / 21.7 / 26.7 mm |
| 444, 4 | 164 | 3176 | 0.05 / 6.5 / 16.3 mm | 151 | 1644 | 0.02 / 11.4 / 17.9 mm |
| 633, 4 | 150 | 3235 | 0.7 / 17.1 / 29.1 mm | 148 | 2065 | 2.7 / 23.8 / 28.8 mm |
| 366, 4 | 162 | 3209 | 0.7 / 14.1 / 20.7 mm | 155 | 2068 | 3.8 / 17.4 / 20.9 mm |
| 461, 16 | 194 | 3169 | 0.5 / 13.4 / 20.5 mm | 189 | 2066 | 2.9 / 15.6 / 20.5 mm |
| 692, 5 | 153 | 3219 | 0.8 / 17.3 / 29.1 mm | 150 | 2068 | 2.8 / 23.7 / 28.9 mm |
| 692, 5 | 154 | 3219 | 0.8 / 17.3 / 29.1 mm | 153 | 2068 | 2.8 / 23.7 / 28.9 mm |
| 68, 0 | 11 | — | — | 13 | — | — |

"Within reach" is a leg frame whose goal is no farther from the hip than 98%
of the leg's rest length; the rest — 2385–2771 of motion A's 5620 leg frames
per model, 334–758 of C's 2402 — ask for a straight or over-stretched leg,
where the distance left is reach, not the solver. Every model's legs follow
their goals: the median distance left is under a centimetre in every row.

**The distance left within reach is the loop count's.** Solving the same
frames of the 516-bone model with motion A, with every chain's loop count
overridden:

| Loop count | Residual p50 | p99 | max |
| ---: | ---: | ---: | ---: |
| 10 | 16.9 mm | 81.0 mm | 107 mm |
| 20 | 7.2 mm | 45.3 mm | 57.5 mm |
| 40 (the model's) | 1.1 mm | 18.7 mm | 26.9 mm |
| 80 | 0.05 mm | 5.7 mm | 9.5 mm |
| 160 | 0.05 mm | 0.17 mm | 0.72 mm |
| 256 | 0.05 mm | 0.08 mm | 0.08 mm |

The residual falls monotonically with the count: the rule converges, and at
the stored 40 iterations cyclic coordinate descent over a plane knee and a
free thigh has not finished on the hardest poses. Whether MMD's own playback
leaves the same distance at 40 iterations cannot be told without a reference
to compare against — MMD itself, or an independent implementation's output
on the same frames — and the rule is not changed without one: that is
MOT-O9 ([MOTION_CONTRACT.md §9](../design/MOTION_CONTRACT.md#9-open-questions)).

At 140–200 µs per frame for a 280–692-bone model, one evaluation is well
inside a 60 Hz frame.
