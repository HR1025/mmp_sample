#include "AbstractDisplay.h"

#include <vector>

#ifdef SAMPLE_WITH_SDL
    #include "SDL/DisplaySDL.h"
#endif /* SAMPLE_WITH_SDL */

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
#ifdef SAMPLE_WITH_SDL
    else if (className == "DisplaySDL")
    {
        return std::make_shared<DisplaySDL>();
    }
#endif /* SAMPLE_WITH_SDL */
    else
    {
        return nullptr;
    }
}

} // namespace Mmp