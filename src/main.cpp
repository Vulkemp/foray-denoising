#include "denoiserapp.hpp"
#include <osi/foray_env.hpp>

int main(int argv, char** args)
{
    foray::osi::OverrideCurrentWorkingDirectory(CWD_OVERRIDE);
    denoise::DenoiserApp project;
    return project.Run();
}