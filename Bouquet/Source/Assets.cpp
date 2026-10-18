#include "Assets.h"

namespace bouquet::assets
{
juce::File find (const juce::String& name, const juce::String& marker)
{
    const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    juce::Array<juce::File> candidates {
        exe.getParentDirectory().getParentDirectory().getChildFile ("Resources").getChildFile (name),   // any bundle
        exe.getParentDirectory().getChildFile (name),                                                    // a plain executable
        juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory).getChildFile ("Bouquet").getChildFile (name),
        juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Bouquet").getChildFile (name),
    };
   #ifdef BOUQUET_ASSET_DIR
    candidates.add (juce::File (BOUQUET_ASSET_DIR).getChildFile (name.toLowerCase()));                  // development
   #endif
    for (auto& c : candidates)
        if (c.getChildFile (marker).existsAsFile())
            return c;
    return {};
}
} // namespace bouquet::assets
