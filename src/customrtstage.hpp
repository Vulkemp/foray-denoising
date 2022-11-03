#pragma once
#include <stages/foray_raytracingstage.hpp>

class CustomRtStage : public foray::stages::RaytracingStage
{
public:
    virtual void Init(foray::core::Context *context, foray::scene::Scene *scene, foray::core::CombinedImageSampler *envmap, foray::core::CombinedImageSampler *noiseSource);
    virtual void CreateRaytraycingPipeline() override;
    virtual void OnShadersRecompiled() override;

    virtual void Destroy() override;
    virtual void DestroyShaders() override;

    struct RtStageShader
    {
        std::string Path = "";
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
