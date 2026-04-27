cmake_minimum_required(VERSION 3.10)
project(RootTreeAnalyzer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------
# 1) ROOT'u bul
# ------------------------------------------------------------
find_package(ROOT REQUIRED)
include(${ROOT_USE_FILE})

# ------------------------------------------------------------
# 2) shared/ altındaki kaynaklardan kütüphane oluştur (dictionary YOK)
# ------------------------------------------------------------
set(SHARED_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/shared/include")
set(SHARED_SRC_DIR     "${CMAKE_SOURCE_DIR}/shared/src")
set(INIREADER_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/iniReader")

file(GLOB_RECURSE SHARED_SOURCES "${SHARED_SRC_DIR}/*.cc")

add_library(BeamClusterTreeReader SHARED
  ${SHARED_SOURCES}
)

target_include_directories(BeamClusterTreeReader
  PUBLIC
    ${SHARED_INCLUDE_DIR}
)

target_link_libraries(BeamClusterTreeReader
  PUBLIC
    ${ROOT_LIBRARIES}
)

# ------------------------------------------------------------
# Helper function for building tools
# ------------------------------------------------------------
function(addTool toolName)

    add_executable(${toolName} main.cpp)

    target_include_directories(${toolName} PRIVATE
        ${INIREADER_INCLUDE_DIR}
    )

    target_link_libraries(${toolName} PRIVATE
        BeamClusterTreeReader
        ${ROOT_LIBRARIES}
    )

    set_target_properties(${toolName} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    # Config dosyalarını kopyala
    set(CONFIG_OUTPUT_DIR ${CMAKE_BINARY_DIR}/configs/${toolName})
    file(MAKE_DIRECTORY ${CONFIG_OUTPUT_DIR})

    file(GLOB CONFIG_FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/*.ini"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.mac"
    )

    foreach(cfg_file ${CONFIG_FILES})
        configure_file(
            ${cfg_file}
            ${CONFIG_OUTPUT_DIR}/
            COPYONLY
        )
    endforeach()

endfunction()

# ------------------------------------------------------------
# 3) To compile tools
# ------------------------------------------------------------
set(TOOLS "" CACHE STRING
    "List of tools to build (semicolon-separated). Empty = all of them.")

file(GLOB TOOL_DIRS RELATIVE ${CMAKE_SOURCE_DIR} "${CMAKE_SOURCE_DIR}/tools/*")
string(REPLACE ";" ";" SELECTED_TOOLS "${TOOLS}")

foreach(tool_dir IN LISTS TOOL_DIRS)
  if(IS_DIRECTORY "${CMAKE_SOURCE_DIR}/${tool_dir}")
    if(TOOLS STREQUAL "")
      set(build_this_tool TRUE)
    else()
      list(FIND SELECTED_TOOLS "${tool_dir}" idx)
      if(idx GREATER -1)
        set(build_this_tool TRUE)
      else()
        set(build_this_tool FALSE)
      endif()
    endif()

    if(build_this_tool)
      message(STATUS ">>> Adding tool: ${tool_dir}")
      add_subdirectory("${tool_dir}")
    else()
      message(STATUS "    Skipping tool: ${tool_dir}")
    endif()
  endif()
endforeach()


#----------------------------------------------------------------------------
# Copy all scripts to the build directory
#
set(EXAMPLEB1_SCRIPTS
  LAPPDInfo.txt
  MCLAPPDInfo.txt
  MRDInfo.txt
  PMTInfo.txt
  )

foreach(_script ${EXAMPLEB1_SCRIPTS})
  configure_file(
    ${PROJECT_SOURCE_DIR}/${_script}
    ${PROJECT_BINARY_DIR}/${_script}
    COPYONLY
    )
endforeach()




