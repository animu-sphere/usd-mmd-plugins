# Phase 9 skeletal motion acceptance (2026-09-22)

> Later: MOT-O10 was measured on 2026-09-25 ([report](2026-09-25-phase9-rest-pose-comparison.md)).

Dated evidence from real runs; append-only
([contributing/documentation.md](../contributing/documentation.md)).

## Question

Can a VMD-derived clip be evaluated with MMD leg IK, cross the shared
`motionUsd` representation, pose the skeleton of a PMX target through
`motionRetarget`, and also retarget onto a skeleton that has no MMD naming or
code on its generic path?

## Deterministic acceptance test

`mmdMotionAdapter_acceptance` constructs its inputs in memory, so it needs no
distributed asset and is repeatable in every CI cell:

1. A VMD document moves `左足ＩＫ` by 0.2 m upward and 0.05 m forward and
   enables the chain. It is grouped by `motionVmd::BuildMotion`, bound by
   `mmdMotionBinding`, and evaluated by `mmdControl` on a generated PMX
   humanoid.
2. `mmdMotionAdapter` samples two frames. The test verifies that both the
   upper-leg and knee humanoid rotations are non-identity even though the VMD
   keys only the IK goal: the control result, not the goal bone, crossed the
   boundary.
3. `motionUsd` v0.5.0 authors that `MotionClip` to an in-memory stage and reads
   it back. The sample count, source format and absence of read warnings are
   asserted.
4. A second generated PMX model, with different proportions, is adapted. Its
   exact joint tokens and rest transforms are authored as
   `/Asset/skel/Skeleton`, read back into a `SkeletonDescriptor`, and compared
   with the adapter's descriptor. `motionRetarget` produces one full target
   array per sample; the retargeted leg and knee are non-identity, diagnostics
   are empty, and a bound `/Asset/skel/RetargetedAnimation` names the target
   skeleton's joint order.
5. The same round-tripped clip is passed to `generic_retarget.cpp`. That source
   file and its header include only shared `motionRetarget` types. It retargets
   cleanly onto an English-named synthetic rig (`World/Pelvis`, `Leg_L`,
   `Shin_L`, and so on), and the leg remains driven.

The test passed on Windows 11 with OpenUSD 26.08 and the digest-pinned
`motionCore`, `motionRetarget` and `motionUsd` v0.5.0 artifacts. The workspace
graph check also passed with all three external edges declared.

## Local asset trial

A separate one-off run used a distributed Miku PMX as the VMD's source model,
`partial_lamb.vmd`, and `尼可.pmx` as the target. None of those inputs is a
fixture, and none is committed or redistributed.

The resulting flattened USDA contained 1,201 samples at 30 fps over frames
0–1,200, targeted the PMX stage's 539 joints, and reported zero retarget
diagnostics. It had no references or sublayers after flattening, passed
`usdchecker`, and was opened in `usdview`; the user observed the target moving.
The generated USDA remains local and is not part of the repository.

## Result and limits

The Phase 9 skeletal end-to-end acceptance item passes. It proves the
MMD-specific work ends at evaluated shared values, because the second
retarget call is compiled in a translation unit with no MMD dependency.

This does not resolve MOT-O10. The deterministic source and generic target use
identity rest rotations, while the local visual check established motion but
did not measure A-pose-to-target rest-direction correction. Expression-channel
interoperability is also a separate Phase 9 item.
