#include "foray_rtstage.hpp"
#include <gltf/foray_modelconverter.hpp>
#include <scene/globalcomponents/foray_lightmanager.hpp>

namespace denoise {
    void ComplexRaytracingStage::Init(foray::core::Context* context, foray::scene::Scene* scene)
    {
        mLightManager = scene->GetComponent<foray::scene::gcomp::LightManager>();
        foray::stages::ExtRaytracingStage::Init(context, scene);
    }

    void ComplexRaytracingStage::CreateRtPipeline()
    {
        mShaderKeys.push_back(mContext->ShaderMan->CompileShader(RAYGEN_FILE, mRaygen));
        mShaderKeys.push_back(mContext->ShaderMan->CompileShader(CLOSESTHIT_FILE, mClosestHit));
        mShaderKeys.push_back(mContext->ShaderMan->CompileShader(ANYHIT_FILE, mAnyHit));
        mShaderKeys.push_back(mContext->ShaderMan->CompileShader(MISS_FILE, mMiss));
        mShaderKeys.push_back(mContext->ShaderMan->CompileShader(VISI_MISS_FILE, mVisiMiss));
        mShaderKeys.push_back(mContext->ShaderMan->CompileShader(VISI_ANYHIT_FILE, mVisiAnyHit));

        mPipeline.GetRaygenSbt().SetGroup(0, &mRaygen);
        mPipeline.GetHitSbt().SetGroup(0, &mClosestHit, &mAnyHit, nullptr);
        mPipeline.GetHitSbt().SetGroup(1, nullptr, &mVisiAnyHit, nullptr);
        mPipeline.GetMissSbt().SetGroup(0, &mMiss);
        mPipeline.GetMissSbt().SetGroup(1, &mVisiMiss);
        mPipeline.Build(mContext, mPipelineLayout);
    }

    void ComplexRaytracingStage::DestroyRtPipeline()
    {
        mPipeline.Destroy();
        mRaygen.Destroy();
        mClosestHit.Destroy();
        mAnyHit.Destroy();
        mMiss.Destroy();
        mVisiMiss.Destroy();
        mVisiAnyHit.Destroy();
    }

    void ComplexRaytracingStage::CreateOrUpdateDescriptors()
    {
        const uint32_t bindpoint_lights = 11;

        mDescriptorSet.SetDescriptorAt(bindpoint_lights, mLightManager->GetBuffer().GetVkDescriptorInfo(), VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, RTSTAGEFLAGS);

        foray::stages::ExtRaytracingStage::CreateOrUpdateDescriptors();
    }
}  // namespace denoise
