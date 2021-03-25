cmake_minimum_required(VERSION 3.12)

project(avatar-asset-pipeline)

include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )

file(GLOB pipeline_FILES components/*.hpp)

set(SRC_FILES
  src/main.cpp
  include/json_func.inl
  include/gltf_func.inl
  include/gltf_overrides_func.inl
  include/bones_func.inl
  include/vrm0_func.inl
  ${pipeline_FILES}
)

set (BUILD_INCLUDES
  ${CMAKE_CURRENT_SOURCE_DIR}/components
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/extern/cgltf
  ${CMAKE_CURRENT_SOURCE_DIR}/extern/glm
  ${CMAKE_CURRENT_SOURCE_DIR}/extern/ghc
  ${CMAKE_CURRENT_SOURCE_DIR}/extern/dspatch/include
  ${CMAKE_CURRENT_SOURCE_DIR}/extern/meshoptimizer/src
  ${CMAKE_CURRENT_SOURCE_DIR}/extern/meshoptimizer/gltf
)

file(GLOB dspatch_root_INCLUDES extern/dspatch/include/*.h)
file(GLOB dspatch_details_INCLUDES extern/dspatch/include/dspatch/*.h)
file(GLOB dspatch_internal_INCLUDES extern/dspatch/src/internal/*.h)
file(GLOB dspatch_srcs extern/dspatch/src/*.cpp extern/dspatch/src/internal/*.cpp)

add_library(dspatch STATIC
  ${dspatch_root_INCLUDES}
  ${dspatch_details_INCLUDES}
  ${dspatch_internal_INCLUDES}
  ${dspatch_srcs}
)

set (dspatch_INCLUDES
  ${CMAKE_CURRENT_SOURCE_DIR}/extern/dspatch/include
  ${CMAKE_CURRENT_SOURCE_DIR}/extern/dspatch/src
)

target_include_directories(dspatch PRIVATE ${dspatch_INCLUDES})

set(REPROC++ ON)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/extern/reproc)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/extern/meshoptimizer)

set( EXE_NAME avatar-build )
add_executable( ${EXE_NAME} ${SRC_FILES} )
set_property( TARGET ${EXE_NAME} PROPERTY CXX_STANDARD 11 )

target_include_directories(${EXE_NAME} PRIVATE ${BUILD_INCLUDES})
target_link_libraries(${EXE_NAME} PRIVATE dspatch reproc++ meshoptimizer gltfpack)

if(MSVC)
    target_compile_options(dspatch PRIVATE /W4 /WX /wd4267)
    target_compile_options(reproc PRIVATE /W4 /WX /wd4996)
    target_compile_options(${EXE_NAME} PRIVATE /W4 /WX)
    add_definitions( -D_CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(${EXE_NAME} PRIVATE -Wall -Wextra -pedantic -Werror)
endif()
install( TARGETS ${EXE_NAME} RUNTIME DESTINATION bin )

