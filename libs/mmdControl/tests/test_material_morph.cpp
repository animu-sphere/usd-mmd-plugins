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

MorphMaterialOffset
Constant(MaterialMorphOperation operation, std::int32_t material, float value)
{
    return {material,
            operation,
            {value, value, value, value},
            {value, value, value},
            value,
            {value, value, value},
            {value, value, value, value},
            value,
            {value, value, value, value},
            {value, value, value, value},
            {value, value, value, value}};
}

std::vector<double>
Components(const MaterialMorphValues& v)
{
    return {v.diffuseColor[0],  v.diffuseColor[1],  v.diffuseColor[2],  v.diffuseColor[3],
            v.specularColor[0], v.specularColor[1], v.specularColor[2], v.specularPower,
            v.ambientColor[0],  v.ambientColor[1],  v.ambientColor[2],  v.edgeColor[0],
            v.edgeColor[1],     v.edgeColor[2],     v.edgeColor[3],     v.edgeSize,
            v.textureTint[0],   v.textureTint[1],   v.textureTint[2],   v.textureTint[3],
            v.sphereTint[0],    v.sphereTint[1],    v.sphereTint[2],    v.sphereTint[3],
            v.toonTint[0],      v.toonTint[1],      v.toonTint[2],      v.toonTint[3]};
}

CanonicalDocument
Model()
{
    CanonicalDocument model;
    model.materials.resize(3); // Material 2 is deliberately unbound by explicit offsets.
    model.morphs.resize(7);
    model.morphs[0].members = {{1, .5f}, {2, .25f}, {4, .5f}, {5, 1}, {6, 1}};
    model.morphs[1].members = {{3, 1}};
    model.morphs[2].members = {{3, 2}};
    model.morphs[3].type = MorphType::Material;
    model.morphs[3].materialOffsets = {Constant(MaterialMorphOperation::Multiply, kNone, 2),
                                       Constant(MaterialMorphOperation::Multiply, 1, 3)};
    model.morphs[4].type = MorphType::Material;
    model.morphs[4].materialOffsets = {{kNone,
                                        MaterialMorphOperation::Add,
                                        {1, 2, 3, 4},
                                        {5, 6, 7},
                                        8,
                                        {9, 10, 11},
                                        {12, 13, 14, 15},
                                        16,
                                        {17, 18, 19, 20},
                                        {21, 22, 23, 24},
                                        {25, 26, 27, 28}},
                                       Constant(MaterialMorphOperation::Add, 1, 4),
                                       Constant(MaterialMorphOperation::Add, 1, 4)};
    model.morphs[5].type = MorphType::Bone;
    model.morphs[6].type = MorphType::Uv;
    return model;
}

