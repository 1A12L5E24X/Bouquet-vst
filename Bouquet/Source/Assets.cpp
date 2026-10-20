#include "Assets.h"
#include <mutex>

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
    juce::String report = "find " + name + " (" + marker + ") from " + exe.getFullPathName() + ":";
    juce::File found;
    for (auto& c : candidates)
    {
        const bool hit = c.getChildFile (marker).existsAsFile();
        report << "\n    " << (hit ? "found   " : (c.isDirectory() ? "no marker " : "missing ")) << c.getFullPathName();
        if (hit && found == juce::File())
            found = c;
    }
    log (report);
    return found;
}

void log (const juce::String& line)
{
   #if JUCE_MAC
    const auto file = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Library/Logs/Bouquet.log");
   #else
    const auto file = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Bouquet/Bouquet.log");
   #endif
    static std::mutex lock;
    const std::scoped_lock guard (lock);
    file.getParentDirectory().createDirectory();
    if (file.getSize() > 256 * 1024)
        file.deleteFile();
    file.appendText (juce::Time::getCurrentTime().toString (true, true) + "  " + line + "\n");
}
} // namespace bouquet::assets
