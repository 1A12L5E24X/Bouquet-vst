#include "Assets.h"

namespace bouquet::assets
{
juce::File find (const juce::String& name, const juce::String& marker)
{
    const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    auto shared = juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory);
    auto user = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    // JUCE gives /Library and ~/Library here; the installer uses Application Support inside them.
    shared = shared.getChildFile ("Application Support");
    user = user.getChildFile ("Application Support");
   #endif
    juce::Array<juce::File> candidates {
        exe.getParentDirectory().getParentDirectory().getChildFile ("Resources").getChildFile (name),   // any bundle
        exe.getParentDirectory().getChildFile (name),                                                    // a plain executable
        shared.getChildFile ("Bouquet").getChildFile (name),                                             // installed (Mac, Windows)
        user.getChildFile ("Bouquet").getChildFile (name),
    };
   #ifdef BOUQUET_ASSET_DIR
    // (CI turns this off to check that an installed copy is found.)
    if (juce::SystemStats::getEnvironmentVariable ("BOUQUET_NO_DEV_ASSETS", {}).isEmpty())
        candidates.add (juce::File (BOUQUET_ASSET_DIR).getChildFile (name.toLowerCase()));                  // development
   #endif
    for (auto& c : candidates)
        if (c.getChildFile (marker).existsAsFile())
            return c;
    return {};
}
} // namespace bouquet::assets
