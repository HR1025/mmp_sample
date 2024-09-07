#include "AbstractDisplay.h"

#include <vector>

#include "DisplaySDL.h"

namespace Mmp
{

AbstractDisplay::ptr AbstractDisplay::Create(const std::string& className)
{
    static std::vector<std::string> kClassNames = 
    {
        "DisplaySDL"
    };

    if (className.empty())
    {
        AbstractDisplay::ptr display;
        for (const auto& _className : kClassNames)
        {
            // Hint : recursive call
            display = Create(_className);
            if (display)
            {
                break;
            }
        }
        return display;
    }
    else if (className == "DisplaySDL")
    {
        return std::make_shared<DisplaySDL>();
    }
    else
    {
        return nullptr;
    }
}

} // namespace Mmp