# Bouquet roadmap

Plan: launch on Mac and Windows as **pay what you want**, build an audience with videos of the
garden, then bring Bouquet to **iPad** as an AUv3 instrument.

Rough timings assume part-time work. Each phase ends with a checkpoint that decides whether to move on.

---

## Phase 0: Decisions to make before anyone downloads it (week 1)

These are cheap now and painful to change later.

- [ ] **Lock the plugin identity.** DAWs remember plugins by bundle ID and manufacturer/plugin codes.
      Changing them after release breaks people's saved projects.
  - [ ] Pick a maker name (yours, or a small studio name) and set `COMPANY_NAME`.
  - [ ] Change `BUNDLE_ID` from `com.bouquet.*` to a domain you own (e.g. `com.yourstudio.bouquet`).
  - [ ] Set unique `PLUGIN_MANUFACTURER_CODE` / `PLUGIN_CODE` in `Bouquet/CMakeLists.txt`.
- [ ] **Check the name.** Search KVR, the App Store and trademarks for "Bouquet" in audio software.
      If it clashes, rename now (e.g. "Bouquet Garden").
- [ ] **Choose the licence for the pay-what-you-want phase.**
  - *Option A, keep AGPL-3.0:* honest for pay what you want, and no copy protection needed.
    But the App Store doesn't accept GPL-family code, so the iPad version (Phase 4) needs option B anyway.
  - *Option B, switch to a JUCE commercial licence now* (check juce.com for the free small-revenue
    tier) and make the repo private. One licence for every platform from day one.
  - Recommendation: **option B**. It avoids relicensing mid-flight and keeps the iPad path open.
- [ ] Register a domain and a contact email (e.g. `hello@yourstudio.com`).

## Phase 1: Ready to ship on desktop (weeks 2–4)

- [ ] **Notarize the Mac installer.** Join the Apple Developer Program ($99/yr) and add the
      `MACOS_*` / `NOTARY_*` secrets listed in the README. No more "Open Anyway".
- [ ] **Windows installer.** Package the VST3 (Inno Setup or similar). Code signing can wait.
      Expect a SmartScreen warning at first and explain it on the download page.
- [ ] **Bump to 1.0.0** in `VERSION` once the checklist below passes.
- [ ] **Compatibility pass**, load, play, save, reopen in:
  - [ ] Logic, Ableton Live, GarageBand, Reaper, FL Studio, Bitwig (whichever you can reach)
  - [ ] Apple silicon and Intel, macOS 11 through current; Windows 10/11
- [ ] **Performance pass.** Measure CPU with the Photoreal garden on vs off and with the editor
      closed. Make sure the UI never costs audio dropouts; consider a lower frame rate when idle.
- [ ] **Preset set.** 30–60 strong factory presets (flowers in the garden), named and tagged.
- [ ] **Crash and edge cases.** Sample rate changes, buffer size 32–2048, offline bounce,
      many instances, session reload with a missing preset.
