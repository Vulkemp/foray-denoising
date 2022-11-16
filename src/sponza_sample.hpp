#pragma once

#include <foray_api.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <foray_asvgf.hpp>
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

#include "foray_complexrtstage.hpp"
#ifdef ENABLE_OPTIX
#include <foray_optix.hpp>
#endif
#include <bench/foray_devicebenchmark.hpp>
#include <stages/foray_denoiserstage.hpp>
#include <util/foray_noisesource.hpp>

using namespace foray::api;

class ImportanceSamplingRtProject : public foray::base::DefaultAppBase
{
  public:
    ImportanceSamplingRtProject()  = default;
    ~ImportanceSamplingRtProject() = default;

  protected:
    virtual void ApiBeforeDeviceSelection(vkb::PhysicalDeviceSelector& pds) override;
    virtual void ApiBeforeDeviceBuilding(vkb::DeviceBuilder& deviceBuilder) override;
    virtual void ApiBeforeInit() override;
    virtual void ApiInit() override;
    virtual void ApiOnEvent(const foray::osi::Event* event) override;

    virtual void ApiRender(foray::base::FrameRenderInfo& renderInfo) override;
    virtual void ApiFrameFinishedExecuting(uint64_t frameIndex) override;
    virtual void ApiOnResized(VkExtent2D size) override;
    virtual void ApiDestroy() override;

    void PrepareImguiWindow();

    std::unique_ptr<foray::scene::Scene> mScene;

    void loadScene();
    void LoadEnvironmentMap();

    /// @brief generates a GBuffer (Albedo, Positions, Normal, Motion Vectors, Mesh Instance Id as output images)
    foray::stages::GBufferStage mGbufferStage;
    /// @brief Renders immediate mode GUI
    foray::stages::ImguiStage mImguiStage;
    /// @brief Copies the intermediate rendertarget to the swapchain image
    foray::stages::ImageToSwapchainStage mImageToSwapchainStage;
    /// @brief Generates a raytraced image
    complex_raytracer::ComplexRaytracingStage mRaytraycingStage;

    foray::core::ManagedImage         mEnvMap{};
    foray::core::CombinedImageSampler mEnvMapSampled;

    VkPhysicalDeviceTimelineSemaphoreFeatures mTimelineFeature{};
    foray::core::ManagedImage                 mDenoisedImage;
    foray::util::ExternalSemaphore            mDenoiseSemaphore;

    foray::asvgf::ASvgfDenoiserStage mASvgfDenoiser;
#ifdef ENABLE_OPTIX
    foray::optix::OptiXDenoiserStage mOptiXDenoiser;
#endif

    foray::bench::DeviceBenchmark mDenoiserBenchmark;
    foray::bench::BenchmarkLog    mDenoiserBenchmarkLog;

    int32_t mActiveDenoiserIndex = 0;
    std::vector<foray::stages::DenoiserStage*> mDenoisers = {
        &mASvgfDenoiser,
#ifdef ENABLE_OPTIX
        &mOptiXDenoiser
#endif
        };
    foray::stages::DenoiserStage* mActiveDenoiser         = nullptr;

    void SetDenoiserActive();

    void ConfigureStages();

    std::map<std::string_view, foray::core::ManagedImage*> mOutputs;
    std::string_view                                       mCurrentOutput = "";
    bool                                                   mOutputChanged = false;

#ifdef ENABLE_GBUFFER_BENCH
    foray::BenchmarkLog mDisplayedLog;
#endif  // ENABLE_GBUFFER_BENCH

    void UpdateOutputs();
    void ApplyOutput();
};