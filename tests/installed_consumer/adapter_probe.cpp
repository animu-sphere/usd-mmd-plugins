// SPDX-License-Identifier: Apache-2.0

#include <mmdControl/Evaluator.h>
#include <mmdModel/Canonicalize.h>
#include <mmdMotionAdapter/Adapter.h>
#include <mmdMotionBinding/Bind.h>
#include <mmdPmx/Reader.h>
#include <mmdSkeletonAdapter/Adapter.h>
#include <motionVmd/Motion.h>
#include <motionVmd/Reader.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>

int
main(int argc, char** argv)
{
    if (argc != 3) {
        return 2;
    }
    const auto vmd = motionVmd::ReadFile(std::filesystem::path(argv[1]));
    const auto pmx = mmd::pmx::ReadFile(std::filesystem::path(argv[2]));
    if (!vmd || !pmx) {
        return 2;
    }
    const mmd::CanonicalDocument model = mmd::Canonicalize(pmx.value()).value();
    const auto bound =
        mmd::binding::Bind(motionVmd::BuildMotion(vmd.value()).value(), model).value();
    const auto evaluator = mmd::control::Evaluator::Prepare(model).value();
    const auto skeleton = mmd::skeleton::Adapt(model);

    std::uint32_t last = 0;
    for (const auto& track : bound.bones) {
        if (!track.keys.empty()) {
            last = std::max(last, track.keys.back().frame);
        }
    }
    const auto clip = mmd::motion::BuildClip(
        model,
        bound,
        evaluator,
        skeleton,
        mmd::motion::ClipOptions{0.0, static_cast<double>(last) / 30.0, 30.0});
    if (!clip) {
        return 2;
    }
    std::printf("samples=%zu joints=%zu roleTable=%d\n",
                clip.value().samples.size(),
                skeleton.skeleton.GetSize(),
                skeleton.roleTableVersion);
    return 0;
}
