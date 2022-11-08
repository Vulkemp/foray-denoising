#pragma once
#include <stages/foray_raytracingstage.hpp>

class CustomRtStage : public foray::stages::ExtRaytracingStage
{
public:
    virtual void CreateRtPipeline() override;

    virtual void DestroyRtPipeline() override;

    struct RtStageShader
    {
        foray::osi::Utf8Path Path = "";
        foray::core::ShaderModule Module;

        void Create(foray::core::Context *context);
        void Destroy();
    };

protected:
    RtStageShader mRaygen{"shaders/raygen.rgen"};
    RtStageShader mDefault_AnyHit{"shaders/ray-default/anyhit.rahit"};
    RtStageShader mDefault_ClosestHit{"shaders/ray-default/closesthit.rchit"};
    RtStageShader mDefault_Miss{"shaders/ray-default/miss.rmiss"};
};
