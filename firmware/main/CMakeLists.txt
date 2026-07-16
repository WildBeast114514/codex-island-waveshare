file(GLOB_RECURSE APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

idf_component_register(
    SRCS ${APP_SOURCES}
    INCLUDE_DIRS "."
    REQUIRES bt json nvs_flash esp_timer esp_driver_gpio esp_driver_i2c qmi8658)

set(PET_ASSET "${CMAKE_SOURCE_DIR}/generated/pet_asset.bin")
if(NOT EXISTS "${PET_ASSET}")
    message(FATAL_ERROR
        "Pet asset is missing. Run scripts/build_firmware.sh so the configured "
        "WebP manifest is converted before ESP-IDF configures the project.")
endif()
target_add_binary_data(${COMPONENT_LIB} "${PET_ASSET}" BINARY
    RENAME_TO codex_pet_asset)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD_REQUIRED ON)
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall
    -Wextra
    -Werror=return-type
    -Wno-error=cpp)
