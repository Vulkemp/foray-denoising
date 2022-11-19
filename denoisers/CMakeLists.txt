
if (ENABLE_OPTIX)
    add_subdirectory("foray-denoiser-optix")
endif()

add_subdirectory("foray-denoiser-asvgf")
add_subdirectory("foray-denoiser-bmfr")