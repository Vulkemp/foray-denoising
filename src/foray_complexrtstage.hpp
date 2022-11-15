#pragma once
#include <foray_api.hpp>
#include <stages/foray_raytracingstage.hpp>
#include <util/foray_noisesource.hpp>
#include <scene/globalcomponents/foray_lightmanager.hpp>

namespace complex_raytracer {

    inline const std::string RAYGEN_FILE     = "shaders/raygen.rgen";
    inline const std::string CLOSESTHIT_FILE = "shaders/default/closesthit.rchit";
    inline const std::string MISS_FILE       = "shaders/default/miss.rmiss";
    inline const std::string VISI_MISS_FILE       = "shaders/visibilitytest/miss.rmiss";
    inline const std::string SCENE_FILE      = DATA_DIR "/gltf/testbox/scene.gltf";

    class ComplexRaytracingStage : public foray::stages::ExtRaytracingStage
    {
      public:
        virtual void Init(foray::core::Context* context, foray::scene::Scene* scene);

      protected:
        virtual void CreateRtPipeline() override;
        virtual void DestroyRtPipeline() override;

        virtual void CreateOrUpdateDescriptors() override;

        foray::core::ShaderModule mRaygen;
        foray::core::ShaderModule mClosestHit;
        foray::core::ShaderModule mMiss;
        foray::core::ShaderModule mVisiMiss;

        foray::scene::gcomp::LightManager* mLightManager;
    };

}  // namespace complex_raytracer
