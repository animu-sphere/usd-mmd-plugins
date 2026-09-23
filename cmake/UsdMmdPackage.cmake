# SPDX-License-Identifier: Apache-2.0
#
# UsdMmdPackage.cmake -- how a plain library of this workspace installs as a
# CMake package (docs/architecture/PACKAGE_CONTRACT.md). One layout, one
# version policy and one export for every library, so no package can differ
# from the contract by omission.
#
# Plugin bundles install differently -- plugInfo.json, buildInfo.json, the
# bundle's lib/ beside plugin/resources/ -- and never through this helper.
include_guard(GLOBAL)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# usdmmd_install_library(<target>)
#
# Installs <target> as the package of the same name, with the imported target
# <target>::<target>:
#
#   ${CMAKE_INSTALL_LIBDIR}/                 the static archive
#   ${CMAKE_INSTALL_INCLUDEDIR}/             the component's include/ tree
#   ${CMAKE_INSTALL_LIBDIR}/cmake/<target>/  <target>Config.cmake,
#                                            <target>ConfigVersion.cmake,
#                                            <target>Targets.cmake
#
# The package config is generated from the component's own
# cmake/<target>Config.cmake.in. That template is where the package declares
# the packages its public link interface needs -- each behind a target check,
# with find_dependency() -- so a consumer never lists a transitive dependency
# itself (PACKAGE_CONTRACT.md).
#
# Pre-1.0, a minor version may break the API, so a consumer's request is
# compatible only within the same major.minor: SameMinorVersion.
function(usdmmd_install_library target)
    set(_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/${target}")
    set(_config "${CMAKE_CURRENT_BINARY_DIR}/${target}Config.cmake")
    set(_version "${CMAKE_CURRENT_BINARY_DIR}/${target}ConfigVersion.cmake")

    install(TARGETS ${target}
        EXPORT ${target}Targets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${target}Config.cmake.in"
        "${_config}"
        INSTALL_DESTINATION "${_package_dir}")
    write_basic_package_version_file("${_version}"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMinorVersion)

    install(EXPORT ${target}Targets
        FILE ${target}Targets.cmake
        NAMESPACE ${target}::
        DESTINATION "${_package_dir}")
    install(FILES "${_config}" "${_version}" DESTINATION "${_package_dir}")
endfunction()
