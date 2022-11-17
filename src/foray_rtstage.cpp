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
        mRaygen.LoadFromSource(mContext, RAYGEN_FILE);
        mClosestHit.LoadFromSource(mContext, CLOSESTHIT_FILE);
        mMiss.LoadFromSource(mContext, MISS_FILE);
        mVisiMiss.LoadFromSource(mContext, VISI_MISS_FILE);

        mShaderSourcePaths.insert(mShaderSourcePaths.begin(), {RAYGEN_FILE, CLOSESTHIT_FILE, MISS_FILE, VISI_MISS_FILE});

        mPipeline.GetRaygenSbt().SetGroup(0, &mRaygen);
        mPipeline.GetHitSbt().SetGroup(0, &mClosestHit, nullptr, nullptr);
        mPipeline.GetMissSbt().SetGroup(0, &mMiss);
        mPipeline.GetMissSbt().SetGroup(1, &mVisiMiss);
        mPipeline.Build(mContext, mPipelineLayout);
    }

    void ComplexRaytracingStage::DestroyRtPipeline()
    {
        mPipeline.Destroy();
        mRaygen.Destroy();
        mClosestHit.Destroy();
        mMiss.Destroy();
        mVisiMiss.Destroy();
    }

    void ComplexRaytracingStage::CreateOrUpdateDescriptors()
    {
        const uint32_t bindpoint_lights = 11;

        mDescriptorSet.SetDescriptorAt(bindpoint_lights, mLightManager->GetBuffer().GetVkDescriptorInfo(), VkDescriptorType::VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, RTSTAGEFLAGS);

        foray::stages::ExtRaytracingStage::CreateOrUpdateDescriptors();
    }
}  // namespace denoise
