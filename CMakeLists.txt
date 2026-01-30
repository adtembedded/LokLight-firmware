cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME loklight)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/config.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/dcc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/led_control.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/loklight_wrapper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/loklight.cpp
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

# first set overall optimization flags and override whatever else has been set so far
set(CMAKE_C_FLAGS_DEBUG "-Og -g3") 
set(CMAKE_CXX_FLAGS_DEBUG "-Og -g3")
set(CMAKE_C_FLAGS_RELEASE "-Os -g0") 
set(CMAKE_CXX_FLAGS_RELEASE "-Os -g0")
# For some reason, the flags below are ignored
set(CMAKE_C_FLAGS_RELWITHDEBUGINFO "-Og -g3" CACHE STRING "Flags for RelWithDebInfo build" FORCE) 
set(CMAKE_CXX_FLAGS_RELWITHDEBUGINFO "-Og -g3" CACHE STRING "Flags for RelWithDebInfo build" FORCE) 
set(CMAKE_C_FLAGS_MINRELSIZE "-Oz -g0" CACHE STRING "Flags for MinRelSize build" FORCE)
set(CMAKE_CXX_FLAGS_MINRELSIZE "-Oz -g0" CACHE STRING "Flags for MinRelSize build" FORCE)

string(TOUPPER "${CMAKE_BUILD_TYPE}" build_type_upper_c)
# message(STATUS "Build type: ${build_type_upper_c}") 

# now set the flags for the STM HAL, which is enormous if not optimized
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(stm32cubemx INTERFACE -Og -g3 -ffunction-sections -fdata-sections)
    # Linker options to not make a difference?
    # target_link_options(stm32cubemx INTERFACE -Wl,--gc-sections,--strip-all)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release") 
    target_compile_options(stm32cubemx INTERFACE -Os -g0 -ffunction-sections -fdata-sections ) 
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo") 
    target_compile_options(stm32cubemx INTERFACE -Os -g3 -ffunction-sections -fdata-sections ) 
elseif(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel") 
    target_compile_options(stm32cubemx INTERFACE -Oz -g0 -ffunction-sections -fdata-sections ) 
else() 
    message(WARNING "Unknown build type: ${CMAKE_BUILD_TYPE}")
    # Default to debug
    target_compile_options(stm32cubemx INTERFACE -Og -g3 -ffunction-sections -fdata-sections)
endif()

set(c_var_name "CMAKE_C_FLAGS_${build_type_upper_c}")
set(cxx_var_name "CMAKE_CXX_FLAGS_${build_type_upper_c}")
set(all_c_flags "${CMAKE_C_FLAGS} ${${c_var_name}}") 
set(all_cxx_flags "${CMAKE_CXX_FLAGS} ${${cxx_var_name}}")
message(STATUS "Active C flags: ${all_c_flags}") 
message(STATUS "Active CXX flags: ${all_cxx_flags}") 
get_target_property(flags stm32cubemx INTERFACE_COMPILE_OPTIONS) 
message(STATUS "stm32cubemx compile flags: ${flags}")