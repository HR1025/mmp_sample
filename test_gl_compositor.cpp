#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <deque>
#include <Poco/Stopwatch.h>
#include <Poco/Util/Application.h>
#include <Poco/Util/HelpFormatter.h>
#include <string>
#include <thread>
#include <vector>

#include "Common/Promise.h"
#include "Common/AbstractLogger.h"
#include "Common/LogMessage.h"
#include "Common/ThreadPool.h"
#include "Common/DmaHeapAllocateMethod.h"
#include "GLCommon.h"
#include "GPU/GL/GLDrawContex.h"
#include "GPU/Windows/AbstractWindows.h"
#include "GPU/Windows/WindowFactory.h"
#include "GPU/PG/AbstractSceneItem.h"
#include "GPU/PG/AbstractSceneLayer.h"
#include "GPU/PG/AbstractSceneCompositor.h"
#include "GPU/PG/Utility/CommonUtility.h"
#include "Codec/StreamFrame.h"
#include "Codec/CodecConfig.h"
#include "Codec/CodecFactory.h"
#include "Common/ImmutableVectorAllocateMethod.h"

#include "AbstractDisplay.h"
#include "SampleUtils.h"


using namespace Mmp;
using namespace Poco::Util;


/**
 * @sa Core/Extension/poco/Util/samples/SampleApp/src/SampleApp.cpp 
 */
class App : public Application
{
public:
    App();
public:
    void defineOptions(OptionSet& options) override;
protected:
    void Initialize();
    void Uninitialize();
    void defineProperty(const std::string& def);
    int main(const ArgVec& args);
private:
    void HandleHelp(const std::string& name, const std::string& value);
    void HandleBackend(const std::string& name, const std::string& value);
    void HandleSplitNum(const std::string& name, const std::string& value);
    void HandleFps(const std::string& name, const std::string& value);
    void HandleMerryGoRound(const std::string& name, const std::string& value);
    void HandleDuration(const std::string& name, const std::string& value);
    void displayHelp();
public:
    GPUBackend backend;
    size_t     splitNum;
    uint64_t   duration;
    uint32_t   fps;
    bool       merryGoRound;
private: /* gpu */
    std::atomic<bool> _gpuInited;
    std::thread _renderThread;
    AbstractWindows::ptr _window;
    GLDrawContex::ptr    _draw;
};

App::App()
{
    backend = GPUBackend::OPENGL;
    splitNum = 4;
    fps = 60;
    merryGoRound = false;
    duration = 30;
}

void App::displayHelp()
{
    AbstractLogger::LoggerSingleton()->Enable(AbstractLogger::Direction::CONSLOE);
    std::stringstream ss;
    HelpFormatter helpFormatter(options());
    helpFormatter.setWidth(1024);
    helpFormatter.setCommand(commandName());
    helpFormatter.setUsage("OPTIONS");
    helpFormatter.setHeader("Simple program to test nxn Compositor using MMP-Core.");
    helpFormatter.format(ss);
    MMP_LOG_INFO << ss.str();
    exit(0);
}

void App::HandleBackend(const std::string& name, const std::string& value)
{
    backend = GetGPUBackend(value);
}

void App::HandleSplitNum(const std::string& name, const std::string& value)
{
    splitNum = std::stoi(value);
    splitNum = std::min(splitNum, (size_t)8);
    splitNum = std::max(splitNum, (size_t)2);
}

void App::HandleFps(const std::string& name, const std::string& value)
{
    fps = std::stoi(value);
    fps = std::min(fps, (uint32_t)240);
    fps = std::max(fps, (uint32_t)1);
}

void App::HandleHelp(const std::string& name, const std::string& value)
{
    displayHelp();
}

void App::HandleMerryGoRound(const std::string& name, const std::string& value)
{
    if (value == "true")
    {
        merryGoRound = true;
    }
}

void App::HandleDuration(const std::string& name, const std::string& value)
{
    duration = std::stoi(value);
    duration = std::max(duration, (uint64_t)1);
}

