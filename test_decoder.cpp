#include <fstream>
#include <queue>
#include <mutex>
#include <thread>
#include <algorithm>
#include <cstring>
#include <Poco/Stopwatch.h>
#include <Poco/Util/Application.h>
#include <Poco/Util/HelpFormatter.h>

#include "Common/Promise.h"
#include "Common/AbstractLogger.h"
#include "Common/LogMessage.h"
#include "Common/ThreadPool.h"
#include "Codec/StreamPack.h"
#include "Codec/StreamFrame.h"
#include "Codec/CodecConfig.h"
#include "Codec/CodecFactory.h"
#include "Common/ImmutableVectorAllocateMethod.h"
#include "AbstractDisplay.h"
#include "Stream/StreamPuller.h"
#include "Stream/StreamPullerFactory.h"
#include "Stream/StreamObserver.h"
#include "Stream/StreamCommon.h"

using namespace Mmp;
using namespace Poco::Util;

constexpr uint32_t kBufSize = 1024 * 1024;


class H26XFileByteReader
{
public:
    explicit H26XFileByteReader(const std::string& path);
    ~H26XFileByteReader();
public:
    Codec::StreamPack::ptr GetNalUint();
public:
    size_t Read(void* data, size_t bytes);
    bool Seek(size_t offset);
    size_t Tell();
    bool eof();
private:
    std::ifstream _ifs;
private:
    uint8_t* _buf;
    size_t _offset;
    uint32_t _cur;
    uint32_t _len;
};

Codec::StreamPack::ptr H26XFileByteReader::GetNalUint()
{
    std::vector<uint8_t> bufs;
    bufs.reserve(1024 * 1024);
    uint32_t next_24_bits = 0;
    bool isFirst = true;
    while (!(next_24_bits == 0x000001 && !isFirst))
    {
        if (next_24_bits == 0x000001)
        {
            isFirst = false;
            bufs.push_back(0);
            bufs.push_back(0);
            bufs.push_back(0);
            bufs.push_back(1);
        }
        uint8_t byte = 0;
        if (Read(&byte, 1) != 1 && eof())
        {
            return nullptr;
        }
        if (!isFirst)
        {
            bufs.push_back(byte);
        }
        next_24_bits = (next_24_bits << 8) | byte;
        next_24_bits = next_24_bits & 0xFFFFFF;
    }
    Seek(Tell() - 3);
    if (bufs.size() >= 3)
    {
        bufs.resize(bufs.size() - 3);
        if (!bufs.empty() && bufs[bufs.size()-1] == 0)
        {
            bufs.pop_back();
        }
    }
    std::shared_ptr<ImmutableVectorAllocateMethod<uint8_t>> alloc = std::make_shared<ImmutableVectorAllocateMethod<uint8_t>>();
    alloc->container.swap(bufs);
    return std::make_shared<Codec::StreamPack>(Codec::CodecType::H264, alloc->container.size(), alloc);
}

H26XFileByteReader::H26XFileByteReader(const std::string& path)
{
    _ifs.open(path, std::ios::in | std::ios::binary);
    if (!_ifs.is_open())
    {
        assert(false);
        exit(255);
    }
    _buf = new uint8_t[kBufSize];
    _offset = (uint32_t)_ifs.tellg();
    _ifs.read((char*)_buf, kBufSize);
    _cur = 0;
    _len = (uint32_t)_ifs.gcount();
}

H26XFileByteReader::~H26XFileByteReader()
{
    delete[] _buf;
    _ifs.close();
}

size_t H26XFileByteReader::Read(void* data, size_t bytes)
{
    if (_cur + bytes <= _len)
    {
        memcpy(data, _buf + _cur, bytes);
        _cur += (uint32_t)bytes;
        return bytes;
    }
    else if (_ifs.eof())
    {
        memcpy(data, _buf + _cur, _len -  _cur);
        return _len -  _cur;
    }
    else
    {
        _offset = (size_t)_ifs.tellg();
        _ifs.read((char*)_buf, kBufSize);
        _cur = 0;
        _len = (uint32_t)_ifs.gcount();
        if (_len == 0) /* eof */
        {
            return 0;
        }
        else
        {
            return Read(data, bytes);
        }
    }
}

