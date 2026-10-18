# SPDX-License-Identifier: Apache-2.0
# Exercise the actual probe: snapshot acceptance must expose omitted springs.
file(MAKE_DIRECTORY "${WORK_DIR}")
set(stage [=[#usda 1.0
(
    metersPerUnit = 1
    upAxis = "Y"
)
def Cube "Body0" (
    prepend apiSchemas = ["PhysicsRigidBodyAPI", "PhysicsCollisionAPI", "PhysicsMassAPI"]
) {
    float physics:mass = 1
}
def Cube "Body1" (
    prepend apiSchemas = ["PhysicsRigidBodyAPI", "PhysicsCollisionAPI", "PhysicsMassAPI"]
) {
    float physics:mass = 1
}
def PhysicsJoint "Joint" {
    rel physics:body0 = </Body0>
    rel physics:body1 = </Body1>
    custom token mmd:physics:type = "spring6Dof"
    custom vector3f mmd:physics:translationSpring = (0, 10, 20)
    custom vector3f mmd:physics:rotationSpring = (5, 6, 7)
}
]=])
file(WRITE "${WORK_DIR}/springs.usda" "${stage}")
execute_process(COMMAND "${PROBE}" "${WORK_DIR}/springs.usda"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics)
if(NOT result EQUAL 0 OR NOT output MATCHES "unsupportedSpringJoints=1\tunsupportedSpringAxes=5\tjointFidelity=approximate"
   OR NOT diagnostics MATCHES "/Joint\tunsupportedJointSprings\ttranslation=\\(0, 10, 20\\)\trotation=\\(5, 6, 7\\)")
  message(FATAL_ERROR "Spring fidelity gap missing: ${result}\n${output}\n${diagnostics}")
endif()

string(REPLACE "(0, 10, 20)" "(0, 0, 0)" zero "${stage}")
string(REPLACE "(5, 6, 7)" "(0, 0, 0)" zero "${zero}")
file(WRITE "${WORK_DIR}/zero.usda" "${zero}")
string(REPLACE "vector3f mmd:physics:translationSpring" "vector3d mmd:physics:translationSpring" invalid "${stage}")
file(WRITE "${WORK_DIR}/invalid.usda" "${invalid}")
execute_process(COMMAND "${PROBE}" "${WORK_DIR}/invalid.usda" "${WORK_DIR}/zero.usda"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics)
if(NOT result EQUAL 1 OR NOT output MATCHES "rejected\tconsumerError\t/Joint: MMD joint springs"
   OR NOT output MATCHES "zero.usda[^\n]*accepted[^\n]*unsupportedSpringJoints=0\tunsupportedSpringAxes=0\tjointFidelity=unverified"
   OR diagnostics MATCHES "unsupportedJointSprings")
  message(FATAL_ERROR "Invalid metadata or subsequent zero control mishandled: ${result}\n${output}\n${diagnostics}")
endif()

# A format-only joint remains a fidelity gap even without nonzero springs.
file(WRITE "${WORK_DIR}/source-only.usda" "${zero}\ndef Xform \"SourceOnly\" {\n    custom token mmd:physics:type = \"hinge\"\n}\n")
execute_process(COMMAND "${PROBE}" "${WORK_DIR}/source-only.usda"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics)
if(NOT result EQUAL 0 OR NOT output MATCHES "preservedOnlyJoints=1"
   OR NOT output MATCHES "unsupportedSpringAxes=0\tjointFidelity=approximate")
  message(FATAL_ERROR "Format-only joint gap missing: ${result}\n${output}\n${diagnostics}")
endif()
