#include "sponza_sample.hpp"
#include <osi/foray_env.hpp>

int main(int argv, char** args)
{
    foray::osi::OverrideCurrentWorkingDirectory(CWD_OVERRIDE);
    ImportanceSamplingRtProject project;
    return project.Run();
}