bool H26XFileByteReader::Seek(size_t offset)
{
    if (offset < _offset)
    {
        _ifs.seekg(offset);
        _offset = (size_t)_ifs.tellg();
        _ifs.read((char*)_buf, kBufSize);
        _cur = 0;
        _len = (uint32_t)_ifs.gcount(); 
        return _offset == offset;
    }
    else if (offset > _offset + kBufSize)
    {
        _ifs.seekg(offset);
        _offset = (size_t)_ifs.tellg();
        _ifs.read((char*)_buf, kBufSize);
        _cur = 0;
        _len = (uint32_t)_ifs.gcount(); 
        return _offset == offset;
    }
    else
    {
        _cur = (uint32_t)(offset - _offset);
        return true;
    }
}

size_t H26XFileByteReader::Tell()
{
    return _offset + _cur;
}

bool H26XFileByteReader::eof()
{
    return _len == 0 || (_ifs.eof() && _cur == _len);
}

/**
 * @brief Simple StreamObserver implementation for test
 */
class TestStreamObserver : public StreamObserver
{
public:
    TestStreamObserver() = default;
    ~TestStreamObserver() = default;
};

/**
 * @brief Convert Mmp::CodecType to Mmp::Codec::CodecType
 */
static Codec::CodecType ConvertCodecType(CodecType type)
{
    switch (type)
    {
        case CodecType::PNG:
            return Codec::CodecType::PNG;
        case CodecType::H264:
            return Codec::CodecType::H264;
        case CodecType::H265:
            return Codec::CodecType::H265;
        case CodecType::VP8:
            return Codec::CodecType::VP8;
        case CodecType::VP9:
            return Codec::CodecType::VP9;
        case CodecType::AV1:
            return Codec::CodecType::AV1;
        default:
            // Default to H264 for unknown types
            return Codec::CodecType::H264;
    }
}

/**
 * @sa MMP-Core/Extension/poco/Util/samples/SampleApp/src/SampleApp.cpp 
 */
class App : public Application
{
public:
    App();
public:
    void defineOptions(OptionSet& options) override;
protected:
    void initialize(Application& self);
    void uninitialize();
    void reinitialize(Application& self);
    void defineProperty(const std::string& def);
    int main(const ArgVec& args);
private:
    void HandleHelp(const std::string& name, const std::string& value);
    void HandleCodecName(const std::string& name, const std::string& value);
    void HandleInput(const std::string& name, const std::string& value);
    void HandleShow(const std::string& name, const std::string& value);
    void HandleFps(const std::string& name, const std::string& value);
    void HandlePullerName(const std::string& name, const std::string& value);
    bool IsNetworkUrl(const std::string& input);
    bool IsMp4File(const std::string& filepath);
    void displayHelp();
public:
    std::string              decoderClassName;
    std::string              inputPath;  // Can be file path or network URL
    std::string              pullerClassName;
    bool                     show;
    uint64_t                 fps;
    size_t                   loopTime;
};

App::App()
{
    show = true;
    fps = 30;
    loopTime = 0;
    pullerClassName = "";
}

void App::displayHelp()
{
    AbstractLogger::LoggerSingleton()->Enable(AbstractLogger::Direction::CONSLOE);
    std::stringstream ss;
    HelpFormatter helpFormatter(options());
    helpFormatter.setWidth(1024);
    helpFormatter.setCommand(commandName());
    helpFormatter.setUsage("OPTIONS");
    helpFormatter.setHeader("Simple program to test decoder using MMP-Core.");
    helpFormatter.format(ss);
    ss << std::endl;
    ss << "Available Decoder Info" << std::endl;
    std::vector<Codec::CodecDescription> descriptions = Codec::DecoderFactory::DefaultFactory().GetDecoderDescriptions();
    for (auto& description : descriptions)
    {
        ss << "-- CodecType(" << description.codecType << ") CodecProcessType(" << description.processType 
           << ") CodecVendorType("<< description.vendorType << ") "
           << "name(" << description.name << ") description(" << description.description << ")" << std::endl;;
    }
    MMP_LOG_INFO << ss.str();
    exit(0);
}

