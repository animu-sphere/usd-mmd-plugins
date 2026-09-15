// SPDX-License-Identifier: Apache-2.0
#include "mmdPmx/Document.h"

namespace mmd::pmx {

std::string_view
ToString(Version version)
{
    return version == Version::V2_1 ? "2.1" : "2.0";
}

std::string_view
ToString(DeformType type)
{
    switch (type) {
    case DeformType::Bdef1:
        return "BDEF1";
    case DeformType::Bdef2:
        return "BDEF2";
    case DeformType::Bdef4:
        return "BDEF4";
    case DeformType::Sdef:
        return "SDEF";
    case DeformType::Qdef:
        return "QDEF";
    }
    return "unknown";
}

std::string_view
ToString(MorphType type)
{
    switch (type) {
    case MorphType::Group:
        return "group";
    case MorphType::Vertex:
        return "vertex";
    case MorphType::Bone:
        return "bone";
    case MorphType::Uv:
        return "uv";
    case MorphType::AdditionalUv1:
        return "additionalUv1";
    case MorphType::AdditionalUv2:
        return "additionalUv2";
    case MorphType::AdditionalUv3:
        return "additionalUv3";
    case MorphType::AdditionalUv4:
        return "additionalUv4";
    case MorphType::Material:
        return "material";
    case MorphType::Flip:
        return "flip";
    case MorphType::Impulse:
        return "impulse";
    }
    return "unknown";
}

std::size_t
Morph::OffsetCount() const
{
    switch (type) {
    case MorphType::Group:
    case MorphType::Flip:
        return groupOffsets.size();
    case MorphType::Vertex:
        return vertexOffsets.size();
    case MorphType::Bone:
        return boneOffsets.size();
    case MorphType::Uv:
    case MorphType::AdditionalUv1:
    case MorphType::AdditionalUv2:
    case MorphType::AdditionalUv3:
    case MorphType::AdditionalUv4:
        return uvOffsets.size();
    case MorphType::Material:
        return materialOffsets.size();
    case MorphType::Impulse:
        return impulseOffsets.size();
    }
    return 0;
}

} // namespace mmd::pmx
