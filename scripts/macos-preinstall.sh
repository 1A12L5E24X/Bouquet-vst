#!/bin/sh
# Installer preinstall: removes plugins from earlier builds that are no longer shipped
# (Murmur, the old name of Bouquet, and Simple Synth), so hosts don't list them twice.

for lib in /Library /Users/*/Library; do
    for bundle in \
        "Audio/Plug-Ins/Components/Murmur.component" \
        "Audio/Plug-Ins/Components/Murmur FX.component" \
        "Audio/Plug-Ins/Components/Simple Synth.component" \
        "Audio/Plug-Ins/VST3/Murmur.vst3" \
        "Audio/Plug-Ins/VST3/Murmur FX.vst3" \
        "Audio/Plug-Ins/VST3/Simple Synth.vst3"; do
        [ -e "$lib/$bundle" ] && rm -rf "$lib/$bundle"
    done
done

for app in "/Applications/Murmur.app" "/Applications/Simple Synth.app"; do
    [ -e "$app" ] && rm -rf "$app"
done

exit 0
