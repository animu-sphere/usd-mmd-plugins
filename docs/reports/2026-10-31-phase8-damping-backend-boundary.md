# Damping backend boundary (2026-10-31)

The [owner source-policy resolver](2026-10-27-phase8-source-damping-response.md)
already returns either finite decay or `ClearPriorVelocity`, with an explicit
normalization indication. Its neutral backend prerequisite now has a locally
verified implementation in `usd-physics-plugins`.

The owning [backend report](https://github.com/animu-sphere/usd-physics-plugins/blob/366bb106069c1d856099ae09fdd6f36655f62860/docs/reports/2026-10-31-prior-velocity-clearing.md)
records the exact base revision, Windows environment, commands and generated
response measurements. Its neutral descriptor selects clearing independently
for linear/angular velocity before force/gravity integration and constraint
solving. New force, contact and constraint responses remain active. The evidence
includes finite-rate coexistence, invalid-step nonmutation, body/world rebuild
replay and installed Jolt/no-Jolt package consumers. This is a generic backend
result; no MMD source data enters that repository.

The backend's descriptor layout changed. The owning
[package contract](https://github.com/animu-sphere/usd-physics-plugins/blob/366bb106069c1d856099ae09fdd6f36655f62860/docs/architecture/PACKAGE_CONTRACT.md#8-static-package-compatibility)
defines feature discovery and requires rebuilding core, backend, bridge and
consumer together. Existing product versions alone do not authorize mixing
old and new packages. No published artifact or dependency pin is claimed.

## Remaining consumer boundary

This checkpoint changes documentation here, not the importer, strict
`ReadBodyProperties`, `ResolveSourceDamping` or the live host. The live reader
still refuses the unit endpoint and out-of-domain declarations. Next work is:

1. Explicitly select the owner source policy in the runtime and map its typed
   responses to the neutral descriptor using rebuilt installed packages.
2. Preserve source body paths and original declarations in normalization
   diagnostics; malformed USD declarations must still refuse.
3. Re-admit the declarations on Reset and verify composed force/constraint
   response, independent channels and reset replay through the live host.
4. Rerun private PMX/VMD intake without the temporary 0.99 damping layer.

Those are runtime/acceptance tasks, not additional coefficient interpretation
inside generic physics. The backend tests do not close the roadmap's damping
integration, representative L3 or final interactive visual gates.
