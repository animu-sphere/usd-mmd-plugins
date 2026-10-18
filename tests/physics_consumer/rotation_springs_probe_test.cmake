# SPDX-License-Identifier: Apache-2.0
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
def Cube "Body" (
    prepend apiSchemas = ["PhysicsRigidBodyAPI", "PhysicsCollisionAPI", "PhysicsMassAPI"]
) {
    float physics:mass = 1
}
def PhysicsJoint "Joint" {
    rel physics:body0 = </Body0>
    rel physics:body1 = </Body>
    custom token mmd:physics:type = "spring6Dof"
    custom vector3f mmd:physics:translationSpring = (1, 0, 0)
    custom vector3f mmd:physics:rotationSpring = (0, 25, -10)
}
]=])
file(WRITE "${WORK_DIR}/spring.usda" "${stage}")
execute_process(COMMAND "${PROBE}" "${WORK_DIR}/spring.usda"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics)
if(NOT result EQUAL 0 OR NOT output MATCHES "approximateRotationSpringAxes=1"
   OR NOT output MATCHES "rotationSpringModel=experimentalSmallAngle"
   OR NOT output MATCHES "unsupportedSpringAxes=3\tjointFidelity=approximate"
   OR NOT diagnostics MATCHES "/Joint\tapproximateRotationSprings\taxes=1")
  message(FATAL_ERROR "Rotation approximation missing: ${result}\n${output}\n${diagnostics}")
endif()
string(REPLACE "\"spring6Dof\"" "\"sixDof\"" inactive "${stage}")
file(WRITE "${WORK_DIR}/inactive.usda" "${inactive}")
execute_process(COMMAND "${PROBE}" "${WORK_DIR}/inactive.usda"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics)
if(NOT result EQUAL 0 OR NOT output MATCHES "approximateRotationSpringAxes=0"
   OR diagnostics MATCHES "approximateRotationSprings")
  message(FATAL_ERROR "Inactive source joint gained rotation springs: ${result}\n${output}\n${diagnostics}")
endif()
