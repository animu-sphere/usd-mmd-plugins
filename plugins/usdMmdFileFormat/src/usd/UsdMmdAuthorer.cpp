// SPDX-License-Identifier: Apache-2.0
#include "usd/UsdMmdAuthorer.h"

#include "pxr/base/tf/token.h"
#include "pxr/base/vt/array.h"
#include "pxr/base/vt/value.h"
#include "pxr/usd/kind/registry.h"
#include "pxr/usd/sdf/layer.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/usd/usd/modelAPI.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usdGeom/metrics.h"
#include "pxr/usd/usdGeom/tokens.h"
#include "pxr/usd/usdGeom/xform.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace usdmmd {

namespace {

// customData keys are USD key paths: "mmd:x" is key "x" in an "mmd"
// sub-dictionary, read back with GetCustomDataByKey("mmd:x")
// (STAGE_CONTRACT.md §3).
const TfToken kStageContractVersionKey("mmd:stageContractVersion");
const TfToken kSourceFormatKey("mmd:sourceFormat");
const TfToken kSourceVersionKey("mmd:sourceVersion");
const TfToken kDiagnosticsKey("mmd:diagnostics");

}  // namespace

bool
UsdMmdAuthorer::WriteToString(
    const mmd::pmx::Document& document,
    std::vector<mmd::Diagnostic>* diagnostics,
    std::string* outUsda) const
{
    if (!diagnostics || !outUsda) {
        return false;
    }

    const UsdStageRefPtr stage = UsdStage::CreateInMemory();
    if (!stage) {
        return false;
    }

    // Stage metadata (STAGE_CONTRACT.md §6): Y-up, meters. Every length is
    // converted to meters in canonicalization, never through metersPerUnit.
    UsdGeomSetStageUpAxis(stage, UsdGeomTokens->y);
    UsdGeomSetStageMetersPerUnit(stage, UsdGeomLinearUnits::meters);

    // /Asset (STAGE_CONTRACT.md §4). A model with no bones -- every model in
    // Phase 0, which reads no bone table -- authors /Asset as an Xform; from
    // Phase 2 a model with bones makes it the UsdSkelRoot.
    const UsdGeomXform asset = UsdGeomXform::Define(stage, SdfPath("/Asset"));
    if (!asset) {
        return false;
    }
    const UsdPrim assetPrim = asset.GetPrim();
    stage->SetDefaultPrim(assetPrim);
    UsdModelAPI(assetPrim).SetKind(KindTokens->component);

    // Model metadata and provenance (STAGE_CONTRACT.md §5), in key order.
    assetPrim.SetCustomDataByKey(kStageContractVersionKey, VtValue(kStageContractVersion));
    assetPrim.SetCustomDataByKey(kSourceFormatKey, VtValue(std::string("PMX")));
    assetPrim.SetCustomDataByKey(kSourceVersionKey,
        VtValue(std::string(mmd::pmx::ToString(document.header.version))));

    // Every recoverable diagnostic, in emission order, as "CODE: message".
    // The key is authored only when there is something to record.
    if (!diagnostics->empty()) {
        VtStringArray recorded;
        recorded.reserve(diagnostics->size());
        for (const mmd::Diagnostic& d : *diagnostics) {
            recorded.push_back(mmd::FormatDiagnostic(d));
        }
        assetPrim.SetCustomDataByKey(kDiagnosticsKey, VtValue(recorded));
    }

    return stage->GetRootLayer()->ExportToString(outUsda);
}

}  // namespace usdmmd
