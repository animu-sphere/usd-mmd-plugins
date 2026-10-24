// SPDX-License-Identifier: Apache-2.0
//
// Binds one VMD to one PMX and evaluates it through the installed mmdControl,
// at every frame from 0 to the last key, printing what it found:
//   joints=5 frames=11 finite=1   the pose's joint count, the frames
//                                  evaluated, and whether every value was finite
// Exit status: 0 evaluated, 2 usage or I/O error.
#include <mmdControl/Evaluator.h>
#include <mmdModel/Canonicalize.h>
#include <mmdMotionBinding/Bind.h>
#include <mmdPmx/Reader.h>
#include <motionVmd/Motion.h>
#include <motionVmd/Reader.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

int
main(int argc, char** argv)
{
    if (argc != 3) {
        std::fprintf(stderr, "usage: control_probe <file.vmd> <model.pmx>\n");
        return 2;
    }
    // Exercise the new owner API through installed headers AND static library.
    mmd::CanonicalDocument groupModel;
    groupModel.morphs.resize(3);
    groupModel.morphs[0].type = mmd::MorphType::Group;
    groupModel.morphs[0].members = {{1, 0.5f}, {2, 1.0f}};
    groupModel.morphs[1].type = mmd::MorphType::Vertex;
    groupModel.morphs[2].type = mmd::MorphType::Bone;
    const auto groups = mmd::control::Evaluator::Prepare(groupModel).value();
    const std::vector<mmd::control::MorphChannel> raw{{0, 2}, {1, 0.25}};
    const std::vector<mmd::control::MorphChannel> expected{{1, 1.25}};
    if (groups.ResolveMorphChannels(raw) != expected) {
        std::fprintf(stderr, "installed non-bone group resolution differs\n");
        return 2;
    }
    const auto read = motionVmd::ReadFile(std::filesystem::path(argv[1]));
    const auto source = mmd::pmx::ReadFile(std::filesystem::path(argv[2]));
    if (!read.ok() || !source.ok()) {
        std::fprintf(stderr, "cannot read the motion or the model\n");
        return 2;
    }
    const mmd::CanonicalDocument model = mmd::Canonicalize(source.value()).value();
    const auto bound =
        mmd::binding::Bind(motionVmd::BuildMotion(read.value()).value(), model).value();
    const mmd::control::Evaluator evaluator = mmd::control::Evaluator::Prepare(model).value();

    std::uint32_t last = 0;
    for (const auto& track : bound.bones) {
        last = std::max(last, track.keys.back().frame);
    }
    bool finite = true;
    mmd::control::Pose pose;
    for (std::uint32_t frame = 0; frame <= last; ++frame) {
        evaluator.Evaluate(bound, frame, pose);
        const auto suspended = evaluator.BeginPhysics(bound, frame);
        const auto resumed = evaluator.ResumePhysics(suspended);
        if (!resumed.ok() || resumed.value() != pose) {
            std::fprintf(stderr, "split evaluation differs from offline evaluation\n");
            return 2;
        }
        for (const auto& world : evaluator.World(pose)) {
            for (const double v : world.translation) {
                finite = finite && std::isfinite(v);
            }
        }
    }
    std::printf("joints=%zu frames=%u finite=%d\n", pose.joints.size(), last + 1, finite ? 1 : 0);
    return 0;
}
