#include "customrtstage.hpp"
#include <core/foray_shadermanager.hpp>

void CustomRtStage::CreateRtPipeline()
{
    mRaygen.Create(mContext);
    mDefault_AnyHit.Create(mContext);
    mDefault_ClosestHit.Create(mContext);
    mDefault_Miss.Create(mContext);

    mPipeline.GetRaygenSbt().SetGroup(0, &(mRaygen.Module));
    mPipeline.GetMissSbt().SetGroup(0, &(mDefault_Miss.Module));
    mPipeline.GetHitSbt().SetGroup(0, &(mDefault_ClosestHit.Module), &(mDefault_AnyHit.Module), nullptr);

    mShaderSourcePaths.insert(mShaderSourcePaths.end(), {mRaygen.Path, mDefault_AnyHit.Path, mDefault_ClosestHit.Path, mDefault_Miss.Path});

    mPipeline.Build(mContext, mPipelineLayout);
}

void CustomRtStage::DestroyRtPipeline()
{
    mPipeline.Destroy();
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