void
KnownModulation()
{
    auto model = Model();
    const auto source = model;
    auto evaluator = Evaluator::Prepare(model).value();
    const std::vector<MorphChannel> raw{
        {0, 2}, {3, -.25}, {3, -.25}, {4, -.25}, {4, -.25}, {5, 9}, {6, 9}, {-1, 1}, {999, 1}};
    const auto saved = raw;
    const auto result = evaluator.EvaluateMaterialMorphs(raw);
    assert(result.ok() && result.value().size() == 3 && raw == saved && model == source);
    // Diamond and duplicate direct input resolve weights to 1.5 and .5.
    for (std::size_t material = 0; material < 3; ++material) {
        const auto& row = result.value()[material];
        assert(row.material == std::int32_t(material));
        const auto multiply = Components(row.multiply), add = Components(row.add);
        for (std::size_t c = 0; c < multiply.size(); ++c) {
            assert(multiply[c] == (material == 1 ? 10 : 2.5));
            assert(add[c] == double(c + 1) * .5 + (material == 1 ? 4 : 0));
        }
        // A nonzero base keeps multiplication separate from addition.
        const double base = .25;
        assert(base * row.multiply.diffuseColor[0] + row.add.diffuseColor[0] ==
               (material == 1 ? 7 : 1.125));
    }
    assert(evaluator.EvaluateMaterialMorphs({}).value().empty());
    assert(evaluator.EvaluateMaterialMorphs(raw).value() == result.value());
    model = {};
    auto moved = std::move(evaluator);
    const auto copy = moved;
    assert(copy.EvaluateMaterialMorphs(raw).value() == result.value());

    // A negative weight extrapolates the multiplication factor, without clamping.
    const std::vector<MorphChannel> negative{{3, -2}};
    const auto extrapolated = copy.EvaluateMaterialMorphs(negative).value();
    assert(extrapolated[0].multiply.diffuseColor[0] == -1);
    assert(extrapolated[1].multiply.diffuseColor[0] == 3);

    const std::vector<MorphChannel> cancelled{{3, 1}, {3, -1}};
    const auto identity = copy.EvaluateMaterialMorphs(cancelled).value();
    assert(identity.size() == 3);
    for (const auto& row : identity) {
        for (double x : Components(row.multiply))
            assert(x == 1);
        for (double x : Components(row.add))
            assert(x == 0);
    }
    auto zero = Model();
    zero.morphs[0].members = {{3, 0}};
    const std::vector<MorphChannel> group{{0, 7}};
    assert(Evaluator::Prepare(zero).value().EvaluateMaterialMorphs(group).value() == identity);

    // Only explicit target 1 is reached; its zero row remains and others are absent.
    zero.morphs[3].materialOffsets.erase(zero.morphs[3].materialOffsets.begin());
    const auto sparse = Evaluator::Prepare(zero).value().EvaluateMaterialMorphs(group).value();
    assert(sparse.size() == 1 && sparse[0] == identity[1]);
    zero.materials.clear();
    zero.morphs[3].materialOffsets = {Constant(MaterialMorphOperation::Multiply, kNone, 2)};
    assert(Evaluator::Prepare(zero).value().EvaluateMaterialMorphs(group).value().empty());

    // Nonuniform source factors exercise every multiply component independently.
    auto varying = Model();
    varying.morphs[4].materialOffsets.resize(1);
    varying.morphs[4].materialOffsets[0].operation = MaterialMorphOperation::Multiply;
    const std::vector<MorphChannel> half{{4, .5}};
    const auto factors = Evaluator::Prepare(varying).value().EvaluateMaterialMorphs(half).value();
    for (std::size_t c = 0; c < 28; ++c) {
        assert(Components(factors[0].multiply)[c] == 1 + double(c) * .5);
    }

    // An earlier addition must not be scaled by a later multiplication.
    auto mixed = Model();
    mixed.morphs[3].materialOffsets = {Constant(MaterialMorphOperation::Add, 0, 4),
                                       Constant(MaterialMorphOperation::Multiply, 0, 3),
                                       Constant(MaterialMorphOperation::Add, 0, -2),
                                       Constant(MaterialMorphOperation::Multiply, 0, 2)};
    const std::vector<MorphChannel> full{{3, 1}};
    const auto separate = Evaluator::Prepare(mixed).value().EvaluateMaterialMorphs(full).value();
    assert(separate.size() == 1);
    assert(separate[0].multiply.diffuseColor[0] == 6 && separate[0].add.diffuseColor[0] == 2);
    assert(.5 * separate[0].multiply.diffuseColor[0] + separate[0].add.diffuseColor[0] == 5);
}

