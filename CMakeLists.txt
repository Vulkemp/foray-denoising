cmake_minimum_required(VERSION 3.18)

project("foray-denoising")

# Include Compiler Config (sets c++ 20 and compiler flags)
include("foray/cmakescripts/compilerconfig.cmake")

# Run Locate SDL2 script
set(SDL2_HINT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/foray/third_party")
include("foray/cmakescripts/locatesdl2.cmake")

option(ENABLE_OPTIX "If set, enables OptiX (requires Cuda and OptiX include)" OFF)
option(ENABLE_BENCHMODE "If set, enables Bench mode (no freeflight camera, terminates after 2000 frames rendered, saves CSV result to disk)" OFF)
option(SHADER_PRINTF "If set, enables debug report for displaying shader printf debug messages. May cause NSight to crash or malfunction." OFF)

# Add subdirectories
add_subdirectory("foray")
add_subdirectory("denoisers")
add_subdirectory("src")
