// SPDX-License-Identifier: Apache-2.0
#include <mmdControl/Codes.h>
#include <mmdControl/Evaluator.h>
#include <mmdModel/Canonicalize.h>

#include <cassert>
#include <limits>
#include <utility>

namespace {

using namespace mmd;
using namespace mmd::control;

CanonicalDocument
Model()
{
    CanonicalDocument model;
    model.mesh.points.resize(3);
    model.mesh.additionalUvCount = 4;
    model.morphs.resize(11);
    // Diamond: group 0 -> groups 1/2 -> UV 3. Each contributes .5.
    model.morphs[0].members = {{1, 0.5f}, {2, 0.25f}, {9, 1}, {10, 1}};
    model.morphs[1].members = {{3, 1}};
    model.morphs[2].members = {{3, 2}};
    model.morphs[3].type = MorphType::Uv;
    model.morphs[3].uvOffsets = {{2, {1, 2, 3, 4}}, {0, {2, 4, 6, 8}}, {2, {1, 0, 0, 0}}};
    for (std::uint8_t c = 1; c <= 4; ++c) {
        auto& morph = model.morphs[3 + c];
        morph.type = static_cast<MorphType>(static_cast<std::uint8_t>(MorphType::Uv) + c);
        morph.uvOffsets = {{2, {1, 2, 3, 4}}};
    }
    model.morphs[8].type = MorphType::Uv;
    model.morphs[8].uvOffsets = {{2, {4, 4, 4, 4}}};
    model.morphs[9].type = MorphType::Bone;
    model.morphs[10].type = MorphType::Material;
    return model;
}

void
KnownDeltas()
{
    auto model = Model();
    auto evaluator = Evaluator::Prepare(model).value();
    const std::vector<MorphChannel> raw{{0, 2},
                                        {3, -0.25},
                                        {3, -0.25},
                                        {8, -0.5},
                                        {4, -2},
                                        {5, 0},
                                        {6, 0.5},
                                        {7, 2},
                                        {9, 1},
                                        {10, 1},
                                        {-1, 5},
                                        {999, 5}};
    const auto saved = raw;
    // UV 3 has effective weight 1.5. UV 8 shares vertex 2 at weight -.5.
    const std::vector<UvMorphDelta> expected{{0, 0, {3, -6, 9, 12}},
                                             {0, 2, {1, -1, 2.5, 4}},
                                             {1, 2, {-2, -4, -6, -8}},
                                             {2, 2, {0, 0, 0, 0}},
                                             {3, 2, {0.5, 1, 1.5, 2}},
                                             {4, 2, {2, 4, 6, 8}}};
    const auto first = evaluator.EvaluateUvMorphs(raw);
    assert(first.ok() && first.value() == expected && raw == saved);
    assert(model.morphs[3].uvOffsets[0].delta == (Float4{1, 2, 3, 4}));
    assert(evaluator.EvaluateUvMorphs({}).value().empty());
    assert(evaluator.EvaluateUvMorphs(raw).value() == expected);
    // Prepared data and retained results survive source destruction and moves.
    model = {};
    auto moved = std::move(evaluator);
    const auto copy = moved;
    assert(copy.EvaluateUvMorphs(raw).value() == expected && first.value() == expected);
    const std::vector<MorphChannel> cancelled{{3, 1}, {3, -1}};
    const std::vector<UvMorphDelta> zeros{{0, 0, {}}, {0, 2, {}}};
    assert(copy.EvaluateUvMorphs(cancelled).value() == zeros);

    // A zero coefficient still reaches every offset for binding validation.
    auto zeroGroup = Model();
    zeroGroup.morphs[0].members = {{3, 0}};
    const std::vector<MorphChannel> group{{0, 7}};
    assert(Evaluator::Prepare(zeroGroup).value().EvaluateUvMorphs(group).value() == zeros);
}

void
CanonicalAndContinuation()
{
    pmx::Document source;
    source.header.globals.additionalVec4Count = 1;
    source.vertices.resize(1);
    source.vertices[0].uv = {0.25f, 0.75f};
    source.vertices[0].additionalVec4[0] = {1, 2, 3, 4};
    source.morphs.resize(3);
    source.morphs[0].type = pmx::MorphType::Group;
    source.morphs[0].groupOffsets = {{1, 0.5f}, {2, 1}};
    source.morphs[1].type = pmx::MorphType::Uv;
    source.morphs[1].uvOffsets = {{0, {0.5f, 0.25f, 2, 3}}};
    source.morphs[2].type = pmx::MorphType::AdditionalUv1;
    source.morphs[2].uvOffsets = {{0, {0.5f, 0.25f, 2, 3}}};
    const auto canonical = Canonicalize(source);
    assert(canonical.ok());
    const auto& model = canonical.value();
    assert(model.mesh.st[0] == (Float2{0.25f, 0.25f}));
    const auto evaluator = Evaluator::Prepare(model).value();
    binding::BoundMotion motion;
    motion.morphs = {{0, {{0, 0}, {30, 2}}}};
    const auto pose = evaluator.Evaluate(motion, 15);
    const auto resumed = evaluator.ResumePhysics(evaluator.BeginPhysics(motion, 15));
    assert(resumed.ok() && resumed.value() == pose);
    const std::vector<UvMorphDelta> expected{{0, 0, {0.25, -0.125, 1, 1.5}},
                                             {1, 0, {0.5, 0.25, 2, 3}}};
    assert(evaluator.EvaluateUvMorphs(pose.channels).value() == expected);
    assert(evaluator.EvaluateUvMorphs(resumed.value().channels).value() == expected);
    // Base UVs plus current delta, with V converted exactly once.
    assert(model.mesh.st[0][0] + expected[0].delta[0] == 0.5);
    assert(model.mesh.st[0][1] + expected[0].delta[1] == 0.125);
    assert(evaluator.EvaluateUvMorphs(evaluator.Evaluate(motion, 0).channels).value()[0].delta ==
           (std::array<double, 4>{}));
}

void
Refusals()
{
    const auto check = [](const CanonicalDocument& model,
                          const std::vector<MorphChannel>& channels) {
        const auto evaluator = Evaluator::Prepare(model).value();
        const auto result = evaluator.EvaluateUvMorphs(channels);
        assert(!result.ok());
        assert(result.fatal()->code == codes::MotionUvMorphInvalid.id);
        assert(result.fatal()->severity == Severity::Fatal && !result.fatal()->recoverable);
        // Failure does not poison later evaluations.
        assert(evaluator.EvaluateUvMorphs({}).ok());
    };
    auto model = Model();
    model.mesh.additionalUvCount = 0;
    check(model, {{4, 0}});
    model = Model();
    model.morphs[3].uvOffsets[0].vertex = -1;
    check(model, {{0, 0}});
    model.morphs[3].uvOffsets[0].vertex = 3;
    check(model, {{3, 0}});
    model = Model();
    for (const float value :
         {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
        model.morphs[3].uvOffsets[0].delta[2] = value;
        check(model, {{3, 0}});
    }
    model = Model();
    for (const double value :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        check(model, {{3, value}});
        check(model, {{0, value}});
    }
    const double huge = std::numeric_limits<double>::max();
    check(model, {{3, huge}, {3, huge}}); // effective-weight overflow
    check(model, {{3, huge}});            // weighted-offset overflow
    model.morphs[3].uvOffsets = {{0, {1, 0, 0, 0}}, {0, {1, 0, 0, 0}}};
    check(model, {{3, huge}}); // delta accumulation overflow
}

} // namespace

void
RunUvMorphTests()
{
    KnownDeltas();
    CanonicalAndContinuation();
    Refusals();
}
