// SPDX-License-Identifier: Apache-2.0
//
// What every document Read returns and every motion BuildMotion returns
// promise, checked by the robustness suite and the fuzz target alike.
#pragma once

#include <motionVmd/Cp932.h>
#include <motionVmd/Motion.h>

#include <string>

namespace vmdtest {

namespace detail {

inline std::string
CheckName(const motionVmd::Name& name)
{
    if (name.bytes.find('\0') != std::string::npos) {
        return "a name's bytes hold a NUL";
    }
    const motionVmd::Cp932Decoded decoded = motionVmd::DecodeCp932(std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(name.bytes.data()), name.bytes.size()));
    if (decoded.text != name.text) {
        return "a name's text is not its bytes decoded";
    }
    return {};
}

template <class Keys>
bool
StrictlyIncreasing(const Keys& keys)
{
    for (std::size_t i = 1; i < keys.size(); ++i) {
        if (keys[i - 1].frame >= keys[i].frame) {
            return false;
        }
    }
    return true;
}

} // namespace detail

/// Empty when `doc` keeps every promise, else the first broken one.
inline std::string
CheckInvariants(const motionVmd::Document& doc)
{
    using namespace motionVmd;
    if (doc.sectionsPresent > kSectionCount) {
        return "more sections than VMD has";
    }
    const auto absent = [&](std::size_t s, bool empty) {
        return doc.sectionsPresent <= s && !empty;
    };
    if (absent(0, doc.boneKeyframes.empty()) || absent(1, doc.morphKeyframes.empty()) ||
        absent(2, doc.cameraKeyframes.empty()) || absent(3, doc.lightKeyframes.empty()) ||
        absent(4, doc.selfShadowKeyframes.empty()) || absent(5, doc.ikKeyframes.empty())) {
        return "an absent section holds records";
    }
    std::string violation = detail::CheckName(doc.header.modelName);
    if (doc.header.modelName.bytes.size() > (doc.header.version == Version::V1 ? 10u : 20u)) {
        return "the model name is longer than its field";
    }
    for (const BoneKeyframe& k : doc.boneKeyframes) {
        if (violation.empty() && k.bone.bytes.size() > 15) {
            violation = "a bone name is longer than its field";
        }
        if (violation.empty()) {
            violation = detail::CheckName(k.bone);
        }
    }
    for (const MorphKeyframe& k : doc.morphKeyframes) {
        if (violation.empty() && k.morph.bytes.size() > 15) {
            violation = "a morph name is longer than its field";
        }
        if (violation.empty()) {
            violation = detail::CheckName(k.morph);
        }
    }
    for (const IkKeyframe& k : doc.ikKeyframes) {
        for (const IkState& s : k.ik) {
            if (violation.empty() && s.bone.bytes.size() > 20) {
                violation = "an IK name is longer than its field";
            }
            if (violation.empty()) {
                violation = detail::CheckName(s.bone);
            }
        }
    }
    return violation;
}

inline std::string
CheckInvariants(const motionVmd::Motion& motion)
{
    using detail::StrictlyIncreasing;
    for (const auto& t : motion.bones) {
        if (!StrictlyIncreasing(t.keys)) {
            return "a bone track's frames do not increase";
        }
    }
    for (const auto& t : motion.morphs) {
        if (!StrictlyIncreasing(t.keys)) {
            return "a morph track's frames do not increase";
        }
    }
    for (const auto& t : motion.ik) {
        if (!StrictlyIncreasing(t.keys)) {
            return "an IK track's frames do not increase";
        }
    }
    if (!StrictlyIncreasing(motion.visibility) || !StrictlyIncreasing(motion.camera) ||
        !StrictlyIncreasing(motion.light) || !StrictlyIncreasing(motion.selfShadow)) {
        return "a scene track's frames do not increase";
    }
    for (std::size_t i = 0; i < motion.bones.size(); ++i) {
        for (std::size_t j = i + 1; j < motion.bones.size(); ++j) {
            if (motion.bones[i].bone.bytes == motion.bones[j].bone.bytes) {
                return "two bone tracks share a name";
            }
        }
    }
    return {};
}

} // namespace vmdtest