void
Refusals()
{
    const auto check = [](const CanonicalDocument& model, const std::vector<MorphChannel>& raw) {
        const auto evaluator = Evaluator::Prepare(model).value();
        const auto result = evaluator.EvaluateMaterialMorphs(raw);
        assert(!result.ok() && result.fatal()->code == codes::MotionMaterialMorphInvalid.id);
        assert(result.fatal()->severity == Severity::Fatal && !result.fatal()->recoverable);
        assert(evaluator.EvaluateMaterialMorphs({}).ok());
    };
    for (const auto target : {-2, 3}) {
        auto model = Model();
        model.morphs[3].materialOffsets[1].material = target;
        check(model, {{0, 0}}); // failure after an otherwise valid all-material offset
    }
    auto model = Model();
    model.morphs[3].materialOffsets[0].operation = static_cast<MaterialMorphOperation>(2);
    check(model, {{3, 0}});
    for (const auto value :
         {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
        model = Model();
        model.morphs[3].materialOffsets[0].toonTint[3] = value;
        check(model, {{3, 0}});
        model.materials.clear();
        check(model, {{3, 0}}); // malformed all-material offset with no targets
    }
    for (const auto value :
         {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        check(Model(), {{3, value}});
        check(Model(), {{0, value}});
    }
    const auto huge = std::numeric_limits<double>::max();
    check(Model(), {{3, huge}, {3, huge}}); // effective weight overflow
    check(Model(), {{4, huge}});            // weighted add overflow
    check(Model(), {{3, huge}});            // weighted multiply overflow
    model = Model();
    model.morphs[3].materialOffsets = {Constant(MaterialMorphOperation::Multiply, 0, 2),
                                       Constant(MaterialMorphOperation::Multiply, 0, 2)};
    check(model, {{3, huge}}); // finite individual factors, product overflow
    model.morphs[3].materialOffsets = {Constant(MaterialMorphOperation::Add, 0, 1),
                                       Constant(MaterialMorphOperation::Add, 0, 1)};
    check(model, {{3, huge}}); // finite individual terms, sum overflow
}

void
CanonicalAndContinuation()
{
    pmx::Document source;
    source.materials.resize(1);
    source.materials[0].diffuse = {.25f, .5f, .75f, 1};
    source.morphs.resize(3);
    source.morphs[0].type = pmx::MorphType::Group;
    source.morphs[0].groupOffsets = {{1, .5f}, {2, 1}};
    source.morphs[1].type = source.morphs[2].type = pmx::MorphType::Material;
    // Only diffuse is used in these independent base-plus-modulation checks.
    source.morphs[1].materialOffsets.resize(1);
    source.morphs[2].materialOffsets.resize(1);
    source.morphs[1].materialOffsets[0].material = -1;
    source.morphs[1].materialOffsets[0].operation = 0;
    source.morphs[1].materialOffsets[0].diffuse = {3, 2, 1, 0};
    source.morphs[2].materialOffsets[0].material = 0;
    source.morphs[2].materialOffsets[0].operation = 1;
    source.morphs[2].materialOffsets[0].diffuse = {.5f, .25f, 0, -.25f};
    const auto canonical = Canonicalize(source);
    assert(canonical.ok());
    const auto& model = canonical.value();
    const auto evaluator = Evaluator::Prepare(model).value();
    binding::BoundMotion motion;
    motion.morphs = {{0, {{0, 0}, {30, 2}}}};
    const auto pose = evaluator.Evaluate(motion, 15);
    const auto resumed = evaluator.ResumePhysics(evaluator.BeginPhysics(motion, 15));
    assert(resumed.ok() && resumed.value() == pose);
    const auto result = evaluator.EvaluateMaterialMorphs(pose.channels).value();
    assert(evaluator.EvaluateMaterialMorphs(resumed.value().channels).value() == result);
    const std::array<double, 4> expected{1, 1, .75, .25};
    for (std::size_t c = 0; c < 4; ++c) {
        assert(model.materials[0].diffuseColor[c] * result[0].multiply.diffuseColor[c] +
                   result[0].add.diffuseColor[c] ==
               expected[c]);
    }
    const auto reset =
        evaluator.EvaluateMaterialMorphs(evaluator.Evaluate(motion, 0).channels).value();
    for (const auto& row : reset) {
        for (double x : Components(row.multiply))
            assert(x == 1);
        for (double x : Components(row.add))
            assert(x == 0);
    }
}
} // namespace

void
RunMaterialMorphTests()
{
    KnownModulation();
    Refusals();
    CanonicalAndContinuation();
}
