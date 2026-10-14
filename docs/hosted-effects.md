# Hosted AU and VST3 effects (test build)

This fork adds an **Effect** block to the TONE3000 signal chain on macOS.
It hosts installed Audio Unit and VST3 audio effects with JUCE. Instruments,
VST2 plug-ins, AUv3 extensions, and MIDI effects are outside this test build.

1. Open the standalone app and right-click an empty `+` chain tile.
2. Choose **Add AU/VST3 Effect** — an in-app browser, not a Finder dialog. A
   **search field** sits across the top. With it empty, the left column lists
   **vendors**; picking one opens that vendor's effects in the right column and
   **blurs the vendor column** behind it (backdrop blur). Pick an effect to load
   it. Vendors come from each bundle's reverse-DNS identifier (e.g.
   `com.fabfilter…` → *FabFilter*); the same name in a vendor is shown as
   `(AU)` / `(VST3)`. Typing filters **every** installed bundle into one global,
   scrollable results list — case-insensitive, whitespace-separated tokens that
   must all appear in the effect name, vendor or format — each row reading
   `name · vendor (format)` so duplicates are unambiguous; clearing the field
   restores vendor browsing, reopening the previously chosen vendor. The list is
   built from a directory listing of `/Library/Audio/Plug-Ins/Components` and
   `/Library/Audio/Plug-Ins/VST3` — browsing **and searching** only filter the
   already-listed entries; neither scans or instantiates, so both are instant.
3. **Click the effect tile** to open the plugin's own editor window. Once you have
   opened it once, the slot shows a small **picture of the plugin's interface**
   (captured from its window) so you can see what is loaded at a glance — until
   then it shows a coloured **vendor badge** with the plugin name. The tile can be
   powered off, removed, dragged to another position, duplicated, and saved in
   presets.

The hosted plug-in must still be installed when a preset or DAW project is
reopened. If it is absent, the block remains visible and passes audio through.
Its saved state is retained so the effect can load when the plug-in is restored.

## Hosted-AudioUnit validation (out-of-process, opt-in)

Some installed effects cannot be loaded into a chain slot, and the slot just
stays empty: either the plug-in fails to load, or it reports a channel layout a
mono/stereo slot cannot host (see `instantiateEffect` in
`ProcessorHostedEffects.cpp`). To find them, **Settings -> Diagnostics ->
Validate installed Audio Units** probes every AudioUnit in
`/Library/Audio/Plug-Ins/Components` and writes a per-component verdict to:

```
~/Library/Application Support/zynamfx/hosted-effects-validation.txt
```

The check runs **out of process**: each component is probed by running
`/usr/bin/auval -v <type> <subtype> <manufacturer>` as a child process
(`TONE3000Processor::validateHostedEffects`) and recording its exit code. auval
loads the AU in its own process, so a broken plug-in cannot crash the app — the
earlier in-process scan instantiated every AU with JUCE on the main thread and
was killed by IK Multimedia's *Lurssen Mastering Console* on 2026-10-14, which is
why it was replaced. Each probe has a 60 s timeout; a hung one is killed and
marked `FAIL` with reason `timeout`. **auval must be present**
(`/usr/bin/auval`, part of the Xcode command line tools) and each component must
be registered with the system; the codes come from the bundle's
`Contents/Info.plist` `AudioComponents` array, so no plug-in is ever loaded
in-process.

Each line is `TEST`, `OK` or `FAIL` plus the component code
(`type/subtype/manufacturer`), name and bundle path; a `FAIL` line carries the
auval exit code and the last output line. The report is rewritten after **every**
component, so an interrupted run resumes: `TEST` lines are retried, `OK`/`FAIL`
lines are skipped. The **Validate** action starts a fresh run (it deletes the
existing report); **Reveal validation report** opens the file.

It is opt-in because auval costs a few seconds per component — measured ~4.4 s
each, i.e. **hours** across the thousands of bundles a large install carries — so
it runs in the background on a worker thread and must not be started implicitly
at launch. One run at a time per process; a run already in progress is ignored.
VST3 effects are hosted too but are not in this pass.

This is an out-of-process host. Effects are still loaded in-process when used in
a chain, so a faulty third-party effect can crash TONE3000 or the DAW running it.
Start testing in the standalone app with a simple AU/VST3 effect. Serial effect
latency is reported to the outer host; parallel branch alignment and latency
changes made inside an open effect editor still need dedicated validation.
