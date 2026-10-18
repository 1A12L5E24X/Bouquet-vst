#!/bin/sh
# Installer preinstall: removes plugins from earlier builds that are no longer shipped
# (Murmur, the old name of Bouquet; Simple Synth; Bouquet Real, now part of Bouquet),
# so hosts don't list them twice.

for lib in /Library /Users/*/Library; do
    for name in "Murmur" "Murmur FX" "Simple Synth" "Bouquet Real" "Bouquet Real FX"; do
        for bundle in "Audio/Plug-Ins/Components/$name.component" "Audio/Plug-Ins/VST3/$name.vst3"; do
            [ -e "$lib/$bundle" ] && rm -rf "$lib/$bundle"
        done
    done
done

for app in "/Applications/Murmur.app" "/Applications/Simple Synth.app" "/Applications/Bouquet Real.app"; do
    [ -e "$app" ] && rm -rf "$app"
done

exit 0
