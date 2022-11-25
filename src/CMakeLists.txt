cmake_minimum_required(VERSION 3.18)

project("foray-denoising")

MESSAGE("--- << CMAKE of ${PROJECT_NAME} >> --- ")
MESSAGE(STATUS "CURRENT SOURCE DIR \"${CMAKE_CURRENT_SOURCE_DIR}\"")

# Enable strict mode for own code
SET(CMAKE_CXX_FLAGS ${STRICT_FLAGS})


# collect sources
file(GLOB_RECURSE src "*.cpp")
    
# Make sure there are source files, add_executable would otherwise fail
if (NOT src)
    message(WARNING "Project \"${PROJECT_NAME}\" does not contain any source files")
    return()
endif ()

# Declare executable
add_executable(${PROJECT_NAME} ${src})

# Assign Compile Flags
target_compile_options(${PROJECT_NAME} PUBLIC "-DCWD_OVERRIDE=\"${CMAKE_CURRENT_LIST_DIR}\"")
target_compile_options(${PROJECT_NAME} PUBLIC "-DDATA_DIR=\"${CMAKE_SOURCE_DIR}/data\"")
target_compile_options(${PROJECT_NAME} PUBLIC "-DAPP_SHADER_DIR=\"${CMAKE_CURRENT_LIST_DIR}/shaders\"")

# Link foray lib
target_link_libraries(
	${PROJECT_NAME}
	PUBLIC foray
	PUBLIC foray-denoiser-asvgf
	PUBLIC foray-denoiser-bmfr
	PUBLIC foray-denoiser-nrd
)

# Windows requires SDL2 libs linked specifically
if (WIN32)
	target_link_libraries(
		${PROJECT_NAME}
		PUBLIC ${SDL2_LIBRARIES}
	)
endif()

# Configure include directories
target_include_directories(
	${PROJECT_NAME}
	PUBLIC "${CMAKE_SOURCE_DIR}/foray/src"
	PUBLIC "${CMAKE_SOURCE_DIR}/foray/third_party"
	PUBLIC "${CMAKE_SOURCE_DIR}/denoisers/foray-denoiser-asvgf/src"
	PUBLIC "${CMAKE_SOURCE_DIR}/denoisers/foray-denoiser-bmfr/src"
	PUBLIC "${CMAKE_SOURCE_DIR}/denoisers/foray-denoiser-nrd/src"
	PUBLIC ${Vulkan_INCLUDE_DIR}
)

if (ENABLE_OPTIX)
	target_include_directories(
		${PROJECT_NAME}
		PUBLIC "${CMAKE_SOURCE_DIR}/denoisers/foray-denoiser-optix/src"
	)
	target_link_libraries(
		${PROJECT_NAME}
		PUBLIC foray-denoiser-optix
	)
	target_compile_options(
		${PROJECT_NAME} 
		PUBLIC "-DENABLE_OPTIX"
	)
endif()