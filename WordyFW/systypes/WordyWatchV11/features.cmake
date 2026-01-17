# Include common features
include("${BUILD_CONFIG_DIR}/../Common/features.cmake")

add_compile_definitions(FEATURE_POWER_CONTROL_LOW_BATTERY_SHUTDOWN)

set(DEV_TYPE_JSON_FILES "${BUILD_CONFIG_DIR}/DevTypes.json")

