// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <mmdPmx/Result.h>
#include <pxr/usd/usd/prim.h>
#include <cstdint>
#include <optional>

namespace mmd::physics {

/// Owned MMD conversions, without a dependency on a solver or world package.
/// Missing pairs leave the caller's conventional USD body defaults untouched.
struct BodyProperties {
    struct Filter {
        uint32_t category, mask;
    };
    struct Contact {
        double friction, restitution;
    };
    struct Damping {
        double linear, angular;
    }; // inverse seconds
    std::optional<Filter> filter;
    std::optional<Contact> contact;
    std::optional<Damping> damping;
};

/// Read default-time MMD filter/contact/damping pairs without editing the stage.
/// Each pair is either absent or complete, uniform, unsampled and correctly typed.
/// Filter bits admit groups. Damping fractions convert with -log1p(-d); d=1
/// refuses because instantaneous stopping cannot be expressed by finite rates.
/// The host selects the world-wide product contact rule with friction capped
/// at 10 (PHYSICS_INTEGRATION section 7.1), and applies the returned values to
/// its own descriptors. Backend finite-step behavior remains an approximation.
Result<BodyProperties> ReadBodyProperties(const pxr::UsdPrim& body);

} // namespace mmd::physics