void App::HandleHelp(const std::string& name, const std::string& value)
{
    displayHelp();
}

void App::HandleCodecName(const std::string& name, const std::string& value)
{
    decoderClassName = value;
}

void App::HandleShow(const std::string& name, const std::string& value)
{
    if (value == "true")
    {
        show = true;
    }
    else if (value == "false")
    {
        show = false;
    }
}

void App::HandleFps(const std::string& name, const std::string& value)
{
    fps = std::stoi(value);
}

void App::HandleInput(const std::string& name, const std::string& value)
{
    inputPath = value;
}

void App::HandlePullerName(const std::string& name, const std::string& value)
{
    pullerClassName = value;
}

bool App::IsNetworkUrl(const std::string& input)
{
    // Check if input contains protocol separator (://)
    return input.find("://") != std::string::npos ||
           input.find("rtsp://") == 0 ||
           input.find("rtmp://") == 0 ||
           input.find("http://") == 0 ||
           input.find("https://") == 0 ||
           input.find("ndi://") == 0 ||
           input.find("srt://") == 0;
}

bool App::IsMp4File(const std::string& filepath)
{
    // Check file extension (case insensitive)
    std::string lowerPath = filepath;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
    return lowerPath.length() >= 4 && 
           lowerPath.substr(lowerPath.length() - 4) == ".mp4";
}

void App::initialize(Application& self)
{
    loadConfiguration(); 
    ThreadPool::ThreadPoolSingleton()->Init();
    Application::initialize(self);
    Codec::CodecConfig::Instance()->Init();
    AbstractLogger::LoggerSingleton()->Enable(AbstractLogger::Direction::CONSLOE);
}

void App::uninitialize()
{
    Codec::CodecConfig::Instance()->Uninit();
    Application::uninitialize();
    ThreadPool::ThreadPoolSingleton()->Uninit();
}

void App::reinitialize(Application& self)
{
    Application::reinitialize(self);
}

