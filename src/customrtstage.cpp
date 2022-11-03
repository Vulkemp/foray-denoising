#include "customrtstage.hpp"
#include <core/foray_shadermanager.hpp>

void CustomRtStage::Init(foray::core::Context *context, foray::scene::Scene *scene, foray::core::CombinedImageSampler *envmap, foray::core::CombinedImageSampler *noiseSource)
{
    mContext = context;
    mScene = scene;
    if (!!envmap)
    {
        mEnvMap = envmap;
    }
    if (!!noiseSource)
    {
        mNoiseSource = noiseSource;
    }
    RaytracingStage::Init();
}
void CustomRtStage::CreateRaytraycingPipeline()
{
    mRaygen.Create(mContext);
    mDefault_AnyHit.Create(mContext);
    mDefault_ClosestHit.Create(mContext);
    mDefault_Miss.Create(mContext);

    mPipeline.GetRaygenSbt().SetGroup(0, &(mRaygen.Module));
    mPipeline.GetMissSbt().SetGroup(0, &(mDefault_Miss.Module));
    mPipeline.GetHitSbt().SetGroup(0, &(mDefault_ClosestHit.Module), &(mDefault_AnyHit.Module), nullptr);
    RaytracingStage::CreateRaytraycingPipeline();
}

void CustomRtStage::OnShadersRecompiled()
{
    foray::core::ShaderManager& shaderCompiler = foray::core::ShaderManager::Instance();
    bool rebuild = shaderCompiler.HasShaderBeenRecompiled(mRaygen.Path) || shaderCompiler.HasShaderBeenRecompiled(mDefault_AnyHit.Path) || shaderCompiler.HasShaderBeenRecompiled(mDefault_ClosestHit.Path) || shaderCompiler.HasShaderBeenRecompiled(mDefault_Miss.Path);
    if (rebuild)
    {
        ReloadShaders();
    }
}
void CustomRtStage::Destroy()
{
    RaytracingStage::Destroy();
}

void CustomRtStage::DestroyShaders()
{
    mRaygen.Destroy();
    mDefault_AnyHit.Destroy();
    mDefault_ClosestHit.Destroy();
    mDefault_Miss.Destroy();
}

void CustomRtStage::RtStageShader::Create(foray::core::Context *context)
{
    Module.LoadFromSpirv(context, Path);
}
void CustomRtStage::RtStageShader::Destroy()
{
    Module.Destroy();
}
