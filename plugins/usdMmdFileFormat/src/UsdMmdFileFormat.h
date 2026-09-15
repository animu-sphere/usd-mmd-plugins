// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "pxr/pxr.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/usd/sdf/fileFormat.h"

PXR_NAMESPACE_OPEN_SCOPE

// The tokens that identify this file format to Sdf's layer registry.
#define USDMMD_FILE_FORMAT_TOKENS \
    ((Id, "pmx"))                 \
    ((Version, "1.0"))            \
    ((Target, "usd"))             \
    ((Extension, "pmx"))

TF_DECLARE_PUBLIC_TOKENS(UsdMmdFileFormatTokens, USDMMD_FILE_FORMAT_TOKENS);

/// SdfFileFormat that imports `.pmx` (PMX 2.0 / 2.1) models as the stage
/// docs/design/STAGE_CONTRACT.md fixes. It authors data only: nothing is
/// evaluated, solved or simulated while a file opens
/// (docs/design/DESIGN_POLICY.md §2.2).
class UsdMmdFileFormat : public SdfFileFormat {
public:
    bool CanRead(const std::string& file) const override;
    bool Read(SdfLayer* layer, const std::string& resolvedPath, bool metadataOnly) const override;
    bool WriteToString(
        const SdfLayer& layer,
        std::string* str,
        const std::string& comment = std::string()) const override;

protected:
    SDF_FILE_FORMAT_FACTORY_ACCESS;

    UsdMmdFileFormat();
    ~UsdMmdFileFormat() override;
};

PXR_NAMESPACE_CLOSE_SCOPE