- [ ] Short **user guide** (one page, mostly the README's "Using it" with screenshots).

**Checkpoint:** five friends or beta testers install it from a clean machine without help.

## Phase 2: Pay-what-you-want launch (weeks 4–6)

**Pricing:** $0 minimum, **suggested $19**. With pay what you want, skip licence keys and copy protection;
they add friction for no gain. The goal of this phase is **reach and an email list**, not revenue.

- [ ] **Storefronts**
  - [ ] itch.io page (music tools), pay what you want, suggested price, showcase GIF + video
  - [ ] Your own page (Lemon Squeezy or Gumroad checkout, also pay what you want) as the main link
  - [ ] Both collect email at download, with an opt-in for "updates and the iPad version"
- [ ] **Landing page**: sky background, the showcase video, 3–4 audio demos, download button,
      system requirements, FAQ (what is granular, is it free, Windows SmartScreen note).
- [ ] **Content before launch** (start posting 2 weeks early):
  - [ ] 6–10 short vertical clips: a chord growing a bouquet, watering a flower, bouquets rotting
        on release, the Magic Lab pad, a 30-second ambient track made only with Bouquet
  - [ ] TikTok, Instagram Reels, YouTube Shorts; same clips, consistent posting
- [ ] **Launch day**
  - [ ] Posts: r/synthesizers, r/ambientmusic, r/WeAreTheMusicMakers (check self-promo rules), KVR
        forum + KVR product listing, Gearspace, Bluesky/X
  - [ ] Email 15–30 ambient / sound-design YouTubers with a short note and the video
  - [ ] Submit to free-plugin roundups (they cover pay-what-you-want releases)
- [ ] **After launch**
  - [ ] Answer every comment and email for the first two weeks
  - [ ] Ship a quick 1.0.x with fixes from real users

**Track:** downloads, % who pay, average payment, email signups, which clip drove traffic.

**Checkpoint (about 6 weeks after launch):**
- 500+ email signups or a clip past 50k views → the audience is there, go to Phase 4 (iPad)
- Far less → do more content and a 1.1 update (Phase 3) before investing in a port

## Phase 3: Keep it growing (ongoing, alongside Phase 4)

- [ ] 1.1 update shaped by feedback (most-requested feature, more presets, more instruments)
- [ ] Monthly clip series, e.g. "watering one preset ten times"
- [ ] A user preset-sharing thread or Discord
- [ ] Optionally list on Plugin Boutique / KVR Marketplace once there are reviews and videos
- [ ] Later option: keep desktop free or pay what you want and charge for paid expansion packs
      (new instruments, new flower species)

## Phase 4: iPad (about 2–4 months after launch)

The garden, the bucket and pressing into keys are all better with touch. This is the growth bet.

### 4a. Technical feasibility spike (1–2 weeks)
- [ ] Add an iOS target: `FORMATS AUv3 Standalone` for iOS in CMake, Xcode project via the JUCE
      CMake Xcode generator
- [ ] **Memory budget.** AUv3 extensions run under tight memory limits. Measure: 138 MB of flower
      images + samples + grain buffers. Likely needs smaller/compressed flower atlases, lazy loading,
      and fewer resident sample layers.
- [ ] **CPU.** Test on the oldest iPad you want to support; pick a minimum (e.g. A12 / iPadOS 16).
- [ ] Load the AUv3 in AUM, Logic for iPad and GarageBand.

**Checkpoint:** it runs stable in a host on a mid-range iPad. If not, scope down (e.g. drawn mode only).

### 4b. Touch UI (3–6 weeks)
- [ ] Layouts for the full app and for the smaller AUv3 window hosts show
- [ ] Bigger hit targets for knobs and switches; drag-to-adjust that works with fingers
- [ ] Multi-touch keyboard in the meadow with velocity (and pressure where supported)
- [ ] The bucket: drag with a finger, tip with a gesture; maybe a haptic tap on watering
- [ ] Presets shared between the app and the AUv3 (app group container)
- [ ] Standalone app: audio session, background audio, MIDI from Bluetooth/USB keyboards

### 4c. App Store release (2–3 weeks)
- [ ] Enroll in Apple's Small Business Program (15% commission)
- [ ] Price: one-time **$9.99–14.99**, launch discount for the email list
- [ ] TestFlight beta, recruited from the email list
- [ ] App Store page: screenshots of the garden, a preview video, keywords (AUv3, granular, ambient)
- [ ] Privacy label (no data collected), App Review notes explaining AUv3 use
- [ ] Launch: email list first, then iPad music communities (r/ipadmusic, the AUM and
      Audiobus forums), iPad music YouTubers

## Costs at a glance

| Item | When | Cost |
| --- | --- | --- |
| Apple Developer Program | Phase 1 | $99/yr (covers Mac notarization **and** the iPad App Store) |
| Domain + simple site | Phase 0 | $20–100/yr |
| Storefront fees | Phase 2 | itch.io 10% (adjustable), Lemon Squeezy/Gumroad ~5–10% |
| JUCE licence | Phase 0 | check current terms; likely free under the small-revenue tier |
| App Store commission | Phase 4 | 15% |
| Windows code signing | optional | $0–300/yr |

## Timeline summary

| Phase | What | When |
| --- | --- | --- |
| 0 | Identity, name, licence | week 1 |
| 1 | Notarized, tested 1.0 | weeks 2–4 |
| 2 | Pay-what-you-want launch | weeks 4–6 |
| 3 | Updates and content | ongoing |
| 4 | iPad AUv3 | months 2–6 |
