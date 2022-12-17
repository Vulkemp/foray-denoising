#pragma once
#include <foray_api.hpp>
#include <stages/foray_defaultraytracingstage.hpp>
#include <util/foray_noisesource.hpp>
#include <scene/globalcomponents/foray_lightmanager.hpp>

namespace denoise {

    inline const std::string RAYGEN_FILE     = APP_SHADER_DIR "/raygen.rgen";
    inline const std::string CLOSESTHIT_FILE = APP_SHADER_DIR "/default/closesthit.rchit";
    inline const std::string ANYHIT_FILE = APP_SHADER_DIR "/default/anyhit.rahit";
    inline const std::string MISS_FILE       = APP_SHADER_DIR "/default/miss.rmiss";
    inline const std::string VISI_MISS_FILE  = APP_SHADER_DIR "/visibilitytest/miss.rmiss";
    inline const std::string VISI_ANYHIT_FILE  = APP_SHADER_DIR "/visibilitytest/anyhit.rahit";

    class ComplexRaytracingStage : public foray::stages::DefaultRaytracingStageBase
    {
      public:
        virtual void Init(foray::core::Context* context, foray::scene::Scene* scene);

      protected:
        virtual void ApiCreateRtPipeline() override;
        virtual void ApiDestroyRtPipeline() override;

        virtual void CreateOrUpdateDescriptors() override;

        foray::core::ShaderModule mRaygen;
        foray::core::ShaderModule mClosestHit;
        foray::core::ShaderModule mAnyHit;
        foray::core::ShaderModule mMiss;
        foray::core::ShaderModule mVisiMiss;
        foray::core::ShaderModule mVisiAnyHit;

        foray::scene::gcomp::LightManager* mLightManager;
    };

}  // namespace denoise
