// SPDX-License-Identifier: Apache-2.0
#include "mmdMotionBinding/Bind.h"

#include "mmdMotionBinding/Codes.h"

#include <mmdModel/Basis.h>
#include <mmdPmx/DiagnosticList.h>
#include <motionVmd/Cp932.h>

#include <algorithm>
#include <map>
#include <utility>

namespace mmd::binding {

namespace {

/// Field bytes -> every element whose name encodes to them, lowest source
/// index first.
struct NameIndex {
    std::map<std::string, std::vector<std::pair<std::size_t, std::int32_t>>> elements;
    std::size_t unencodable = 0;
    std::string firstUnencodable;
};

template <class Element>
NameIndex
IndexNames(const std::vector<Element>& elements, std::size_t width)
{
    NameIndex index;
    for (std::size_t i = 0; i < elements.size(); ++i) {
        const Name& name = elements[i].name;
        if (name.source.empty()) {
            continue;
        }
        const auto bytes = FieldBytes(name.source, width);
        if (!bytes) {
            if (index.unencodable++ == 0) {
                index.firstUnencodable = name.source;
            }
            continue;
        }
        index.elements[*bytes].emplace_back(elements[i].sourceIndex, static_cast<std::int32_t>(i));
    }
    for (auto& [bytes, matches] : index.elements) {
        std::sort(matches.begin(), matches.end());
    }
    return index;
}

void
ReportUnencodable(DiagnosticList& diagnostics, const NameIndex& index, std::string_view table,
                  std::string_view noun)
{
    if (index.unencodable == 0) {
        return;
    }
    Location where;
    where.table = std::string(table);
    diagnostics.Add(codes::MotionUnencodableName,
                    std::to_string(index.unencodable) + " " + std::string(noun) +
                        (index.unencodable == 1 ? " name" : " names") +
                        " cannot be written in CP932, so no motion can name them (the first is '" +
                        index.firstUnencodable + "')",
                    std::move(where));
}

/// The element `name` binds to, or kNone, reporting what binding it found.
template <class Element>
std::int32_t
Match(DiagnosticList& diagnostics, const NameIndex& index, const motionVmd::Name& name,
      const std::vector<Element>& elements, std::string_view section, const Code& unmatched,
      std::string_view noun)
{
    Location where;
    where.table = std::string(section);
    const auto found = index.elements.find(name.bytes);
    if (name.bytes.empty() || found == index.elements.end()) {
        diagnostics.Add(unmatched,
                        "the model has no " + std::string(noun) + " named '" + name.text +
                            "'; its keys are dropped",
                        std::move(where));
        return kNone;
    }
    const auto& matches = found->second;
    const std::int32_t chosen = matches.front().second;
    if (matches.size() > 1) {
        diagnostics.Add(codes::MotionAmbiguousName,
                        "'" + name.text + "' names " + std::to_string(matches.size()) + " " +
                            std::string(noun) + "s once cut to the VMD field; source index " +
                            std::to_string(matches.front().first) + ", '" +
                            elements[static_cast<std::size_t>(chosen)].name.source + "', is bound",
                        std::move(where));
    }
    return chosen;
}

template <class Track>
void
SortByIndex(std::vector<Track>& tracks, std::int32_t Track::* member)
{
    std::sort(tracks.begin(), tracks.end(), [member](const Track& a, const Track& b) {
        return a.*member < b.*member;
    });
}

} // namespace

std::optional<std::string>
FieldBytes(std::string_view sourceName, std::size_t width)
{
    auto bytes = motionVmd::EncodeCp932(sourceName);
    if (!bytes) {
        return std::nullopt;
    }
    if (bytes->size() > width) {
        bytes->resize(width);
    }
    const std::size_t nul = bytes->find('\0');
    if (nul != std::string::npos) {
        bytes->resize(nul);
    }
    return bytes;
}

Result<BoundMotion>
Bind(const motionVmd::Motion& motion, const CanonicalDocument& model)
{
    BoundMotion bound;
    bound.sourceModelName = motion.header.modelName.text;
    DiagnosticList diagnostics;

    const std::vector<Bone>& bones = model.skeleton.bones;
    const NameIndex boneNames = IndexNames(bones, kBoneNameField);
    const NameIndex ikNames = IndexNames(bones, kIkNameField);
    const NameIndex morphNames = IndexNames(model.morphs, kMorphNameField);
    ReportUnencodable(diagnostics, boneNames, "bones", "bone");
    ReportUnencodable(diagnostics, morphNames, "morphs", "morph");

    const std::string_view boneSection = ToString(motionVmd::Section::Bone);
    for (const motionVmd::BoneTrack& track : motion.bones) {
        const std::int32_t joint = Match(diagnostics,
                                         boneNames,
                                         track.bone,
                                         bones,
                                         boneSection,
                                         codes::MotionUnmatchedBone,
                                         "bone");
        if (joint == kNone) {
            continue;
        }
        BoneTrack& out = bound.bones.emplace_back();
        out.joint = joint;
        out.keys.reserve(track.keys.size());
        for (const motionVmd::BoneKey& key : track.keys) {
            out.keys.push_back(BoneKey{key.frame,
                                       basis::Displacement(key.translation),
                                       basis::Quaternion(key.rotation),
                                       key.curves});
        }
    }

    const std::string_view morphSection = ToString(motionVmd::Section::Morph);
    for (const motionVmd::MorphTrack& track : motion.morphs) {
        const std::int32_t morph = Match(diagnostics,
                                         morphNames,
                                         track.morph,
                                         model.morphs,
                                         morphSection,
                                         codes::MotionUnmatchedMorph,
                                         "morph");
        if (morph != kNone) {
            bound.morphs.push_back(MorphTrack{morph, track.keys});
        }
    }

    const std::string_view ikSection = ToString(motionVmd::Section::Ik);
    for (const motionVmd::IkTrack& track : motion.ik) {
        const std::int32_t joint = Match(
            diagnostics, ikNames, track.bone, bones, ikSection, codes::MotionUnmatchedBone, "bone");
        if (joint != kNone) {
            bound.ik.push_back(IkTrack{joint, track.keys});
        }
    }
    bound.visibility = motion.visibility;

    SortByIndex(bound.bones, &BoneTrack::joint);
    SortByIndex(bound.morphs, &MorphTrack::morph);
    SortByIndex(bound.ik, &IkTrack::joint);
    return Result<BoundMotion>::Success(std::move(bound), diagnostics.Take());
}

Diagnostic
ToDiagnostic(const motionVmd::Diagnostic& d)
{
    const auto severity = [&] {
        switch (d.severity) {
        case motionVmd::Severity::Info:
            return Severity::Info;
        case motionVmd::Severity::Warning:
            return Severity::Warning;
        case motionVmd::Severity::Error:
            return Severity::Error;
        case motionVmd::Severity::Fatal:
            break;
        }
        return Severity::Fatal;
    }();
    Location where;
    where.byteOffset = d.location.byteOffset;
    where.table = d.location.section;
    where.index = d.location.index;
    where.field = d.location.field;
    return Diagnostic{d.code, severity, d.message, std::move(where), d.recoverable};
}

} // namespace mmd::binding
