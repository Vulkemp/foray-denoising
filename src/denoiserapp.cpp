#include "denoiserapp.hpp"
#include <bench/foray_hostbenchmark.hpp>
#include <filesystem>
#include <gltf/foray_modelconverter.hpp>
#include <imgui/imgui.h>
#include <scene/components/foray_camera.hpp>
#include <scene/globalcomponents/foray_animationmanager.hpp>
#include <scene/globalcomponents/foray_cameramanager.hpp>
#include <util/foray_imageloader.hpp>

namespace denoise {

#pragma region Init

    void DenoiserApp::ApiBeforeInit()
    {
        mAuxiliaryCommandBufferCount = 1;
#ifdef SHADER_PRINTF
        mInstance.SetEnableDebugReport(true);
#else
        mInstance.SetEnableDebugReport(false);
#endif
    }

    void DenoiserApp::ApiBeforeInstanceCreate(vkb::InstanceBuilder& builder)
    {
        builder.enable_extension(VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME);
    }

    void DenoiserApp::ApiBeforeDeviceSelection(vkb::PhysicalDeviceSelector& pds)
    {
        pds.add_required_extension(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
        pds.add_required_extension(VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME);
        pds.add_required_extension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
        pds.add_required_extension(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME);
#ifdef WIN32
        pds.add_required_extension(VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME);
        pds.add_required_extension(VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME);
#else
        pds.add_required_extension(VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME);
        pds.add_required_extension(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
#endif
    }

    void DenoiserApp::ApiBeforeDeviceBuilding(vkb::DeviceBuilder& deviceBuilder)
    {
        mTimelineFeature =
            VkPhysicalDeviceTimelineSemaphoreFeatures{.sType = VkStructureType::VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES, .timelineSemaphore = VK_TRUE};

        deviceBuilder.add_pNext(&mTimelineFeature);
    }

    void DenoiserApp::ApiInit()
    {
        // LoadEnvironmentMap(); Current Testing scene does not use an environment map
        LoadScene();
        ConfigureStages();
    }

    void DenoiserApp::LoadScene()
    {
        std::vector<std::string> scenePaths({
            // SCENE_PATH,
            DATA_DIR "/gltf/testbox/scene.gltf",
            // DATA_DIR "/intel-sponza/Main.1_Sponza/NewSponza_Main_glTF_002.gltf",
            // DATA_DIR "/gltf/lightandcam/lightAndCamera.gltf",
            // DATA_DIR "/intel-sponza/PKG_D.1_10k_Candles/NewSponza_4_Combined_glTF.gltf"
        });

        mScene = std::make_unique<foray::scene::Scene>(&mContext);
        foray::gltf::ModelConverter converter(mScene.get());
        for(const auto& path : scenePaths)
        {
            foray::gltf::ModelConverterOptions options{.FlipY = false};
            converter.LoadGltfModel(path, nullptr, options);
        }

        mScene->UpdateTlasManager();
        mScene->UseDefaultCamera(true);
        mScene->UpdateLightManager();

        auto camManager  = mScene->GetComponent<foray::scene::gcomp::CameraManager>();
        auto animManager = mScene->GetComponent<foray::scene::gcomp::AnimationManager>();
        if(!!animManager)
        {
            foray::scene::ncomp::Camera* camera = nullptr;
            for(auto& animation : animManager->GetAnimations())
            {
                animation.GetPlaybackConfig().ConstantDelta = 0.01666666667f;
                camera                                      = (!!camera) ? camera : animation.GetChannels()[0].Target->GetComponent<foray::scene::ncomp::Camera>();
            }

            if(!!camera)
            {
                camera->SetName("Animated Camera");
                camManager->SelectCamera(camera);
            }
        }

        for(int32_t i = 0; i < scenePaths.size(); i++)
        {
            const auto& path = scenePaths[i];
            const auto& log  = converter.GetBenchmark().GetLogs()[i];
            foray::logger()->info("Model Load \"{}\":\n{}", path, log.PrintPretty());
        }
    }

    void DenoiserApp::LoadEnvironmentMap()
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

    void DenoiserApp::ConfigureStages()
    {
        mGbufferStage.Init(&mContext, mScene.get());

        mRaytraycingStage.Init(&mContext, mScene.get());

        mDenoiseSemaphore.Create(&mContext);

        foray::core::ManagedImage::CreateInfo ci(VkImageUsageFlagBits::VK_IMAGE_USAGE_STORAGE_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                                     | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                                                 VkFormat::VK_FORMAT_R16G16B16A16_SFLOAT, mContext.GetSwapchainSize(), "Denoised Image");

        mDenoisedImage.Create(&mContext, ci);

        ActivateOrSwitchDenoiser();

        mOutputs      = {&mDenoisedImage, mRaytraycingStage.GetRtOutput()};
        mActiveOutput = mOutputs[mActiveOutputIndex];

        mImageToSwapchainStage.Init(&mContext, mActiveOutput);
        mImageToSwapchainStage.SetFlipY(true);

        mImguiStage.InitForSwapchain(&mContext);
        mImguiStage.AddWindowDraw([this]() { this->ImGui(); });

        RegisterRenderStage(&mGbufferStage);
        RegisterRenderStage(&mRaytraycingStage);
        RegisterRenderStage(&mBmfrDenoiser);
        RegisterRenderStage(&mASvgfDenoiser);
#ifdef ENABLE_OPTIX
        RegisterRenderStage(&mOptiXDenoiser);
#endif
        RegisterRenderStage(&mImguiStage);
        RegisterRenderStage(&mImageToSwapchainStage);
    }

#pragma endregion
#pragma region Runtime

    void DenoiserApp::ApiRender(foray::base::FrameRenderInfo& renderInfo)
    {
        ActivateOrSwitchDenoiser();
        ActivateOrSwitchOutput();

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

        // copy final image to swapchain
        mImageToSwapchainStage.RecordFrame(primaryCmdBuffer, renderInfo);

        // draw imgui windows
        mImguiStage.RecordFrame(primaryCmdBuffer, renderInfo);

        renderInfo.PrepareSwapchainImageForPresent(primaryCmdBuffer);

        primaryCmdBuffer.Submit();
    }

    void DenoiserApp::ApiFrameFinishedExecuting(uint64_t frameIndex)
    {
        namespace fs = std::filesystem;

        if(mDenoiserBenchmark.Exists() && mDenoiserBenchmark.LogQueryResults(frameIndex))
        {
            mDenoiserBenchmarkLog = mDenoiserBenchmark.GetLogs().back();
        }
        if(frameIndex >= BENCH_FRAMES)
        {
            foray::osi::Utf8Path savePath = foray::osi::Utf8Path("bench.csv").MakeAbsolute();
            std::fstream         out((fs::path)savePath, std::ios_base::openmode::_S_out);
            foray::Assert(out.is_open() && !out.bad(), "Write Benchmark failed");
            out << mDenoiserBenchmark.GetLogs().front().PrintCsvHeader();
            for(const foray::bench::BenchmarkLog& log : mDenoiserBenchmark.GetLogs())
            {
                out << log.PrintCsvLine();
            }
            out.flush();
            out.close();
            mRenderLoop.RequestStop();
        }
    }

    void DenoiserApp::ApiOnResized(VkExtent2D size)
    {
        mScene->InvokeOnResized(size);

        mDenoisedImage.Resize(size);
    }

    void DenoiserApp::ApiOnEvent(const foray::osi::Event* event)
    {
        mScene->InvokeOnEvent(event);
        // process events for imgui
        mImguiStage.ProcessSdlEvent(&(event->RawSdlEventData));
    }

    void DenoiserApp::ImGui()
    {
        foray::base::RenderLoop::FrameTimeAnalysis analysis = mRenderLoop.AnalyseFrameTimes();

        ImGui::Begin("window");
        if(analysis.Count > 0)
        {
            ImGui::Text("FPS: %f avg %f min", 1.f / analysis.AvgFrameTime, 1.f / analysis.MaxFrameTime);
        }

        {  // Output Switching
            std::string outputLabel(mActiveOutput->GetName());
            if(ImGui::BeginCombo("Output View", outputLabel.c_str()))
            {

                for(int32_t i = 0; i < mOutputs.size(); i++)
                {
                    bool        selected = mActiveOutputIndex == i;
                    std::string name(mOutputs[i]->GetName());
                    if(ImGui::Selectable(name.c_str(), selected))
                    {
                        this->mActiveOutputIndex = i;
                    }
                }

                ImGui::EndCombo();
            }
        }
        {  // Denoiser Switching
            std::string denoiserLabel = mActiveDenoiser->GetUILabel();
            if(ImGui::BeginCombo("Denoiser", denoiserLabel.c_str()))
            {

                for(int32_t i = 0; i < mDenoisers.size(); i++)
                {
                    bool        selected = mActiveDenoiserIndex == i;
                    std::string name     = mDenoisers[i]->GetUILabel();
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

        {
            foray::scene::gcomp::CameraManager* camManager = mScene->GetComponent<foray::scene::gcomp::CameraManager>();

            if(!!camManager)
            {
                std::vector<foray::scene::ncomp::Camera*> cameras;
                camManager->GetCameras(cameras);
                if(cameras.size() > 0)
                {
                    uint idx = 0;
                    for(auto camera : cameras)
                    {
                        if(camera == camManager->GetSelectedCamera())
                        {
                            break;
                        }
                        idx++;
                    }
                    std::string cameraLabel = cameras[idx]->GetName();
                    if(cameraLabel.size() == 0)
                    {
                        cameraLabel = fmt::format("Camera #{}", idx);
                    }
                    if(ImGui::BeginCombo("Camera", cameraLabel.c_str()))
                    {

                        for(int32_t i = 0; i < cameras.size(); i++)
                        {
                            bool        selected    = idx == i;
                            std::string cameraLabel = cameras[i]->GetName();
                            if(cameraLabel.size() == 0)
                            {
                                cameraLabel = fmt::format("Camera #{}", i);
                            }
                            if(ImGui::Selectable(cameraLabel.c_str(), selected))
                            {
                                camManager->SelectCamera(cameras[i]);
                                mActiveDenoiser->IgnoreHistoryNextFrame();
                            }
                        }

                        ImGui::EndCombo();
                    }
                }
            }
        }

        ImGui::End();
    }

#pragma endregion
#pragma region Destroy

    void DenoiserApp::ApiDestroy()
    {
        mScene->Destroy();
        mScene = nullptr;
        mASvgfDenoiser.Destroy();
        mBmfrDenoiser.Destroy();
#ifdef ENABLE_OPTIX
        mOptiXDenoiser.Destroy();
#endif
        mNrdDenoiser.Destroy();
        mGbufferStage.Destroy();
        mImguiStage.Destroy();
        mRaytraycingStage.Destroy();
        mEnvMap.Destroy();
        mDenoiseSemaphore.Destroy();
        mDenoisedImage.Destroy();
    }

#pragma endregion
#pragma region Denoiser& Output selection

    void DenoiserApp::ActivateOrSwitchDenoiser()
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

    void lUpdateOutput(std::map<std::string_view, foray::core::ManagedImage*>& map, foray::stages::RenderStage& stage, const std::string_view name)
    {
        map[name] = stage.GetImageOutput(name);
    }

    void DenoiserApp::ActivateOrSwitchOutput()
    {
        if(mActiveOutput == mOutputs[mActiveOutputIndex])
        {
            return;
        }
        vkDeviceWaitIdle(mDevice);
        mActiveOutput = mOutputs[mActiveOutputIndex];
        mImageToSwapchainStage.SetSrcImage(mActiveOutput);
    }

#pragma endregion
}  // namespace denoise
