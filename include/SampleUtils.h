#pragma once

#include <string>

#include "Common/AbstractPicture.h"
#include "GPU/GL/GLCommon.h"

namespace Mmp
{

AbstractPicture::ptr GetFrame1920x1080A();

AbstractPicture::ptr GetFrame1920x1080B();

GPUBackend GetGPUBackend(const std::string& str);

} // namespace Mmp