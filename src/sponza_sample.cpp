#include "sponza_sample.hpp"
#include <bench/foray_hostbenchmark.hpp>
#include <gltf/foray_modelconverter.hpp>
#include <imgui/imgui.h>
#include <util/foray_imageloader.hpp>

// #define USE_PRINTF

void ImportanceSamplingRtProject::ApiBeforeInit()
{
    mAuxiliaryCommandBufferCount = 0;
    mWindowSwapchain.GetWindow().DisplayMode(foray::osi::EDisplayMode::WindowedResizable);
    mInstance.SetEnableDebugReport(false);
}

VkBool32 myDebugCallback(VkDebugReportFlagsEXT      flags,
                         VkDebugReportObjectTypeEXT objectType,
                         uint64_t                   object,
                         size_t                     location,
                         int32_t                    messageCode,
                         const char*                pLayerPrefix,
                         const char*                pMessage,
                         void*                      pUserData)
{
    if(flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
    {
        printf("debugPrintfEXT: %s", pMessage);
    }

    return false;
}

void ImportanceSamplingRtProject::ApiInit()
{
#ifdef USE_PRINTF
    VkDebugReportCallbackEXT debugCallbackHandle;

    // Populate the VkDebugReportCallbackCreateInfoEXT
    VkDebugReportCallbackCreateInfoEXT ci = {};
    ci.sType                              = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    ci.pfnCallback                        = myDebugCallback;
    ci.flags                              = VK_DEBUG_REPORT_INFORMATION_BIT_EXT;
    ci.pUserData                          = nullptr;

    PFN_vkCreateDebugReportCallbackEXT pfn_vkCreateDebugReportCallbackEXT =
        reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetDeviceProcAddr(mContext.Device(), "vkCreateDebugReportCallbackEXT"));

    PFN_vkCreateDebugReportCallbackEXT CreateDebugReportCallback = VK_NULL_HANDLE;
    CreateDebugReportCallback                                    = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(mContext.Instance(), "vkCreateDebugReportCallbackEXT");

    // Create the callback handle
    CreateDebugReportCallback(mContext.Instance(), &ci, nullptr, &debugCallbackHandle);
#endif
    foray::logger()->set_level(spdlog::level::debug);
    LoadEnvironmentMap();
    GenerateNoiseSource();
    loadScene();
    ConfigureStages();
}

void ImportanceSamplingRtProject::ApiBeforeInstanceCreate(vkb::InstanceBuilder& instanceBuilder)
{
#ifdef USE_PRINTF
    instanceBuilder.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT);
    instanceBuilder.enable_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    instanceBuilder.enable_extension(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif
}

