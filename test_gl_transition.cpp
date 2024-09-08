#include <deque>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <algorithm>

#include <Poco/Stopwatch.h>
#include <Poco/Util/Application.h>
#include <Poco/Util/HelpFormatter.h>

#include "Common/Promise.h"
#include "Common/AbstractLogger.h"
#include "Common/LogMessage.h"
#include "Common/ThreadPool.h"
#include "Common/DmaHeapAllocateMethod.h"
#include "Common/ImmutableVectorAllocateMethod.h"
#include "GPU/GL/GLCommon.h"
#include "GPU/GL/GLDrawContex.h"
#include "GPU/Windows/WindowFactory.h"
#include "GPU/Windows/AbstractWindows.h"
#include "GPU/PG/TransitionFactory.h"
#include "GPU/PG/Utility/CommonUtility.h"
#include "Codec/CodecFactory.h"
#include "Codec/CodecConfig.h"

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
    void HandleTransition(const std::string& name, const std::string& value);
    void HandleDuration(const std::string& name, const std::string& value);
    void displayHelp();
public:
    std::string transitionName;
    GPUBackend backend;
    uint64_t   duration;
    uint32_t   fps;
private: /* gpu */
    std::atomic<bool> _gpuInited;
    std::thread _renderThread;
    AbstractWindows::ptr _window;
    GLDrawContex::ptr    _draw;
};

App::App()
{
    backend = GPUBackend::OPENGL;
    fps = 60;
    duration = 1;
    transitionName = "DirectionalTransition";
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
    ss << "Available transition:" << std::endl;
    ss << "\t" << "BowTieHorizontalTransition, BowTieVerticalTransition, DirectionalTransition, GlitchMemoriesTransition" << std::endl;
    ss << "\t" << "InvertedPageCurlTransition, LinearBlurTransition, PolkaDotsCurtainTransition, SimpleZoomTransition" << std::endl;
    ss << "\t" << "StereoViewerTransition, WaterDropTransition, WindowsliceTransition, CircleCropTransition" << std::endl;
    ss << "\t" << "ColourDistanceTransition, DirectionalwarpTransition, MorphTransition, PerlinTransition" << std::endl;
    ss << "\t" << "SwirlTransition, CannabisleafTransition, ButterflyWaveScrawlerTransition, CrazyParametricFunTransition" << std::endl;
    ss << "\t" << "CrosshatchTransition, CrossZoomTransition, DreamyTransition, KaleidoscopeTransition" << std::endl;
    ss << "\t" << "GridFlipTransition, RadialTransition, ZoomInCirclesTransition, AngularTransition" << std::endl;
    ss << "\t" << "BurnTransition, CircleopenTransition, CircleTransition, ColorphaseTransition" << std::endl;
    ss << "\t" << "DoomScreenTransitionTransition, DreamyZoomTransition, GlitchDisplaceTransition, HexagonalizeTransition" << std::endl;
    ss << "\t" << "PinwheelTransition, RippleTransition, WindowblindsTransition, CrosswarpTransition" << std::endl;
    ss << "\t" << "CubeTransition, DirectionalwipeTransition, DoorwayTransition, FadecolorTransition" << std::endl;
    ss << "\t" << "FadegrayscaleTransition, FadeTransition, FlyeyeTransition, HeartTransition" << std::endl;
    ss << "\t" << "PixelizeTransition, PolarFunctionTransition, RandomsquaresTransition, SquareswireTransition" << std::endl;
    ss << "\t" << "SqueezeTransition, SwapTransition, WindTransition" << std::endl;
    ss << "(see `https://gl-transitions.com/gallery` for more information)" << std::endl;
    MMP_LOG_INFO << ss.str();
    exit(0);
}

void App::HandleBackend(const std::string& name, const std::string& value)
{
    backend = GetGPUBackend(value);
}

void App::HandleTransition(const std::string& name, const std::string& value)
{
    transitionName = value;
}

void App::HandleHelp(const std::string& name, const std::string& value)
{
    displayHelp();
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
    options.addOption(Option("duration", "d", "default(1) second")
        .required(false)
        .repeatable(false)
        .argument("[num]")
        .callback(OptionCallback<App>(this, &App::HandleDuration))
    );
    options.addOption(Option("transition", "t", "transition type, available value see -h")
        .required(false)
        .repeatable(false)
        .argument("[name]")
        .callback(OptionCallback<App>(this, &App::HandleTransition))
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
    MMP_LOG_INFO << "-- transition : " << transitionName;
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
    Texture::ptr framebuffer;
    {
        imageA = Gpu::Create2DTextures(GLDrawContex::Instance(), sceneA->info)[0];
        imageB = Gpu::Create2DTextures(GLDrawContex::Instance(), sceneB->info)[0];
        framebuffer = Gpu::Create2DTextures(GLDrawContex::Instance(), sceneA->info, "Framebuffer", GlTextureFlags::TEXTURE_USE_FOR_RENDER)[0];
        Gpu::Update2DTextures(GLDrawContex::Instance(), std::vector<Texture::ptr>({imageA}), sceneA);
        Gpu::Update2DTextures(GLDrawContex::Instance(), std::vector<Texture::ptr>({imageB}), sceneB);
    }
    
    Promise<void>::ptr lastDraw;
    Poco::Timestamp stamp;
    uint64_t curDrawTime = 0;
    uint64_t itemParamOffset = 0;
    Gpu::AbstractTransition::ptr transition = Gpu::TransitionFactory::DefaultFactory().CreateTransition(transitionName);
    Gpu::AbstractTransitionParams::ptr params = std::make_shared<Gpu::AbstractTransitionParams>();
    if (!transition)
    {
        MMP_LOG_WARN << "Unsupport transition " << transitionName;
        displayHelp();
        return 0;
    }
    for (uint32_t i=0; i< fps * duration; i++)
    {
        stamp.update();
        params->progress = (float)i / (fps * duration);
        transition->Transition(imageA, imageB, framebuffer, params);
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
    if (lastDraw)
    {
        lastDraw->Wait();
    }
    transition.reset();
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