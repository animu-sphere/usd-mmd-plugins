# Phase 6 against distributed models (2026-09-17)

Dated evidence from real runs; append-only
([contributing/documentation.md](../contributing/documentation.md)).

## What was run

Two runs over the PMX files held locally — distributed MMD character and prop
models, none of them a fixture, none committed or redistributed
([DESIGN_POLICY.md §13](../design/DESIGN_POLICY.md#13-testing-policy)) — on
Windows 11, MSVC, OpenUSD 26.08.

1. **The Euler order (PMX-O1), from the source alone.** A script independent
   of `mmdPmx` read every rigid body. A capsule's long axis is its local Y,
   and a modeller lays a capsule along the bone it follows. For every capsule
   attached to a bone whose rotation turns about at least two axes, it
   composed the Euler angles in each of the six orders and asked, per order,
   whether the rotated Y axis lies along the bone — head to tail, or head to
   the body's centre — to within `|cos| > 0.99995`.
2. **The importer at Phase 6.** Each of the sixteen files with rigid bodies
   was opened with `Usd.Stage.Open`, and a script reading **only the stage**
   recovered every rigid body as (index, bone) through the Skeleton's
   `mmd:bone:sourceIndex`, and every joint as (index, body A, body B) through
   `physics:body0` / `physics:body1`, and compared them with the independent
   reader. It repeated the capsule test on the stage — the collider's Y axis
   turned by the body's `xformOp:orient`, against the bone from the
   Skeleton's bind transforms and `/Asset/rig/Bones` — and ran every
   validator OpenUSD registers, `usdPhysicsValidators` included.

## The Euler order

Of 822 capsules turned about two or more axes, 437 lie exactly along their
bone under some order:

| Order (`R =`, column vectors) | Exactly aligned |
| --- | ---: |
| `Ry · Rx · Rz` | **434** |
| `Ry · Rz · Rx` | 150 |
| `Rx · Ry · Rz` | 133 |
| `Rz · Ry · Rx` | 133 |
| `Rz · Rx · Ry` | 110 |
| `Rx · Rz · Ry` | 70 |

Every capsule another order aligns is aligned by `Ry · Rx · Rz` too, except
three; the others are the bodies a coincidence aligns under several orders.
`Ry · Rx · Rz` — Z first, then X, then Y — is Direct3D's yaw-pitch-roll,
which MMD is built on. PMX-O1 is resolved to it
([PMX_CONTRACT.md §13](../design/PMX_CONTRACT.md#13-rigid-bodies-and-joints)).

## Physics on the stage

Modes are follow-bone / dynamic / dynamic-with-bone. Joints are all
`spring6Dof` in these files, and every one is a `PhysicsJoint`.

| Model | Bodies | Modes | Joints | Limits | Capsules aligned | Opened in | Recovered | Validation errors |
| --- | ---: | --- | ---: | ---: | ---: | ---: | --- | ---: |
| A | 195 | 41/28/126 | 250 | 1440 | 28/62 | 0.46 s | yes | 0 |
| B | 381 | 36/122/223 | 600 | 3534 | 50/74 | 0.71 s | yes | 0 |
| C | 173 | 28/121/24 | 229 | 1374 | 24/29 | 0.49 s | yes | 0 |
| D | 177 | 30/123/24 | 231 | 1386 | 24/29 | 0.50 s | yes | 0 |
| E | 3 | 1/0/2 | 2 | 12 | 0/0 | 0.02 s | yes | 0 |
| F | 449 | 36/353/60 | 733 | 4374 | 8/32 | 0.83 s | yes | 0 |
| G | 349 | 41/230/78 | 493 | 2958 | 7/77 | 0.58 s | yes | 0 |
| H | 265 | 78/105/82 | 292 | 1752 | 77/80 | 1.44 s | yes | 0 |
| I | 401 | 35/185/181 | 541 | 3207 | 16/76 | 0.71 s | yes | 0 |
| J | 6 | 2/2/2 | 6 | 30 | 3/4 | 0.05 s | yes | 0 |
| K | 20 | 3/6/11 | 23 | 120 | 8/14 | 0.10 s | yes | 0 |
| L | 518 | 45/420/53 | 811 | 4866 | 45/105 | 0.97 s | yes | 0 |
| M | 254 | 43/76/135 | 348 | 2088 | 22/36 | 0.40 s | yes | 0 |
| N | 23 | 2/18/3 | 30 | 180 | 4/4 | 0.14 s | yes | 0 |
| O | 571 | 38/405/128 | 873 | 5169 | 59/100 | 1.10 s | yes | 0 |
| P | 571 | 38/405/128 | 873 | 5169 | 59/100 | 1.11 s | yes | 0 |

"Limits" counts the `PhysicsLimitAPI` instances authored: six per joint less
each free axis. "Capsules aligned" is the capsule test on the stage.

- **Every rigid body and joint is recovered.** On all sixteen models the
  bodies, their bones and every joint's two bodies read back from the stage
  alone are exactly the source's — Phase 6's acceptance
  ([DESIGN_POLICY.md §14](../design/DESIGN_POLICY.md#14-phases)). No joint
  names a missing body, so none was dropped.
- **The conversion keeps the alignment.** On the stage, 434 of the 822
  capsules lie along their bone — the source's count under the chosen order:
  the mirror, the unit scale, the quaternion and the joint order changed
  nothing a capsule's alignment depends on.
- **Free axes are real.** Of 38,010 source limit pairs, 351 have a lower
  limit above the upper one — a free axis in Bullet, and one that would lock
  in `UsdPhysics` — and author no limit.
- **Collision masks are masks.** 4,003 of 4,356 bodies clear their own
  group's bit and set most others, so the stored value reads as "collides
  with". In 62 of 98 used groups, bodies of one group carry different masks,
  which `PhysicsCollisionGroup` cannot express; the stage preserves group and
  mask only.
- **Validation is unchanged.** Adding `/Asset/physics` leaves every OpenUSD
  validator passing, the physics ones included.
- **No new diagnostics.** None of these files has an undefined shape, mode or
  joint type, or a PMX 2.1 joint type; the fixtures cover each.