void ImportanceSamplingRtProject::ApiBeforeDeviceSelection(vkb::PhysicalDeviceSelector& pds)
{
    pds.add_required_extension(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
#ifdef WIN32
    pds.add_required_extension(VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME);
    pds.add_required_extension(VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
#else
    pds.add_required_extension(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
    pds.add_required_extension(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
#endif
#ifdef USE_PRINTF
    pds.add_required_extension(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
#endif
}

void ImportanceSamplingRtProject::ApiBeforeDeviceBuilding(vkb::DeviceBuilder& deviceBuilder)
{
    mTimelineFeature =
        VkPhysicalDeviceTimelineSemaphoreFeatures{.sType = VkStructureType::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES, .timelineSemaphore = VK_TRUE};

    deviceBuilder.add_pNext(&mTimelineFeature);
}

void ImportanceSamplingRtProject::ApiOnEvent(const foray::osi::Event* event)
{
    mScene->InvokeOnEvent(event);

    const foray::osi::EventInputBinary* binary = dynamic_cast<const foray::osi::EventInputBinary*>(event);
    if(!!binary)
    {
        if(binary->SourceInput->GetButtonId() == foray::osi::EButton::Keyboard_1 && binary->State)
        {
            mCurrentOutput = foray::stages::ExtRaytracingStage::OutputName;
            mOutputChanged = true;
        }
        if(binary->SourceInput->GetButtonId() == foray::osi::EButton::Keyboard_2 && binary->State)
        {
            mCurrentOutput = "Denoised Image";
            mOutputChanged = true;
        }
    }

    // process events for imgui
    mImguiStage.ProcessSdlEvent(&(event->RawSdlEventData));
}

void ImportanceSamplingRtProject::loadScene()
{
    std::vector<std::string> scenePaths({
        DATA_DIR "/gltf/testbox/testbox.gltf"
    });

    mScene = std::make_unique<foray::scene::Scene>(&mContext);
    foray::gltf::ModelConverter converter(mScene.get());
    for(const auto& path : scenePaths)
    {
        converter.LoadGltfModel(path);
    }

    mScene->UpdateTlasManager();
    mScene->UseDefaultCamera();

    for(int32_t i = 0; i < scenePaths.size(); i++)
    {
        const auto& path = scenePaths[i];
        const auto& log  = converter.GetBenchmark().GetLogs()[i];
        foray::logger()->info("Model Load \"{}\":\n{}", path, log.PrintPretty());
    }
}

void ImportanceSamplingRtProject::LoadEnvironmentMap()
{

    constexpr VkFormat                    hdrVkFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
    foray::util::ImageLoader<hdrVkFormat> imageLoader;
    // env maps at https://polyhaven.com/a/alps_field
    std::string pathToEnvMap = DATA_DIR "/env/default/envmap.exr";
    if(!imageLoader.Init(pathToEnvMap))
    {
        foray::logger()->warn("Loading env map failed \"{}\"", pathToEnvMap);
        return;
    }
    if(!imageLoader.Load())
    {
        foray::logger()->warn("Loading env map failed #2 \"{}\"", pathToEnvMap);
        return;
    }

    VkExtent2D ext2D{
        .width  = imageLoader.GetInfo().Extent.width,
        .height = imageLoader.GetInfo().Extent.height
    };

    foray::core::ManagedImage::CreateInfo ci(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, hdrVkFormat,
                                             ext2D, "Environment map");

    imageLoader.InitManagedImage(&mContext, &mEnvMap, ci);
    imageLoader.Destroy();

    VkSamplerCreateInfo samplerCi{.sType                   = VkStructureType::VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                  .magFilter               = VkFilter::VK_FILTER_LINEAR,
                                  .minFilter               = VkFilter::VK_FILTER_LINEAR,
                                  .addressModeU            = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                  .addressModeV            = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                  .addressModeW            = VkSamplerAddressMode::VK_SAMPLER_ADDRESS_MODE_REPEAT,
                                  .anisotropyEnable        = VK_FALSE,
                                  .compareEnable           = VK_FALSE,
                                  .minLod                  = 0,
                                  .maxLod                  = 0,
                                  .unnormalizedCoordinates = VK_FALSE};

    mEnvMapSampled.Init(&mContext, &mEnvMap, samplerCi);
}

void ImportanceSamplingRtProject::GenerateNoiseSource()
{
    foray::bench::HostBenchmark bench;
    bench.Begin();
    mNoiseSource.Create(&mContext);
    bench.End();
    foray::logger()->info("Create Noise Tex \n{}", bench.GetLogs().front().PrintPretty());
}

void ImportanceSamplingRtProject::ApiDestroy()
{
    mNoiseSource.Destroy();
    mScene->Destroy();
    mScene = nullptr;
    mDenoiser.Destroy();
    mGbufferStage.Destroy();
    mImguiStage.Destroy();
    mRaytraycingStage.Destroy();
    mEnvMap.Destroy();
    #ifdef ENABLE_OPTIX
    mDenoiser.Destroy();
    #endif
    mDenoiseSemaphore.Destroy();
    mDenoisedImage.Destroy();
}

void ImportanceSamplingRtProject::PrepareImguiWindow()
{
    mImguiStage.AddWindowDraw([this]() {
        foray::base::RenderLoop::FrameTimeAnalysis analysis = this->GetRenderLoop().AnalyseFrameTimes();

        ImGui::Begin("window");
        if(analysis.Count > 0)
        {
            ImGui::Text("FPS: %f avg %f min", 1.f / analysis.AvgFrameTime, 1.f / analysis.MaxFrameTime);
        }
        std::string denoiserLabel = this->mDenoiser.GetUILabel();
        ImGui::Text("%s", denoiserLabel.c_str());

        const char* current = mCurrentOutput.data();
        if(ImGui::BeginCombo("Output", current))
        {
            std::string_view newOutput = mCurrentOutput;
            for(auto output : mOutputs)
            {
                bool selected = output.first == mCurrentOutput;
                if(ImGui::Selectable(output.first.data(), selected))
                {
                    newOutput = output.first;
                }
            }

            if(newOutput != mCurrentOutput)
            {
                mCurrentOutput = newOutput;
                mOutputChanged = true;
            }

            ImGui::EndCombo();
        }

        if (ImGui::CollapsingHeader("Denoiser Config"))
        {
            this->mDenoiser.DisplayImguiConfiguration();
        }

#ifdef ENABLE_GBUFFER_BENCH
        if(mDisplayedLog.Timestamps.size() > 0 && ImGui::CollapsingHeader("GBuffer Benchmark"))
        {
            mDisplayedLog.PrintImGui();
        }
#endif  // ENABLE_GBUFFER_BENCH

        ImGui::End();
    });
}

void ImportanceSamplingRtProject::ConfigureStages()
{
    mGbufferStage.Init(&mContext, mScene.get());
    auto albedoImage = mGbufferStage.GetImageOutput(foray::stages::GBufferStage::AlbedoOutputName);
    auto normalImage = mGbufferStage.GetImageOutput(foray::stages::GBufferStage::NormalOutputName);
    auto motionImage = mGbufferStage.GetImageOutput(foray::stages::GBufferStage::MotionOutputName);

    mRaytraycingStage.Init(&mContext, mScene.get(), &mEnvMapSampled, &mNoiseSource.GetImage());
    auto rtImage = mRaytraycingStage.GetImageOutput(foray::stages::ExtRaytracingStage::OutputName);

    mDenoiseSemaphore.Create(&mContext);

    VkExtent2D extent{.width = mContext.GetSwapchainSize().width, .height = mContext.GetSwapchainSize().height};

    foray::core::ManagedImage::CreateInfo ci(VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                                 | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                             VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT, extent, "Denoised Image");

    mDenoisedImage.Create(&mContext, ci);

    foray::stages::DenoiserConfig config(rtImage, &mDenoisedImage, &mGbufferStage);
    config.AuxiliaryInputs[std::string(mNoiseSource.GetImage().GetName())] = &mNoiseSource.GetImage();
    config.Semaphore   = &mDenoiseSemaphore;

    mDenoiser.Init(&mContext, config);

    UpdateOutputs();

    mImguiStage.Init(&mContext, mOutputs[mCurrentOutput]);
    PrepareImguiWindow();

    // �nit copy stage
    mImageToSwapchainStage.Init(&mContext, mOutputs[mCurrentOutput]);

    RegisterRenderStage(&mGbufferStage);
    RegisterRenderStage(&mRaytraycingStage);
    RegisterRenderStage(&mDenoiser);
    RegisterRenderStage(&mImguiStage);
    RegisterRenderStage(&mImageToSwapchainStage);
}

void ImportanceSamplingRtProject::ApiRender(foray::base::FrameRenderInfo& renderInfo)
{
    if(mOutputChanged)
    {
        ApplyOutput();
        mOutputChanged = false;
    }

    foray::core::DeviceCommandBuffer& primaryCmdBuffer = renderInfo.GetPrimaryCommandBuffer(); 

    // Begin aux command buffer
    primaryCmdBuffer.Begin();

    mScene->Update(renderInfo, primaryCmdBuffer);
    mGbufferStage.RecordFrame(primaryCmdBuffer, renderInfo);

    mRaytraycingStage.RecordFrame(primaryCmdBuffer, renderInfo);

    mDenoiser.RecordFrame(primaryCmdBuffer, renderInfo);

    // draw imgui windows
    mImguiStage.RecordFrame(primaryCmdBuffer, renderInfo);

    renderInfo.ClearSwapchainImage(primaryCmdBuffer);

    // copy final image to swapchain
    mImageToSwapchainStage.RecordFrame(primaryCmdBuffer, renderInfo);

    renderInfo.PrepareSwapchainImageForPresent(primaryCmdBuffer);

    primaryCmdBuffer.Submit();
}

void ImportanceSamplingRtProject::ApiFrameFinishedExecuting(uint64_t frameIndex)
{
#ifdef ENABLE_GBUFFER_BENCH
    mGbufferStage.GetBenchmark().LogQueryResults(frameIndex);
    mDisplayedLog = mGbufferStage.GetBenchmark().GetLogs().back();
#endif  // ENABLE_GBUFFER_BENCH
}

void ImportanceSamplingRtProject::ApiOnResized(VkExtent2D size)
{
    mScene->InvokeOnResized(size);

    mDenoisedImage.Resize(VkExtent3D{size.width, size.height, 1});
}

void lUpdateOutput(std::map<std::string_view, foray::core::ManagedImage*>& map, foray::stages::RenderStage& stage, const std::string_view name)
{
    map[name] = stage.GetImageOutput(name);
}

void ImportanceSamplingRtProject::UpdateOutputs()
{
    mOutputs.clear();
    lUpdateOutput(mOutputs, mGbufferStage, foray::stages::GBufferStage::AlbedoOutputName);
    lUpdateOutput(mOutputs, mGbufferStage, foray::stages::GBufferStage::PositionOutputName);
    lUpdateOutput(mOutputs, mGbufferStage, foray::stages::GBufferStage::NormalOutputName);
    lUpdateOutput(mOutputs, mRaytraycingStage, foray::stages::ExtRaytracingStage::OutputName);
    mOutputs.emplace("Denoised Image", &mDenoisedImage);

    if(mCurrentOutput.size() == 0 || !mOutputs.contains(mCurrentOutput))
    {
        if(mOutputs.size() == 0)
        {
            mCurrentOutput = "";
        }
        else
        {
            mCurrentOutput = mOutputs.begin()->first;
        }
    }
}

void ImportanceSamplingRtProject::ApplyOutput()
{
    vkDeviceWaitIdle(mDevice);
    auto output = mOutputs[mCurrentOutput];
    mImguiStage.SetBackgroundImage(output);
    mImageToSwapchainStage.SetSrcImage(output);
}