void App::Initialize()
{
    ThreadPool::ThreadPoolSingleton()->Init();
    Codec::CodecConfig::Instance()->Init();
    {
        _renderThread = std::thread([this]() -> void
        {
            GLDrawContex::SetGPUBackendType(backend);
            _window = WindowFactory::DefaultFactory().createWindow(WindowFactory::DefaultFactory().GetGuessClassName(backend));
            _window->SetRenderMode(false);
            _window->Open();
            _window->BindRenderThread(true);
            _draw = GLDrawContex::Instance();
            _draw->SetWindows(_window);
            _gpuInited = true;
            _draw->ThreadStart();
            while (true)
            {
                GpuTaskStatus status;
                status = _draw->ThreadFrame();
                if (status == GpuTaskStatus::EXIT)
                {
                    break;
                }
                else if (status == GpuTaskStatus::PRESENT)
                {
                    _window->Swap();
                }
                else
                {
                    continue;
                }
            };
            _draw->ThreadEnd();
            _window->BindRenderThread(false);
            _window->Close();
        });
        while (!_gpuInited)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    }
}

void App::Uninitialize()
{
    Application::uninitialize();
    {
        _draw->ThreadStop();
        _renderThread.join();
    }
    Codec::CodecConfig::Instance()->Uninit();
    ThreadPool::ThreadPoolSingleton()->Uninit();
}

void App::defineOptions(OptionSet& options)
{
    Application::defineOptions(options);

    std::string backendDescription = "gpu process uint, available value is: ";
    {
        std::vector<GPUBackend> backends = GLDrawContex::GetAvailableBackendType();
        for (auto& backend : backends)
        {
            backendDescription += GPUBackendToStr(backend) + " ";
        }
    }

    options.addOption(Option("help", "h", "")
        .required(false)
        .repeatable(false)
        .callback(OptionCallback<App>(this, &App::HandleHelp))
    );
    options.addOption(Option("backend", "b", backendDescription)
        .required(false)
        .repeatable(false)
        .argument("[type]")
        .callback(OptionCallback<App>(this, &App::HandleBackend))
    );
    options.addOption(Option("split_num", "sn", "default(4), 2~8, 2*2, 3*3, n*n")
        .required(false)
        .repeatable(false)
        .argument("[num]")
        .callback(OptionCallback<App>(this, &App::HandleSplitNum))
    );
    options.addOption(Option("frame_per_second", "fps", "default(60), 1~240(overload print warn log)")
        .required(false)
        .repeatable(false)
        .argument("[num]")
        .callback(OptionCallback<App>(this, &App::HandleFps))
    );
    options.addOption(Option("merry_go_around", "mgr", "default(false), true or false")
        .required(false)
        .repeatable(false)
        .argument("[switch]")
        .callback(OptionCallback<App>(this, &App::HandleMerryGoRound))
    );
    options.addOption(Option("duration", "d", "default(30) second")
        .required(false)
        .repeatable(false)
        .argument("[num]")
        .callback(OptionCallback<App>(this, &App::HandleDuration))
    );
    
}

void App::defineProperty(const std::string& def)
{
    std::string name;
    std::string value;
    std::string::size_type pos = def.find('=');
    if (pos != std::string::npos)
    {
        name.assign(def, 0, pos);
        value.assign(def, pos + 1, def.length() - pos);
    }
    else name = def;
    config().setString(name, value);
}

/********************************************************* TEST(BEGIN) *****************************************************/

