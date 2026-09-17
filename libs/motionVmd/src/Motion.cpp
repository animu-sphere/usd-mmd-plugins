// SPDX-License-Identifier: Apache-2.0
#include "motionVmd/Motion.h"

#include "motionVmd/Codes.h"

#include "DiagnosticList.h"

#include <algorithm>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace motionVmd {

namespace {

/// Sorts `keys` by frame, keeping the last of each frame in the order they
/// were appended -- file order -- and returns how many were dropped.
template <class Key>
std::size_t
SortAndDeduplicate(std::vector<Key>& keys)
{
    std::stable_sort(
        keys.begin(), keys.end(), [](const Key& a, const Key& b) { return a.frame < b.frame; });
    std::size_t kept = 0;
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (i + 1 < keys.size() && keys[i + 1].frame == keys[i].frame) {
            continue; // a later record for the same frame wins
        }
        if (kept != i) {
            keys[kept] = std::move(keys[i]);
        }
        ++kept;
    }
    const std::size_t dropped = keys.size() - kept;
    keys.resize(kept);
    return dropped;
}

void
ReportDuplicates(detail::DiagnosticList& diagnostics, std::size_t dropped, std::string_view section,
                 std::string_view track)
{
    if (dropped == 0) {
        return;
    }
    Location where;
    where.section = std::string(section);
    diagnostics.Add(codes::MotionDuplicateKeyframe,
                    std::string(track) + ": " + std::to_string(dropped) +
                        (dropped == 1 ? " keyframe shares" : " keyframes share") +
                        " a frame with a later one, which is kept",
                    std::move(where));
}

std::string
TrackName(const Name& name)
{
    return "'" + name.text + "'";
}

/// The track for `name`, created -- after every existing one -- on first use.
template <class Track>
Track&
TrackFor(std::vector<Track>& tracks, std::map<std::string, std::size_t>& byBytes, const Name& name,
         Name Track::* member)
{
    const auto [it, inserted] = byBytes.try_emplace(name.bytes, tracks.size());
    if (inserted) {
        Track& track = tracks.emplace_back();
        track.*member = name;
        return track;
    }
    return tracks[it->second];
}

} // namespace

std::array<Bezier, 4>
BoneCurves(const std::array<std::uint8_t, 64>& b)
{
    std::array<Bezier, 4> out;
    for (std::size_t c = 0; c < 4; ++c) {
        out[c] = Bezier{b[c], b[4 + c], b[8 + c], b[12 + c]};
    }
    // Row 1 is row 0 shifted by one byte, and holds these two where row 0's
    // may be a physics toggle instead.
    out[kZ].x1 = b[16 + 1];
    out[kRotation].x1 = b[16 + 2];
    return out;
}

std::array<Bezier, 6>
CameraCurves(const std::array<std::uint8_t, 24>& b)
{
    std::array<Bezier, 6> out;
    for (std::size_t c = 0; c < 6; ++c) {
        out[c] = Bezier{b[4 * c], b[4 * c + 2], b[4 * c + 1], b[4 * c + 3]};
    }
    return out;
}

Result<Motion>
BuildMotion(const Document& document)
{
    Motion motion;
    motion.header = document.header;
    detail::DiagnosticList diagnostics;

    std::map<std::string, std::size_t> boneTracks;
    for (const BoneKeyframe& k : document.boneKeyframes) {
        TrackFor(motion.bones, boneTracks, k.bone, &BoneTrack::bone)
            .keys.push_back(
                BoneKey{k.frame, k.translation, k.rotation, BoneCurves(k.interpolation)});
    }
    std::map<std::string, std::size_t> morphTracks;
    for (const MorphKeyframe& k : document.morphKeyframes) {
        TrackFor(motion.morphs, morphTracks, k.morph, &MorphTrack::morph)
            .keys.push_back(MorphKey{k.frame, k.weight});
    }
    for (const CameraKeyframe& k : document.cameraKeyframes) {
        motion.camera.push_back(CameraKey{k.frame,
                                          k.distance,
                                          k.position,
                                          k.rotation,
                                          CameraCurves(k.interpolation),
                                          k.viewAngle,
                                          k.orthographic != 0});
    }
    for (const LightKeyframe& k : document.lightKeyframes) {
        motion.light.push_back(LightKey{k.frame, k.color, k.direction});
    }
    for (const SelfShadowKeyframe& k : document.selfShadowKeyframes) {
        motion.selfShadow.push_back(SelfShadowKey{k.frame, k.mode, k.distance});
    }
    std::map<std::string, std::size_t> ikTracks;
    for (const IkKeyframe& k : document.ikKeyframes) {
        motion.visibility.push_back(VisibilityKey{k.frame, k.visible != 0});
        for (const IkState& state : k.ik) {
            TrackFor(motion.ik, ikTracks, state.bone, &IkTrack::bone)
                .keys.push_back(IkKey{k.frame, state.enabled != 0});
        }
    }

    const std::string_view bones = ToString(Section::Bone);
    for (BoneTrack& track : motion.bones) {
        ReportDuplicates(diagnostics, SortAndDeduplicate(track.keys), bones, TrackName(track.bone));
    }
    const std::string_view morphs = ToString(Section::Morph);
    for (MorphTrack& track : motion.morphs) {
        ReportDuplicates(
            diagnostics, SortAndDeduplicate(track.keys), morphs, TrackName(track.morph));
    }
    ReportDuplicates(
        diagnostics, SortAndDeduplicate(motion.camera), ToString(Section::Camera), "camera");
    ReportDuplicates(
        diagnostics, SortAndDeduplicate(motion.light), ToString(Section::Light), "light");
    ReportDuplicates(diagnostics,
                     SortAndDeduplicate(motion.selfShadow),
                     ToString(Section::SelfShadow),
                     "self-shadow");
    const std::string_view ik = ToString(Section::Ik);
    ReportDuplicates(diagnostics, SortAndDeduplicate(motion.visibility), ik, "visibility");
    for (IkTrack& track : motion.ik) {
        ReportDuplicates(diagnostics, SortAndDeduplicate(track.keys), ik, TrackName(track.bone));
    }

    return Result<Motion>::Success(std::move(motion), diagnostics.Take());
}

} // namespace motionVmd
