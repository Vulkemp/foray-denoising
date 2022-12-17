#pragma once

#include <foray_api.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <foray_asvgf.hpp>
#include <foray_bmfr.hpp>
#include <foray_glm.hpp>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <stdint.h>
#include <vector>

#include "foray_rtstage.hpp"
#ifdef ENABLE_OPTIX
#include <foray_optix.hpp>
#endif
#include <bench/foray_devicebenchmark.hpp>
#include <stages/foray_denoiserstage.hpp>
#include <util/foray_noisesource.hpp>

namespace denoise {

    inline const char* SCENE_PATH = DATA_DIR "/gltf/testbox/scene.gltf";

#if ENABLE_BENCHMODE
    inline const uint32_t BENCH_FRAMES = 2000;
#endif

    class DenoiserApp : public foray::base::DefaultAppBase
    {
      public:
        DenoiserApp()  = default;
        ~DenoiserApp() = default;

      protected:
        virtual void ApiBeforeInit() override;
        virtual void ApiBeforeInstanceCreate(vkb::InstanceBuilder& builder) override;
        virtual void ApiBeforeDeviceSelection(vkb::PhysicalDeviceSelector& pds) override;
        virtual void ApiBeforeDeviceBuilding(vkb::DeviceBuilder& deviceBuilder) override;
        virtual void ApiInit() override;
        void         LoadEnvironmentMap();
        void         LoadScene();
        void         ConfigureStages();

        virtual void ApiRender(foray::base::FrameRenderInfo& renderInfo) override;
        virtual void ApiFrameFinishedExecuting(uint64_t frameIndex) override;
        virtual void ApiOnResized(VkExtent2D size) override;
        virtual void ApiOnEvent(const foray::osi::Event* event) override;
        void         ImGui();

        virtual void ApiDestroy() override;


        std::unique_ptr<foray::scene::Scene> mScene;


        /// @brief generates a GBuffer (Albedo, Positions, Normal, Motion Vectors, Mesh Instance Id as output images)
        foray::stages::GBufferStage mGbufferStage;
        /// @brief Renders immediate mode GUI
        foray::stages::ImguiStage mImguiStage;
        /// @brief Copies the intermediate rendertarget to the swapchain image
        foray::stages::ImageToSwapchainStage mImageToSwapchainStage;
        /// @brief Generates a raytraced image
        denoise::ComplexRaytracingStage mRaytraycingStage;

        foray::core::ManagedImage         mEnvMap{};
        foray::core::CombinedImageSampler mEnvMapSampled;

        VkPhysicalDeviceTimelineSemaphoreFeatures mTimelineFeature{};
        foray::core::ManagedImage                 mDenoisedImage;
        foray::util::ExternalSemaphore            mDenoiseSemaphore;

        foray::bmfr::BmfrDenoiser        mBmfrDenoiser;
        foray::asvgf::ASvgfDenoiserStage mASvgfDenoiser;
#ifdef ENABLE_OPTIX
        foray::optix::OptiXDenoiserStage mOptiXDenoiser;
#endif

        foray::bench::DeviceBenchmark mDenoiserBenchmark;
        foray::bench::BenchmarkLog    mDenoiserBenchmarkLog;

        int32_t                                    mActiveDenoiserIndex = 0;
        std::vector<foray::stages::DenoiserStage*> mDenoisers           = {&mBmfrDenoiser, &mASvgfDenoiser,
#ifdef ENABLE_OPTIX
                                                                 &mOptiXDenoiser
#endif
        };
        foray::stages::DenoiserStage* mActiveDenoiser = nullptr;

        void ActivateOrSwitchDenoiser();
        void ActivateOrSwitchOutput();

        std::vector<foray::core::ManagedImage*> mOutputs;
        int32_t                                 mActiveOutputIndex = 0;
        foray::core::ManagedImage*              mActiveOutput      = nullptr;
    };
}  // namespace denoise