# Phase 8 finishing a retargeted pose on a PMX (2026-10-05)

Dated evidence from real runs; append-only
([contributing/documentation.md](../contributing/documentation.md)).

## Question

A recorded mocopi capture, retargeted onto PMX characters with
[MOTION_CONTRACT.md §10.9](../design/MOTION_CONTRACT.md#109-a-generic-clip-onto-a-pmx)'s
recipe, was viewed in `usdview`. Legs, body and head looked right on every
character. The arms looked wrong on two Miku models, whose sleeves stood
out from the arms, and on no other character. Every arm segment already
turns with the source to within 0.01°, so the bone directions were not the
cause. What else does a PMX need from a retargeted pose?

## What was run

A scratch program, outside this repository, against this repository's
install tree with the shoulder unaimed (MOT-O14) and `usd-motion-plugins`
v0.5.3. It replayed the mocopi device session of the earlier reports
through `motion-connectors` v0.1.0's `MocopiConnector`. It then retargeted
with §10.9's recipe onto the 17 local characters and, as an experiment,
finished each pose:

1. **Roll onto the twist bones.** Each sample's `腕` rotation was split into
   a swing and a twist about the bone's rest direction toward `ひじ`. The
   swing was kept, and the twist was composed onto `腕捩`. The same was done
   for `ひじ` toward `手首`, onto `手捩`.
2. **Held joints.** The bound joints, those twist bones, and every ancestor
   of either.
3. **The model's rig.** `mmdControl::Evaluator::Evaluate` ran over a bound
   motion whose only keys were the driven joints' given transforms. An IK
   chain was switched off when a link was held or fed a held joint's
   append. The evaluator's result was then written for every joint except
   the held ones, which kept the retarget's values.

It measured each enabled IK chain's effector-to-goal distance before and
after, over every sample. It also measured how far the evaluator alone would
have moved each held joint, and how much roll moved. The runs were on
Windows 11 with OpenUSD 26.08. None of the inputs is committed or
redistributed, and no image of a model is
([DESIGN_POLICY.md §13](../design/DESIGN_POLICY.md#13-testing-policy)).

## Results

**Three of the 17 characters keep arm helpers on IK.** Two variants of one
model chain `腕W`, `ひじW` and `手W`, with smaller shoulder helpers, each
toward an IK goal that hangs under the next arm joint. Their sleeves hang
from `手W`. A third model chains `腕W` toward `腕WIK` under `ひじ`. Before
the finish, those effectors sat from 9.7 mm to 580 mm from their goals: the
helpers stayed in the A-pose while the arm moved. After it, every one was
within 1.3 mm, and all but one within 0.003 mm. In `usdview` the sleeves
then followed the arms on both Miku models.

**The legs' IK is never solved.** Every character's four leg and toe
chains were switched off by the rule: their links feed `足D`'s append or
are bound themselves. The model's other chains were left on and changed
nothing they had not already reached. Those are ten skirt chains on one
model, a decorative chain on two, and one more on one model.

**Holding is necessary.** 14 of the 17 characters have `腰キャンセル`,
which appends −1 × `腰`'s rotation, and on them `腰` carries the `hips` role.
On every one of the 14, the evaluator alone would have turned both
`腰キャンセル` by up to 161.8°: the hips' turn when the walker turned back.
`腰キャンセル` is an ancestor of the bound `足D` joints. The retarget
solved those legs with it at rest, so letting it move would have spun the
legs against the body. The experiment kept every held joint's given
transform, so the bound joints left the finish exactly as the retarget left
them. Every other held joint agreed with the evaluator's own result to the
precision of the comparison.

**Roll is large and real.** The upper arm's roll about its bone, carried
onto `腕捩`, reached 48.8–54.4° per side over the take on every character.
A standing person's relaxed upper arm rolls about 30° inward, and the
source's own rolled about the same. The forearm's, carried onto `手捩`,
stayed under 3.4°, because the capture's wrists barely turn. With the roll
on `腕` instead of `腕捩`, `腕` turns the skin it shares with `肩` about the
bone, and the top of the shoulder looked squared.

**Characters without arm helpers do not change otherwise.** On a character
whose arms have only appended twist bones, the finished pose and the
unfinished one look the same. The roll moved to `腕捩`, and `腕捩1`–`3`
followed it through their appends.

## Findings

**A retargeted PMX pose needs the model's own rig.** MMD moves helper
joints from the joints a motion keys. A retarget writes only the bound
joints, so any mesh weighted to a helper is posed as the A-pose wherever the
helper is IK or an append from a moved joint. The finish is MMD's evaluation
over the retarget's answer. It is not a new rule.

**It must not move what the retarget solved.** The retarget computes each
bound joint against the rest of its ancestors. An append into an ancestor
(`腰キャンセル`), or an IK chain over a bound joint or a joint that feeds
one (the legs), would undo that. So the held joints take neither, and the
chains that would touch them stay off.

**The roll belongs to the twist bones.** That is where MMD motions put it,
and where the models weight it. Moving it is exact for every joint below
when the twist bone sits on the arm's bone line, because the twist is
composed back on the next joint down. Where it sits off the line, as it does
on some models, its offset has to turn with the roll, or the joints below it
shift by that offset's turn. §10.10 states that.

These are recorded as
[MOTION_CONTRACT.md §10.10](../design/MOTION_CONTRACT.md#1010-finishing-a-retargeted-pose-on-a-pmx)
and §11.9, MOT-O15.

## Limits

- A twist bone is found by its exact name. A model that names its twist
  bones otherwise keeps the roll on the arm.
- Physics is not simulated, as before: bones driven by rigid bodies, such
  as hair and skirts, stay where their parents carry them.
- The finish keeps an IK chain off when it touches a held joint. A model
  whose arm helpers share links with bound joints would keep those helpers
  unfinished. None of the local models does.
