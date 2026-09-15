# SPDX-License-Identifier: Apache-2.0
#
# UsdMmdOpenUsd.cmake -- the workspace's OpenUSD pin, enforced in one place.
#
# OpenUSD is 26.08 and nothing else (docs/architecture/DEPENDENCIES.md §1). A
# plugin built against one OpenUSD release is not loadable in another, and
# usd-avatar-runtime composes every animu-sphere plugin into one OpenUSD
# process, so this is the release the rest of the ecosystem pins
# (usd-vrm-plugins' cmake/UsdVrmOpenUsd.cmake).
#
# Every entry point that resolves OpenUSD includes this immediately after its
# `find_package(pxr ...)`: the root project and each bundle under plugins/. A
# bundle built standalone by `ost plugin build` never composes the root
# project, so the pin travels with the find_package call, not with the root.
# The plain libraries (libs/mmdPmx) never include it: they resolve no OpenUSD
# at all (docs/architecture/WORKSPACE.md §2).
#
# WHY NOT `find_package(pxr 26.08 EXACT ...)`: OpenUSD installs no
# pxrConfigVersion.cmake, so any version argument makes find_package fail with
# "no config version file" whichever OpenUSD is present. pxrConfig.cmake does
# set the version variables directly, and those are what this tests.
#
# Unlike usd-vrm-plugins' module there is no OpenExec probe: nothing here
# evaluates anything (DEPENDENCIES.md §1, "Not used").
#
# Sets, for callers that report build metadata:
#   USDMMD_OPENUSD_RELEASE   - "26.08"
include_guard(GLOBAL)

# The single supported point. `PXR_VERSION` is OpenUSD's own packed form: 2608
# is 26.08. The display form is built from MINOR/PATCH because OpenUSD's
# PXR_MAJOR_VERSION is 0 -- "0.26.8", the value pxrConfig.cmake publishes, is
# not what anyone calls this release.
set(USDMMD_OPENUSD_REQUIRED_PXR_VERSION 2608)
set(USDMMD_OPENUSD_REQUIRED_RELEASE "26.08")

if(NOT pxr_FOUND)
    message(FATAL_ERROR
        "UsdMmdOpenUsd.cmake was included before OpenUSD was resolved. "
        "Include it after find_package(pxr REQUIRED CONFIG).")
endif()

if(NOT DEFINED PXR_VERSION)
    message(FATAL_ERROR
        "This OpenUSD install publishes no PXR_VERSION, so its version cannot "
        "be verified. usd-mmd-plugins requires OpenUSD "
        "${USDMMD_OPENUSD_REQUIRED_RELEASE} exactly.\n"
        "  pxrConfig.cmake: ${pxr_DIR}")
endif()

if(DEFINED PXR_MINOR_VERSION AND DEFINED PXR_PATCH_VERSION)
    # 26 + 8 -> "26.08"; OpenUSD zero-pads the month in every name it uses.
    string(REGEX REPLACE "^([0-9])$" "0\\1" _usdmmd_patch "${PXR_PATCH_VERSION}")
    set(USDMMD_OPENUSD_RELEASE "${PXR_MINOR_VERSION}.${_usdmmd_patch}")
    unset(_usdmmd_patch)
else()
    set(USDMMD_OPENUSD_RELEASE "${PXR_VERSION}")
endif()

if(NOT PXR_VERSION EQUAL USDMMD_OPENUSD_REQUIRED_PXR_VERSION)
    message(FATAL_ERROR
        "Unsupported OpenUSD: found ${USDMMD_OPENUSD_RELEASE} "
        "(PXR_VERSION ${PXR_VERSION}), require "
        "${USDMMD_OPENUSD_REQUIRED_RELEASE} "
        "(PXR_VERSION ${USDMMD_OPENUSD_REQUIRED_PXR_VERSION}) exactly.\n"
        "  pxrConfig.cmake: ${pxr_DIR}\n"
        "OpenUSD guarantees no ABI stability across releases, so a plugin "
        "built against another release could not be loaded beside the rest "
        "of the ecosystem. See docs/architecture/DEPENDENCIES.md.")
endif()

# include_guard(GLOBAL) makes this the only time the line is printed per
# configure, however many members include the module.
message(STATUS
    "OpenUSD ${USDMMD_OPENUSD_RELEASE} (PXR_VERSION ${PXR_VERSION})")
