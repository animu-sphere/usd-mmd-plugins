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
if(NOT result EQUAL 0 OR NOT output MATCHES "jointFidelity=approximate"
   OR NOT output MATCHES "translationSpringModel=experimentalHookean	approximateTranslationSpringAxes=2"
   OR NOT diagnostics MATCHES "/Joint	approximateTranslationSprings	axes=2")
  message(FATAL_ERROR "Experimental spring execution missing: ${result}\n${output}\n${diagnostics}")
endif()
string(REPLACE "= \"spring6Dof\"" "= \"sixDof\"" plain "${stage}")
file(WRITE "${WORK_DIR}/plain.usda" "${plain}")
execute_process(COMMAND "${PROBE}" "${WORK_DIR}/plain.usda"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics)
if(NOT result EQUAL 0 OR NOT output MATCHES "approximateTranslationSpringAxes=0"
   OR diagnostics MATCHES "approximateTranslationSprings")
  message(FATAL_ERROR "Non-spring source joint activated springs: ${result}\n${output}\n${diagnostics}")
endif()
