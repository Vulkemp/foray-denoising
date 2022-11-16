#include "sponza_sample.hpp"
#include <bench/foray_hostbenchmark.hpp>
#include <gltf/foray_modelconverter.hpp>
#include <imgui/imgui.h>
#include <util/foray_imageloader.hpp>

void ImportanceSamplingRtProject::ApiBeforeInit()
{
    mAuxiliaryCommandBufferCount = 1;
    mWindowSwapchain.GetWindow().DisplayMode(foray::osi::EDisplayMode::WindowedResizable);
    mInstance.SetEnableDebugReport(false);
}

void ImportanceSamplingRtProject::ApiInit()
{
    foray::logger()->set_level(spdlog::level::debug);
    LoadEnvironmentMap();
    loadScene();
    ConfigureStages();
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
    std::vector<std::string> scenePaths({DATA_DIR "/gltf/testbox/scene.gltf"});

    mScene = std::make_unique<foray::scene::Scene>(&mContext);
    foray::gltf::ModelConverter converter(mScene.get());
    for(const auto& path : scenePaths)
    {
        converter.LoadGltfModel(path);
    }

    mScene->UpdateTlasManager();
    mScene->UseDefaultCamera();
    mScene->UpdateLightManager();

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

    VkExtent2D ext2D{.width = imageLoader.GetInfo().Extent.width, .height = imageLoader.GetInfo().Extent.height};

    foray::core::ManagedImage::CreateInfo ci(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, hdrVkFormat, ext2D,
                                             "Environment map");

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

void ImportanceSamplingRtProject::ApiDestroy()
{
    mScene->Destroy();
    mScene = nullptr;
    mASvgfDenoiser.Destroy();
#ifdef ENABLE_OPTIX
    mOptiXDenoiser.Destroy();
#endif
    mGbufferStage.Destroy();
    mImguiStage.Destroy();
    mRaytraycingStage.Destroy();
    mEnvMap.Destroy();
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

        {  // Output Switching
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
        }
        {  // Denoiser Switching
            std::string denoiserLabel = this->mActiveDenoiser->GetUILabel();
            if(ImGui::BeginCombo("Denoiser", denoiserLabel.c_str()))
            {
                
                for(int32_t i = 0; i < this->mDenoisers.size(); i++)
                {
                    bool selected = this->mActiveDenoiserIndex == i;
                    std::string name = this->mDenoisers[i]->GetUILabel();
                    if(ImGui::Selectable(name.c_str(), selected))
                    {
                        this->mActiveDenoiserIndex = i;
                    }
                }

                ImGui::EndCombo();
            }
        }

        if(ImGui::CollapsingHeader("Denoiser Config"))
        {
            this->mActiveDenoiser->DisplayImguiConfiguration();
        }

        if(ImGui::CollapsingHeader("Denoiser Benchmark"))
        {
            this->mDenoiserBenchmarkLog.PrintImGui();
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

    mRaytraycingStage.Init(&mContext, mScene.get());
    auto rtImage = mRaytraycingStage.GetImageOutput(foray::stages::ExtRaytracingStage::OutputName);

    mDenoiseSemaphore.Create(&mContext);

    VkExtent2D extent{.width = mContext.GetSwapchainSize().width, .height = mContext.GetSwapchainSize().height};

    foray::core::ManagedImage::CreateInfo ci(VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                                 | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                             VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, extent, "Denoised Image");

    mDenoisedImage.Create(&mContext, ci);
    UpdateOutputs();

    SetDenoiserActive();

    mImguiStage.Init(&mContext, mOutputs[mCurrentOutput]);
    PrepareImguiWindow();

    // �nit copy stage
    mImageToSwapchainStage.Init(&mContext, mOutputs[mCurrentOutput]);

    RegisterRenderStage(&mGbufferStage);
    RegisterRenderStage(&mRaytraycingStage);
    RegisterRenderStage(&mASvgfDenoiser);
#ifdef ENABLE_OPTIX
    RegisterRenderStage(&mOptiXDenoiser);
#endif
    RegisterRenderStage(&mImguiStage);
    RegisterRenderStage(&mImageToSwapchainStage);
}

void ImportanceSamplingRtProject::SetDenoiserActive()
{
    if(mActiveDenoiser == mDenoisers[mActiveDenoiserIndex])
    {
        return;
    }

    vkDeviceWaitIdle(mDevice);

    if(!!mActiveDenoiser)
    {
        mActiveDenoiser->Destroy();
    }
    mActiveDenoiser = mDenoisers[mActiveDenoiserIndex];
    foray::stages::DenoiserConfig config(mRaytraycingStage.GetRtOutput(), &mDenoisedImage, &mGbufferStage);
    config.Benchmark = &mDenoiserBenchmark;
    config.Semaphore = &mDenoiseSemaphore;

    mActiveDenoiser->Init(&mContext, config);

    foray::stages::ExternalDenoiserStage* externalDenoiser = dynamic_cast<foray::stages::ExternalDenoiserStage*>(mActiveDenoiser);

    {  // Setup semaphores
        for(foray::base::InFlightFrame& frame : mInFlightFrames)
        {
            foray::core::DeviceCommandBuffer& auxCmdBuffer     = frame.GetAuxiliaryCommandBuffer(0);
            foray::core::DeviceCommandBuffer& primaryCmdBuffer = frame.GetPrimaryCommandBuffer();
            auxCmdBuffer.SetSignalSemaphores(std::vector<foray::core::SemaphoreReference>({foray::core::SemaphoreReference::Timeline(mDenoiseSemaphore, 0)}));
            if(!!externalDenoiser)
            {
                primaryCmdBuffer.SetWaitSemaphores(std::vector<foray::core::SemaphoreReference>(
                    {foray::core::SemaphoreReference::Binary(frame.GetSwapchainImageReady(), VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR),
                     foray::core::SemaphoreReference::Timeline(mDenoiseSemaphore, 0)}));
            }
            else
            {
                primaryCmdBuffer.SetWaitSemaphores(std::vector<foray::core::SemaphoreReference>(
                    {foray::core::SemaphoreReference::Binary(frame.GetSwapchainImageReady(), VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR)}));
            }
        }
    }
}

void ImportanceSamplingRtProject::ApiRender(foray::base::FrameRenderInfo& renderInfo)
{
    if(mOutputChanged)
    {
        ApplyOutput();
        mOutputChanged = false;
    }
    SetDenoiserActive();

    foray::stages::ExternalDenoiserStage* externalDenoiser = dynamic_cast<foray::stages::ExternalDenoiserStage*>(mActiveDenoiser);

    foray::core::DeviceCommandBuffer& auxCmdBuffer     = renderInfo.GetAuxCommandBuffer(0);
    foray::core::DeviceCommandBuffer& primaryCmdBuffer = renderInfo.GetPrimaryCommandBuffer();

    foray::core::DeviceCommandBuffer* cmdBuffer                 = &primaryCmdBuffer;
    uint64_t                          timelineValueSignal       = renderInfo.GetFrameNumber() * 2 + 1;
    uint64_t                          timelineValueWaitExternal = renderInfo.GetFrameNumber() * 2 + 2;
    if(!!externalDenoiser)
    {
        auxCmdBuffer.GetSignalSemaphores().back().TimelineValue   = timelineValueSignal;
        primaryCmdBuffer.GetWaitSemaphores().back().TimelineValue = timelineValueWaitExternal;
        cmdBuffer                                                 = &auxCmdBuffer;
    }


    // Begin aux command buffer
    cmdBuffer->Begin();

    mScene->Update(renderInfo, *cmdBuffer);
    mGbufferStage.RecordFrame(*cmdBuffer, renderInfo);

    mRaytraycingStage.RecordFrame(*cmdBuffer, renderInfo);

    if(!!externalDenoiser)
    {
        externalDenoiser->BeforeDenoise(*cmdBuffer, renderInfo);
        cmdBuffer->Submit();
        externalDenoiser->DispatchDenoise(timelineValueSignal, timelineValueWaitExternal);
        primaryCmdBuffer.Begin();
        externalDenoiser->AfterDenoise(primaryCmdBuffer, renderInfo);
    }
    else
    {
        mActiveDenoiser->RecordFrame(primaryCmdBuffer, renderInfo);
    }

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
    if(mDenoiserBenchmark.Exists() && mDenoiserBenchmark.LogQueryResults(frameIndex))
    {
        mDenoiserBenchmarkLog = mDenoiserBenchmark.GetLogs().back();
        mDenoiserBenchmark.GetLogs().clear();
    }
}

void ImportanceSamplingRtProject::ApiOnResized(VkExtent2D size)
{
    mScene->InvokeOnResized(size);

    mDenoisedImage.Resize(size);
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