cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(tymp_ml_classifier_eval)


set(AVAILABLE_MODELS "v1" "v2" "v3" "v4" "v5")

if (NOT DEFINED MODEL)
    message(FATAL_ERROR "MODEL variable is not defined. Please specify a model using -DMODEL=<model_name>.")

endif()

message(STATUS "Building with Model: ${MODEL_VERSION}")

set(MODEL_DIR "models/neuton_model_${MODEL_VERSION}")

if(NOT EXISTS "${MODEL_DIR}/nrf_edgeai_user_model.c")
  message(FATAL_ERROR "Model source not found for MODEL_VERSION='${MODEL_VERSION}' (looked in ${MODEL_DIR})")
endif()

target_sources(app PRIVATE
  src/main.c
  "${MODEL_DIR}/nrf_edgeai_user_model.c"
)

target_include_directories(app PRIVATE
  ${MODEL_DIR}
)

target_compile_definitions(app PRIVATE
  MODEL_VERSION_STR="${MODEL_VERSION}"
)