int App::main(const ArgVec& args)
{
    AbstractLogger::LoggerSingleton()->Enable(AbstractLogger::Direction::CONSLOE);
    MMP_LOG_INFO << "test_gl_compositor config";
    MMP_LOG_INFO << "-- backend : " << backend;
    MMP_LOG_INFO << "-- window : " << WindowFactory::DefaultFactory().GetGuessClassName(backend);
    MMP_LOG_INFO << "-- split_num : " << splitNum;
    MMP_LOG_INFO << "-- fps : " << fps;
    MMP_LOG_INFO << "-- merry_go_round : " << (merryGoRound ? "true" : "false");
    MMP_LOG_INFO << "-- duration : " << duration << " second";
    Initialize();

    AbstractPicture::ptr sceneA = GetFrame1920x1080A();
    AbstractPicture::ptr sceneB = GetFrame1920x1080B();
    AbstractDisplay::ptr display = AbstractDisplay::Create();
    PixelsInfo info = {1920, 1080, 8, PixelFormat::RGBA8888};
    AbstractPicture::ptr fb = std::make_shared<NormalPicture>(info);
    if (display)
    {
        display->Init();
        display->Open(info);
    }
    /******************************* PluginTransitionTest(BEGIN) ********************************/
    Texture::ptr imageA;
    Texture::ptr imageB;
    Texture::ptr canvas;
    Texture::ptr framebuffer;
    {
        imageA = Gpu::Create2DTextures(GLDrawContex::Instance(), sceneA->info)[0];
        imageB = Gpu::Create2DTextures(GLDrawContex::Instance(), sceneB->info)[0];
        canvas = Gpu::Create2DTextures(GLDrawContex::Instance(), sceneA->info, "Canvas", GlTextureFlags::TEXTURE_USE_FOR_RENDER)[0];
        framebuffer = Gpu::Create2DTextures(GLDrawContex::Instance(), sceneA->info, "Framebuffer", GlTextureFlags::TEXTURE_USE_FOR_RENDER)[0];
        Gpu::Update2DTextures(GLDrawContex::Instance(), std::vector<Texture::ptr>({imageA}), sceneA);
        Gpu::Update2DTextures(GLDrawContex::Instance(), std::vector<Texture::ptr>({imageB}), sceneB);
    }
    
    Promise<void>::ptr lastDraw;
    auto equalSplitScreen = [&](size_t count, size_t fps, uint64_t duration) -> void
    {
        Gpu::AbstractSceneLayer::ptr layer = Gpu::AbstractSceneLayer::Create();
        std::vector<Gpu::SceneItemParam> params;
        std::vector<Gpu::AbstractSceneItem::ptr> items;
        size_t curItem = 0;
        // Init params and items
        for (size_t col=0; col<count; col++)
        {
            for (size_t row=0; row<count; row++)
            {
                {
                    Gpu::SceneItemParam param = {};
                    param.location = NormalizedPoint(1.0f/count * row, 1.0f/count * col);
                    param.area = NormalizedRect(1.0f/count, 1.0f/count);
                    params.push_back(param);
                }
                {
                    Gpu::AbstractSceneItem::ptr item = Gpu::AbstractSceneItem::Create();
                    Texture::ptr image;
                    if ((col+1) % 2 == 1 && (row+1) % 2 == 1)
                    {
                        image = imageA;
                    }
                    else if ((col+1) % 2 == 0 && (row+1) % 2 == 0)
                    {
                        image = imageA;
                    }
                    else
                    {
                        image = imageB;
                    }
                    item->UpdateImage(image);
                    items.push_back(item);
                }
            }
        }
        // Add items to layer
        for (size_t col=0; col<count; col++)
        {
            for (size_t row=0; row<count; row++)
            {
                items[curItem]->SetParam(params[curItem]);
                layer->AddSceneItem(std::string() + "item" + "_" + std::to_string(row) + "_" + std::to_string(col), items[curItem]);
                curItem++;
            }
        }
        {
            Gpu::SceneLayerParam param = {};
            param.strategy = Gpu::SceneRenderStrategy::Keep;
            layer->SetParam(param);
            layer->UpdateCanvas(canvas);
        }
        Poco::Timestamp stamp;
        uint64_t curDrawTime = 0;
        uint64_t itemParamOffset = 0;
        for (uint32_t i=0; i< fps * duration; i++)
        {
            stamp.update();
            if (merryGoRound && (curDrawTime * 2 % fps == 0))
            {
                curItem = 0;
                for (size_t col=0; col<count; col++)
                {
                    for (size_t row=0; row<count; row++)
                    {
                        items[curItem]->SetParam(params[(curItem + itemParamOffset) % (count * count)]);
                        curItem++;
                    }
                }
                itemParamOffset++;
            }
            layer->Draw(framebuffer);
            if (lastDraw)
            {
                lastDraw->Wait();
            }
            Gpu::Copy2DTexturesToMemory(GLDrawContex::Instance(), std::vector<Texture::ptr>({framebuffer}), fb);
            if (stamp.elapsed()/1000 > 1000 / fps)
            {
                MMP_LOG_WARN << "overload, cost time is: " << stamp.elapsed()/1000 << " ms";
            }
            lastDraw = std::make_shared<Promise<void>>([display, fb, info]()
            {
                if (display)
                {
                    display->UpdateWindow((const uint32_t*)(fb->GetData()), info);
                }
            });
            ThreadPool::ThreadPoolSingleton()->Commit(lastDraw);
            curDrawTime++;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000 / fps - stamp.elapsed()/1000));
        }
        layer.reset();
        if (lastDraw)
        {
            lastDraw->Wait();
        }
    };

    equalSplitScreen(splitNum, fps, duration);

    /******************************* PluginTransitionTest(END) ********************************/
    if (display)
    {
        display->Close();
        display->UnInit();
    }

    Uninitialize();
    return 0;
}

/********************************************************* TEST(END) *****************************************************/

POCO_APP_MAIN(App)