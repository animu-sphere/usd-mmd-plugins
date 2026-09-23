# OST dogfooding reports

This repository is built with [OpenStrata](https://github.com/animu-sphere/open-strata)
(`ost`), and these are the dated records of what that was like, measured in
this repository's own tree. They are upstream feedback first and our own
status trail second, and follow the series `usd-vrm-plugins` keeps
([its reports](https://github.com/animu-sphere/usd-vrm-plugins/tree/main/docs/reports/ost)):
the ecosystem's running ask list is there, and a report here records what only
this repository measured.

**They are append-only historical evidence.** A report is never rewritten to
match what later turned out to be true. When a newer `ost` resolves an item, a
new report re-verifies it and the superseded report gets a one-line
forward-note at the top.

## Reading order

The newest report carries the current asks.

| Report | `ost` | Subject |
| --- | --- | --- |
| [01](01-2026-09-24-v0.23.3-a-bundle-is-staged-in-its-source-tree.md) | 0.23.3 | The CMake dependency contract needs nothing from `ost`; a bundle's library and registration, and a tool's directories, can only be staged in the source tree |
