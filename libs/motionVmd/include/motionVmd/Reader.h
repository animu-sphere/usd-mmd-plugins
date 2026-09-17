// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "motionVmd/Document.h"
#include "motionVmd/Result.h"

#include <cstddef>
#include <filesystem>
#include <span>

namespace motionVmd {

/// Reads a VMD file held in memory: the header and every section present, in
/// file order. Every read is bounded by `bytes`, explicitly little-endian, and
/// independent of the host's locale (MOTION_CONTRACT.md §3, under
/// PMX_CONTRACT.md §2's reading rules). Parsing never needs a model.
///
/// Fatal: an unknown signature, a section cut short, or a count the rest of
/// the file cannot hold. A file that ends between two sections is complete.
/// Recoverable: a name field CP932 cannot decode, a name cut inside a
/// character, and bytes after the last section.
Result<Document> Read(std::span<const std::byte> bytes);

/// Reads the file at `path` and parses it with Read. A file that cannot be
/// opened or read in full is MMD_MOTION_FILE_UNREADABLE. The path is never a
/// narrow string, for the reason mmd::pmx::ReadFile gives.
Result<Document> ReadFile(const std::filesystem::path& path);

} // namespace motionVmd
