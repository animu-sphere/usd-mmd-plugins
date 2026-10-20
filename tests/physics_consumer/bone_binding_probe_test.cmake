# SPDX-License-Identifier: Apache-2.0
# Check the real CLI rejects malformed metadata before any optional execution.
file(MAKE_DIRECTORY "${WORK_DIR}")
set(stage [=[#usda 1.0
(
    metersPerUnit = 1
    upAxis = "Y"
)
def Cube "Body" (
    prepend apiSchemas = ["PhysicsRigidBodyAPI", "PhysicsCollisionAPI", "PhysicsMassAPI"]
) {
    float physics:mass = 1
    custom uniform int mmd:physics:bone = 0
    custom uniform token mmd:physics:mode = "dynamicWithBone"
}
]=])
file(WRITE "${WORK_DIR}/invalid.usda" "${stage}")
string(REPLACE "mmd:physics:bone = 0" "mmd:physics:bone = -1" valid "${stage}")
file(WRITE "${WORK_DIR}/unbound.usda" "${valid}")
execute_process(COMMAND "${PROBE}" "${WORK_DIR}/invalid.usda" "${WORK_DIR}/unbound.usda"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE diagnostics)
if(NOT result EQUAL 1 OR NOT output MATCHES "rejected\tconsumerError\t/Body: MMD bone binding: mmd:physics:bone"
   OR NOT output MATCHES "unbound.usda[^\n]*accepted"
   OR NOT diagnostics MATCHES "/Body\tunboundBone")
  message(FATAL_ERROR "Binding rejection/sentinel diagnostics missing: ${result}\n${output}\n${diagnostics}")
endif()
