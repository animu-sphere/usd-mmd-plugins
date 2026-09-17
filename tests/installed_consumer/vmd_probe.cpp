// SPDX-License-Identifier: Apache-2.0
//
// Reads one VMD through the installed motionVmd and, given a PMX, binds it to
// that model through the installed mmdMotionBinding, printing what it found:
//   tracks=3 2 2                bone, morph and IK tracks
//   bound=2 1 1                 the tracks that bound to the model, if given
//   fatal=MMD_MOTION_...        when the file is refused
// Exit status: 0 read, 1 refused, 2 usage or I/O error.
#include <mmdModel/Canonicalize.h>
#include <mmdMotionBinding/Bind.h>
#include <mmdPmx/Reader.h>
#include <motionVmd/Motion.h>
#include <motionVmd/Reader.h>

#include <cstdio>
#include <filesystem>

int
main(int argc, char** argv)
{
    if (argc != 2 && argc != 3) {
        std::fprintf(stderr, "usage: vmd_probe <file.vmd> [model.pmx]\n");
        return 2;
    }
    const auto read = motionVmd::ReadFile(std::filesystem::path(argv[1]));
    if (!read.ok()) {
        std::printf("fatal=%s\n", read.fatal()->code.c_str());
        return 1;
    }
    const motionVmd::Motion motion = motionVmd::BuildMotion(read.value()).value();
    std::printf(
        "tracks=%zu %zu %zu\n", motion.bones.size(), motion.morphs.size(), motion.ik.size());
    if (argc == 3) {
        const auto model = mmd::pmx::ReadFile(std::filesystem::path(argv[2]));
        if (!model.ok()) {
            std::fprintf(stderr, "cannot read %s\n", argv[2]);
            return 2;
        }
        const auto bound =
            mmd::binding::Bind(motion, mmd::Canonicalize(model.value()).value()).value();
        std::printf(
            "bound=%zu %zu %zu\n", bound.bones.size(), bound.morphs.size(), bound.ik.size());
    }
    return 0;
}
