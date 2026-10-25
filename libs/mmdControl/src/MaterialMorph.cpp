// SPDX-License-Identifier: Apache-2.0
#include <mmdControl/Codes.h>
#include <mmdControl/Evaluator.h>

#include <cmath>
#include <map>
#include <string>
#include <type_traits>
#include <utility>

namespace mmd::control {
namespace {

// Visit the same semantic components in an offset or an evaluated value set.
template <class Source, class Function>
void
Visit(MaterialMorphValues& target, const Source& source, const Function& function)
{
    const auto visit = [&](auto& output, const auto& input) {
        if constexpr (std::is_arithmetic_v<std::remove_cvref_t<decltype(input)>>) {
            function(output, double(input));
        } else {
            for (std::size_t i = 0; i < input.size(); ++i) {
                function(output[i], double(input[i]));
            }
        }
    };
    visit(target.diffuseColor, source.diffuseColor);
    visit(target.specularColor, source.specularColor);
    visit(target.specularPower, source.specularPower);
    visit(target.ambientColor, source.ambientColor);
    visit(target.edgeColor, source.edgeColor);
    visit(target.edgeSize, source.edgeSize);
    visit(target.textureTint, source.textureTint);
    visit(target.sphereTint, source.sphereTint);
    visit(target.toonTint, source.toonTint);
}

} // namespace

Result<std::vector<MaterialMorphModulation>>
Evaluator::EvaluateMaterialMorphs(std::span<const MorphChannel> channels) const
{
    std::map<std::int32_t, MaterialMorphModulation> rows;
    for (const auto& resolved : ResolveMorphChannels(channels)) {
        const auto& morph = _morphs[static_cast<std::size_t>(resolved.morph)];
        if (morph.type != MorphType::Material) {
            continue;
        }
        const auto fail = [&](const std::string& reason) {
            return Result<std::vector<MaterialMorphModulation>>::Failure(
                MakeDiagnostic(codes::MotionMaterialMorphInvalid,
                               "material morph " + std::to_string(resolved.morph) + ": " + reason));
        };
        if (!std::isfinite(resolved.weight)) {
            return fail("effective weight is nonfinite");
        }
        for (const auto& offset : morph.materialOffsets) {
            if (offset.material < kNone ||
                (offset.material != kNone &&
                 static_cast<std::size_t>(offset.material) >= _materialCount) ||
                (offset.operation != MaterialMorphOperation::Multiply &&
                 offset.operation != MaterialMorphOperation::Add)) {
                return fail("offset must name a material (or all materials) and a valid operation");
            }
            const bool multiply = offset.operation == MaterialMorphOperation::Multiply;
            bool finite = true;
            MaterialMorphValues weighted;
            Visit(weighted, offset, [&](double& value, double source) {
                value =
                    multiply ? 1.0 + resolved.weight * (source - 1.0) : resolved.weight * source;
                finite &= std::isfinite(source) && std::isfinite(value);
            });
            // Validate reached declarations even at zero weight or with no materials.
            if (!finite) {
                return fail("offset or weighted modulation is nonfinite");
            }
            const auto first =
                offset.material == kNone ? std::size_t{0} : std::size_t(offset.material);
            const auto end = offset.material == kNone ? _materialCount : first + 1;
            for (auto material = first; material < end; ++material) {
                const auto index = static_cast<std::int32_t>(material);
                auto& row = rows[index];
                row.material = index;
                Visit(
                    multiply ? row.multiply : row.add, weighted, [&](double& value, double factor) {
                        value = multiply ? value * factor : value + factor;
                        finite &= std::isfinite(value);
                    });
                if (!finite) {
                    return fail("accumulated modulation is nonfinite");
                }
            }
        }
    }
    std::vector<MaterialMorphModulation> result;
    result.reserve(rows.size());
    for (auto& [index, row] : rows) {
        result.push_back(std::move(row));
    }
    return Result<std::vector<MaterialMorphModulation>>::Success(std::move(result));
}

} // namespace mmd::control