void App::defineOptions(OptionSet& options)
{
    Application::defineOptions(options);

    options.addOption(Option("help", "h", "")
        .required(false)
        .repeatable(false)
        .callback(OptionCallback<App>(this, &App::HandleHelp))
    );
    options.addOption(Option("codec_name", "codec", "decoder class name (see help name field)")
        .required(true)
        .repeatable(false)
        .argument("[name]")
        .callback(OptionCallback<App>(this, &App::HandleCodecName))
    );
    options.addOption(Option("input", "i", "")
        .required(true)
        .repeatable(false)
        .argument("[filepath]")
        .callback(OptionCallback<App>(this, &App::HandleInput))
    );
    options.addOption(Option("display", "display", "default true")
        .required(false)
        .repeatable(false)
        .argument("[show]")
        .callback(OptionCallback<App>(this, &App::HandleShow))
    );
    options.addOption(Option("fps", "fps", "default 30")
        .required(false)
        .repeatable(false)
        .argument("[num]")
        .callback(OptionCallback<App>(this, &App::HandleFps))
    );
    options.addOption(Option("puller", "p", "puller class name for network/MP4 mode (e.g., ZLMStreamPuller, FileStreamPuller, NDIStreamMediaPuller)")
        .required(false)
        .repeatable(false)
        .argument("[name]")
        .callback(OptionCallback<App>(this, &App::HandlePullerName))
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
    AbstractDisplay::ptr display;
    Codec::AbstractDecoder::ptr decoder = Codec::DecoderFactory::DefaultFactory().CreateDecoder(decoderClassName);
    {
        bool isNetwork = IsNetworkUrl(inputPath);
        bool isMp4 = !isNetwork && IsMp4File(inputPath);
        
        MMP_LOG_INFO << "Decoder config";
        MMP_LOG_INFO << "-- codec name : " << decoderClassName;
        MMP_LOG_INFO << "-- input : " << inputPath;
        if (isNetwork)
        {
            MMP_LOG_INFO << "-- mode : network";
            MMP_LOG_INFO << "-- puller class : " << (pullerClassName.empty() ? "auto" : pullerClassName);
        }
        else if (isMp4)
        {
            MMP_LOG_INFO << "-- mode : file (MP4)";
            MMP_LOG_INFO << "-- puller class : FileStreamPuller";
        }
        else
        {
            MMP_LOG_INFO << "-- mode : file (raw H.26X)";
        }
        MMP_LOG_INFO << "-- display : " << (show ? "true" : "false");
        MMP_LOG_INFO << "-- fps : " << fps;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (!decoder)
    {
        MMP_LOG_ERROR << "Unsupport decoder, name is: " << decoderClassName;
        displayHelp();
        return 0;
    }
    decoder->Init();
    decoder->Start();

    if (show)
    {
        display = AbstractDisplay::Create();
    }
    if (display)
    {
        display->Init();
    }

    /***************************************** 渲染线程(Begin) ****************************************/
    std::atomic<bool> running(true);
    std::atomic<bool> sync(false);
    Promise<void>::ptr displayTask = std::make_shared<Promise<void>>([&]()
    {
        uint64_t intervalMs = 1000 / fps;
        Poco::Stopwatch sw;
        sw.start();
        bool first = true;
        while (running)
        {
            AbstractFrame::ptr frame;
            if (decoder->Pop(frame))
            {
                MMP_LOG_INFO << "AbstractDisplay Pop";
                Codec::StreamFrame::ptr streamFrame = std::dynamic_pointer_cast<Codec::StreamFrame>(frame);
                if (display && first)
                {
                    display->Open(streamFrame->info);
                    first = false;
                }
                if (display)
                {
                    display->UpdateWindow((const uint32_t*)streamFrame->GetData(0), streamFrame->info);
                    if (intervalMs > sw.elapsed() / 1000)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs - sw.elapsed() / 1000));
                    }
                    else
                    {
                        MMP_LOG_WARN << "Process too slow!!!";
                    }                    
                    sw.restart();
                }
            }
            sync = true;
        } 
    });
    ThreadPool::ThreadPoolSingleton()->Commit(displayTask);
    /***************************************** 渲染线程(End) ****************************************/

    bool isNetwork = IsNetworkUrl(inputPath);
    bool isMp4 = !isNetwork && IsMp4File(inputPath);

    if (isNetwork || isMp4)
    {
        /*********************************** 使用 StreamPuller (网络或MP4文件)(Begin) ******************************/
        // Create StreamObserver
        StreamObserver::ptr observer = std::make_shared<TestStreamObserver>();
        
        // Create StreamPuller
        StreamPuller::ptr puller;
        if (!pullerClassName.empty())
        {
            puller = StreamPullerFactory::DefaultFactory().CreatePuller(pullerClassName, observer);
        }
        else
        {
            if (isMp4)
            {
                // MP4 file uses FileStreamPuller
                puller = StreamPullerFactory::DefaultFactory().CreatePuller("FileStreamPuller", observer);
            }
            else if (isNetwork)
            {
                // Auto detect network puller based on URL
                if (inputPath.find("ndi://") != std::string::npos)
                {
#if defined (USE_NDI)
                    puller = StreamPullerFactory::DefaultFactory().CreatePuller("NDIStreamMediaPuller", observer);
#else
                    MMP_LOG_ERROR << "NDI support not enabled";
                    decoder->Stop();
                    decoder->Uninit();
                    return 1;
#endif
                }
                else
                {
                    // Network stream uses ZLMStreamPuller
                    puller = StreamPullerFactory::DefaultFactory().CreatePuller("ZLMStreamPuller", observer);
                }
            }
        }

        if (!puller)
        {
            MMP_LOG_ERROR << "Failed to create puller";
            decoder->Stop();
            decoder->Uninit();
            return 1;
        }

        // Configure puller
        MediaPullConfig config;
        config.streamId = "test_stream";
        if (isMp4)
        {
            config.type = MediaPullType::File;
            // FileStreamPuller gets file path from playUrls
            config.playUrls.push_back(inputPath);
        }
        else
        {
            config.type = MediaPullType::Network;
            config.playUrls.push_back(inputPath);
        }
        if (!pullerClassName.empty())
        {
            config.className = pullerClassName;
        }
        puller->SetMediaPullConfig(config);

        // Add data callback
        std::mutex packMtx;
        std::queue<Codec::StreamPack::ptr> packQueue;
        puller->AddDataCallback("test_decoder", [&packMtx, &packQueue](StreamPack::ptr pack)
        {
            if (pack)
            {
                // Convert StreamPack (Common) to Codec::StreamPack
                Codec::CodecType codecType = ConvertCodecType(pack->type);
                Codec::StreamPack::ptr codecPack = std::make_shared<Codec::StreamPack>(
                    codecType, pack->GetSize());
                memcpy(codecPack->GetData(), pack->GetData(), pack->GetSize());
                codecPack->SetSize(pack->GetSize());
                // Convert uint64_t (microseconds) to milliseconds
                codecPack->pts = std::chrono::milliseconds(pack->pts / 1000);
                codecPack->dts = std::chrono::milliseconds(pack->dts / 1000);
                
                std::lock_guard<std::mutex> lock(packMtx);
                packQueue.push(codecPack);
            }
        });

        // Start puller
        if (!puller->Start())
        {
            MMP_LOG_ERROR << "Failed to start puller";
            decoder->Stop();
            decoder->Uninit();
            return 1;
        }

        MMP_LOG_INFO << "Puller started, waiting for data...";

        // Wait for data and push to decoder
        while (running)
        {
            Codec::StreamPack::ptr pack = nullptr;
            {
                std::lock_guard<std::mutex> lock(packMtx);
                if (!packQueue.empty())
                {
                    pack = packQueue.front();
                    packQueue.pop();
                }
            }

            if (pack)
            {
                MMP_LOG_INFO << "Push packet to decoder, size: " << pack->GetSize();
                decoder->Push(pack);
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            // Check puller status
            if (puller->GetPullStatus() == ClientPullStatus::Stoped ||
                puller->GetPullStatus() == ClientPullStatus::Failure ||
                puller->GetPullStatus() == ClientPullStatus::TimeOut)
            {
                MMP_LOG_WARN << "Puller stopped, status: " << puller->GetPullStatus();
                break;
            }
        }

        puller->Stop();
        /*********************************** 使用 StreamPuller (网络或MP4文件)(End) ******************************/
    }
    else
    {
        /*********************************** 原始文件模式 (H.26X raw file)(Begin) ******************************/
        std::shared_ptr<H26XFileByteReader> byteReader = std::make_shared<H26XFileByteReader>(inputPath);
        Codec::StreamPack::ptr pack = nullptr;

        size_t currentLoopTime = 0;
        do
        {
            pack = byteReader->GetNalUint();
            if (pack)
            {
                currentLoopTime++;
                MMP_LOG_INFO << "Push packet to decoder";
                decoder->Push(pack);
            }
        } while (pack && (loopTime == 0 || currentLoopTime < loopTime));
        /*********************************** 原始文件模式 (H.26X raw file)(End) ******************************/
    }

    if (display)
    {
        display->Close();
        display->UnInit();
    }

    running = false;
    while (!sync)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    decoder->Stop();
    decoder->Uninit();
    return 0;
}

/********************************************************* TEST(END) *****************************************************/

POCO_APP_MAIN(App)