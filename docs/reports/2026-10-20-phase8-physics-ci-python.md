# Phase 8 hosted Python SDK overrides (2026-10-20)

## Failure and correction

The [first hosted run](https://github.com/animu-sphere/usd-mmd-plugins/actions/runs/39278795693)
of the installed physics lane at MMD `857b290` reached shared-package
configuration after building MMD and installing Jolt. OpenUSD's relocated
`pxrConfig.cmake` retained the SDK builder's Python development paths.
`find_dependency(Python3)` therefore attempted to use an import library that
does not exist on the hosted runner, despite finding Python 3.13.16.
The representative consumer tests were not reached. This is separate from
the delayed GitHub queue/status reporting observed during that run.

[run_physics_ci.py](../../scripts/run_physics_ci.py) now derives the selected
host interpreter's executable, headers and Windows import library once.
Both the workflow's shared-package configure and all three consumer
configurations pass these explicit CMake inputs, overriding the SDK's
build-machine defaults. Missing host development files fail early; their
resolved paths are included in the local configuration evidence.

## Regression and validation

[test_run_physics_ci.py](../../scripts/test_run_physics_ci.py) reproduces the
relocation problem through a CMake package-discovery smoke: foreign default
header/library paths reject executable-only configuration, while supplying
the host SDK resolves Python's interpreter and development targets.
The Windows workflow runs this control before provisioning OpenUSD.

The [original local evidence](2026-10-20-phase8-physics-ci.md) retains its
original scope. Local follow-up commands use the same installed physics,
OpenUSD and Jolt prefixes and a new output directory:

```powershell
python -m unittest discover -s scripts -p test_run_physics_ci.py -v
python scripts/run_physics_ci.py --mmd-build build/windows-msvc `
  --physics-prefix build/physics-ci-shared-prefix --usd-root $env:USD_ROOT `
  --jolt-root $env:JOLT_ROOT --build-root build/physics-ci-python-fixed
```

On Windows x86_64 / MSVC 19.51 / Python 3.13.14, the regression passes and
all installed-physics CTests pass again: reader 7/7, execution 7/7 and
experimental springs 11/11. Workflow PowerShell syntax and documentation
checks (including the docs checker selftest) also pass.

Hosted verification and the P0-2 gate are tracked in
[current.md](../roadmap/current.md); no avatar L3 claim changes.
