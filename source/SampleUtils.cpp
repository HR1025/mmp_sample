#include "SampleUtils.h"

#include <cstring>
#include <memory.h>

#include "GPU/GL/GLCommon.h"
#include "Codec/CodecFactory.h"

#include "PngA.h"
#include "PngB.h"

namespace Mmp
{

AbstractPicture::ptr GetFrame1920x1080A()
{
    using namespace Codec;
    // 1 - Read compress data from png array
    NormalPack::ptr pack = std::make_shared<NormalPack>(0);
    pack->SetCapacity(sizeof(bin2c_A_png));
    pack->SetSize((size_t)(sizeof(bin2c_A_png)));
    memcpy(pack->GetData(), bin2c_A_png, sizeof(bin2c_A_png));
    // 2 - decoder to pixel data
    AbstractFrame::ptr frame;
    {
        auto pngDecoder = DecoderFactory::DefaultFactory().CreateDecoder("PngDecoder");
        pngDecoder->Init();
        pngDecoder->Start();
        PngDecoderParameter parameter;
        parameter.format = PixelFormat::RGBA8888;
        pngDecoder->SetParameter(parameter);
        pngDecoder->Push(pack);
        if (!pngDecoder->Pop(frame))
        {
            assert(false);
        }
        pngDecoder->Stop();
        pngDecoder->Uninit();
    }
    return std::dynamic_pointer_cast<AbstractPicture>(frame);
}

AbstractPicture::ptr GetFrame1920x1080B()
{
    using namespace Codec;
    // 1 - Read compress data from png array
    NormalPack::ptr pack = std::make_shared<NormalPack>(0);
    pack->SetCapacity(sizeof(bin2c_B_png));
    pack->SetSize((size_t)(sizeof(bin2c_B_png)));
    memcpy(pack->GetData(), bin2c_B_png, sizeof(bin2c_B_png));
    // 2 - decoder to pixel data
    AbstractFrame::ptr frame;
    {
        auto pngDecoder = DecoderFactory::DefaultFactory().CreateDecoder("PngDecoder");
        pngDecoder->Init();
        pngDecoder->Start();
        PngDecoderParameter parameter;
        parameter.format = PixelFormat::RGBA8888;
        pngDecoder->SetParameter(parameter);
        pngDecoder->Push(pack);
        if (!pngDecoder->Pop(frame))
        {
            assert(false);
        }
        pngDecoder->Stop();
        pngDecoder->Uninit();
    }
    return std::dynamic_pointer_cast<AbstractPicture>(frame);
}

GPUBackend GetGPUBackend(const std::string& str)
{
    if ("OPENGL" == str)
    {
        return  GPUBackend::OPENGL;
    }
    else if ("OPENGL_ES" == str)
    {
        return GPUBackend::OPENGL_ES;
    }
    else if ("D3D11" == str)
    {
        return GPUBackend::D3D11;
    }
    else if ("VULKAN" == str)
    {
        return GPUBackend::VULKAN;
    }
    else
    {
        return GPUBackend::OPENGL;
    }
}

} // namespace Mmp