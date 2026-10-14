#include "Processor.h"

#include <algorithm>
#include <atomic>
#include <map>

#if JUCE_MAC
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace {

std::unique_ptr<juce::AudioPluginFormatManager> makeFormatManager() {
  auto formats = std::make_unique<juce::AudioPluginFormatManager>();
  juce::addDefaultFormatsToManager(*formats);
  return formats;
}

bool parseDescription(const juce::String& xml, juce::PluginDescription& description) {
  auto document = juce::XmlDocument::parse(xml);
  return document != nullptr && description.loadFromXml(*document) &&
         !description.isInstrument;
}

std::shared_ptr<juce::AudioPluginInstance> instantiateEffect(
    const juce::String& xml, const juce::MemoryBlock& state, double rate, int blockSize) {
  juce::PluginDescription description;
  if (!parseDescription(xml, description))
    return {};

  auto formats = makeFormatManager();
  juce::String error;
  auto instance = formats->createPluginInstance(description, rate, blockSize, error);
  if (instance == nullptr) {
    juce::Logger::writeToLog("[Hosted effect] " + description.name + ": " + error);
    return {};
  }

  // A slot drives only the plug-in's MAIN bus. Several effects declare extra
  // buses — Plugin Alliance's bx_aura and bx_pulsar expose a second input bus
  // for side-chain — so the total channel count reads as 4 in and a stereo slot
  // rejected the plug-in. Keep those buses ENABLED (disabling them made bx_aura
  // crash in its render, which reads the side-chain bus) and feed the extras
  // silence in the chain's process path; only the MAIN bus must be mono/stereo.
  if (instance->getChannelCountOfBus(true, 0) < 1 || instance->getChannelCountOfBus(false, 0) < 1) {
    // The main bus came up disabled: ask for stereo on it.
    auto layout = instance->getBusesLayout();
    if (!layout.inputBuses.isEmpty())
      layout.inputBuses.getReference(0) = juce::AudioChannelSet::stereo();
    if (!layout.outputBuses.isEmpty())
      layout.outputBuses.getReference(0) = juce::AudioChannelSet::stereo();
    instance->setBusesLayout(layout);
  }

  const int mainIn = instance->getChannelCountOfBus(true, 0);
  const int mainOut = instance->getChannelCountOfBus(false, 0);
  if (mainIn < 1 || mainIn > 2 || mainOut < 1 || mainOut > 2) {
    juce::Logger::writeToLog(
        "[Hosted effect] Unsupported main-bus channel layout: " + description.name + " (" +
        juce::String(mainIn) + " in / " + juce::String(mainOut) + " out)");
    return {};
  }
  instance->setRateAndBufferSizeDetails(rate, blockSize);
  instance->prepareToPlay(rate, blockSize);
  if (!state.isEmpty())
    instance->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
  return std::shared_ptr<juce::AudioPluginInstance>(std::move(instance));
}

#if JUCE_MAC
// Reads the bundle's CFBundleIdentifier without loading/instantiating the plugin
// (CFBundleCreate just parses Info.plist — cheap and safe on the message thread).
juce::String bundleIdentifier(const juce::File& bundle) {
  const auto path = bundle.getFullPathName();
  CFURLRef url = CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(path.toRawUTF8()),
      static_cast<CFIndex>(path.getNumBytesAsUTF8()), true);
  if (url == nullptr)
    return {};
  CFBundleRef cfBundle = CFBundleCreate(kCFAllocatorDefault, url);
  CFRelease(url);
  if (cfBundle == nullptr)
    return {};
  juce::String result;
  if (CFStringRef id = CFBundleGetIdentifier(cfBundle)) {
    const CFIndex len = CFStringGetLength(id);
    const CFRange range = CFRangeMake(0, len);
    CFIndex used = 0;
    CFStringGetBytes(id, range, kCFStringEncodingUTF8, 0, false, nullptr, 0, &used);
    juce::HeapBlock<UInt8> buffer(static_cast<size_t>(used) + 1);
    CFStringGetBytes(id, range, kCFStringEncodingUTF8, 0, false,
                     reinterpret_cast<UInt8*>(buffer.getData()), used, nullptr);
    buffer[static_cast<size_t>(used)] = 0;
    result = juce::String::fromUTF8(reinterpret_cast<const char*>(buffer.getData()));
  }
  CFRelease(cfBundle);
  return result;
}
#endif

// Vendor label from a reverse-DNS bundle id, e.g. "com.fabfilter.Pro-Q.4" -> "FabFilter".
juce::String vendorFromBundleId(const juce::String& id) {
  if (id.isEmpty())
    return "Other";
  auto parts = juce::StringArray::fromTokens(id, ".", "");
  parts.trim();
  if (parts.isEmpty())
    return "Other";
  static const juce::StringArray kTlds{
      "com", "org", "net", "io", "co", "audio", "de", "se", "uk", "fr", "jp",
      "eu", "us", "me", "it", "nl", "ca", "ch", "at", "au", "tv", "app", "dev", "info", "biz"};
  int i = 0;
  while (i < parts.size() && kTlds.contains(parts[i].toLowerCase()))
    ++i;
  juce::String vendor = (i < parts.size()) ? parts[i] : parts.getReference(0);
  if (vendor.isEmpty())
    return "Other";
  // Normalise the grouping key (case + separators) so e.g. "plugin-alliance",
  // "pluginalliance" and "Pluginalliance" collapse to one vendor.
  const auto key = vendor.toLowerCase().retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789");
  // Display names for vendors whose reverse-DNS label is not their name.
  static const std::map<juce::String, juce::String> kAliases{
      {"uaudio", "Universal Audio"}, {"slatedigital", "Slate Digital"},
      {"pluginalliance", "Plugin Alliance"}, {"izotope", "iZotope"},
      {"fabfilter", "FabFilter"}, {"toneempire", "Tone Empire"},
      {"wavesaudio", "Waves"}, {"soundtheory", "Soundtheory"},
      {"bertomaudio", "Bertom Audio"}, {"ikmultimedia", "IK Multimedia"},
      {"nativeinstruments", "Native Instruments"}, {"neuraldsp", "Neural DSP"},
      {"blackroosteraudio", "Black Rooster Audio"}, {"unitedplugins", "United Plugins"},
      {"inmusicbrands", "inMusic Brands"}, {"theusualsuspects", "The Usual Suspects"},
      {"mcdsp", "McDSP"}, {"newfangledaudio", "Newfangled Audio"},
      {"lindellplugins", "Lindell Plugins"}, {"kiiveaudio", "Kiive Audio"},
      {"babyaudio", "BABY Audio"}, {"soundtoys", "Soundtoys"},
      {"softube", "Softube"}, {"eventide", "Eventide"}, {"ssl", "SSL"}};
  const auto alias = kAliases.find(key);
  if (alias != kAliases.end())
    return alias->second;
  return vendor.substring(0, 1).toUpperCase() + vendor.substring(1);
}

}  // namespace

juce::String TONE3000Processor::vendorForBundle(const juce::String& bundlePath) {
#if JUCE_MAC
  return vendorFromBundleId(bundleIdentifier(juce::File(bundlePath)));
#else
  juce::ignoreUnused(bundlePath);
  return "Other";
#endif
}

juce::var TONE3000Processor::listHostedEffectBundles() const {
  juce::Array<juce::var> result;
  auto addDir = [&result](const juce::String& dir, const juce::String& wildcard, const juce::String& format) {
    const juce::File folder(dir);
    if (!folder.isDirectory())
      return;
    for (const auto& entry : juce::RangedDirectoryIterator(
             folder, /*recursive*/ false, wildcard, juce::File::findDirectories)) {
      auto* item = new juce::DynamicObject();
      item->setProperty("name", entry.getFile().getFileNameWithoutExtension());
      item->setProperty("format", format);
      item->setProperty("path", entry.getFile().getFullPathName());
#if JUCE_MAC
      item->setProperty("vendor", vendorFromBundleId(bundleIdentifier(entry.getFile())));
#else
      item->setProperty("vendor", "Other");
#endif
      result.add(juce::var(item));
    }
  };
  addDir("/Library/Audio/Plug-Ins/Components", "*.component", "AudioUnit");
  addDir("/Library/Audio/Plug-Ins/VST3", "*.vst3", "VST3");
  return result;
}

juce::var TONE3000Processor::listHostedEffects() const {
  juce::Array<juce::var> result;
  auto formats = makeFormatManager();
  for (int i = 0; i < formats->getNumFormats(); ++i) {
    auto* format = formats->getFormat(i);
    if (format == nullptr || (format->getName() != "AudioUnit" && format->getName() != "VST3"))
      continue;
    // Hardcoded standard macOS locations, one per format, instead of every default
    // location JUCE would otherwise walk. Predictable and fast.
    juce::FileSearchPath searchPaths;
    if (format->getName() == "AudioUnit")
      searchPaths.add(juce::File("/Library/Audio/Plug-Ins/Components"));
    else if (format->getName() == "VST3")
      searchPaths.add(juce::File("/Library/Audio/Plug-Ins/VST3"));
    auto identifiers = format->searchPathsForPlugins(searchPaths, false, false);
    for (const auto& identifier : identifiers) {
      juce::OwnedArray<juce::PluginDescription> descriptions;
      format->findAllTypesForFile(descriptions, identifier);
      for (const auto* description : descriptions) {
        if (description == nullptr || description->isInstrument ||
            description->name.containsIgnoreCase("TONE3000") ||
            description->name.containsIgnoreCase("zynamfx"))
          continue;
        auto xml = description->createXml();
        if (xml == nullptr)
          continue;
        auto* item = new juce::DynamicObject();
        item->setProperty("name", description->name);
        item->setProperty("manufacturer", description->manufacturerName);
        item->setProperty("format", format->getName());
        item->setProperty("description", xml->toString());
        result.add(juce::var(item));
      }
    }
  }
  return result;
}

std::string TONE3000Processor::addHostedEffect(const juce::String& xml,
                                                const std::string& targetInsertId) {
  juce::PluginDescription description;
  if (!parseDescription(xml, description))
    return {};
  auto instance = instantiateEffect(xml, {}, chainSampleRate(), chainDomainBlockSize());
  if (instance == nullptr)
    return {};

  ChainEditFade editFade(*this);
  juce::ScopedLock lock(chainMutex);
  pushChainHistory();
  const std::string id = juce::Uuid().toString().toStdString();
  auto block = std::make_unique<ChainBlock>(id, ChainBlockType::EFFECT);
  block->effectDescriptionXml = xml;
  block->effectName = description.name;
  block->effectInstance = std::move(instance);
  block->loaded = true;
  block->effectScratch.setSize(juce::jmax(block->effectInstance->getTotalNumInputChannels(),
                                         block->effectInstance->getTotalNumOutputChannels()),
                               chainDomainBlockSize());
  prepareBlockForChainRate(*block);

  Lane* target = nullptr;
  Lane::iterator slot;
  for (auto& laneBlocks : lanes) {
    auto it = std::find_if(laneBlocks.begin(), laneBlocks.end(), [&](const auto& candidate) {
      return candidate->type == ChainBlockType::INSERT && candidate->id == targetInsertId;
    });
    if (it != laneBlocks.end()) {
      target = &laneBlocks;
      slot = it;
      break;
    }
  }
  if (target == nullptr) {
    target = &activeChain();
    slot = std::find_if(target->begin(), target->end(), [](const auto& candidate) {
      return candidate->type == ChainBlockType::INSERT;
    });
  }
  if (slot != target->end())
    *slot = std::move(block);
  else
    target->push_back(std::move(block));
  alignBranchLaneLengths();
  bumpChainRevision();
  triggerAsyncUpdate();
  return id;
}

std::string TONE3000Processor::addHostedEffectFromFile(
    const juce::File& bundle, const std::string& targetInsertId) {
  if (!bundle.hasFileExtension(".component") && !bundle.hasFileExtension(".vst3"))
    return {};
  auto formats = makeFormatManager();
  for (int i = 0; i < formats->getNumFormats(); ++i) {
    auto* format = formats->getFormat(i);
    if (format == nullptr ||
        (bundle.hasFileExtension(".component") && format->getName() != "AudioUnit") ||
        (bundle.hasFileExtension(".vst3") && format->getName() != "VST3"))
      continue;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format->findAllTypesForFile(descriptions, bundle.getFullPathName());
    for (const auto* description : descriptions) {
      if (description == nullptr || description->isInstrument)
        continue;
      if (auto xml = description->createXml())
        return addHostedEffect(xml->toString(), targetInsertId);
    }
  }
  return {};
}

void TONE3000Processor::restoreHostedEffect(ChainBlock& block) {
  block.effectInstance = instantiateEffect(block.effectDescriptionXml, block.effectState,
                                            chainSampleRate(), chainDomainBlockSize());
  block.loaded = block.effectInstance != nullptr;
  if (block.loaded)
    block.effectScratch.setSize(juce::jmax(block.effectInstance->getTotalNumInputChannels(),
                                          block.effectInstance->getTotalNumOutputChannels()),
                                chainDomainBlockSize());
  block.loadFailed = !block.loaded;
  block.modelLoading = false;
}

std::shared_ptr<juce::AudioPluginInstance> TONE3000Processor::hostedEffectInstance(
    const std::string& blockId) {
  juce::ScopedLock lock(chainMutex);
  auto* block = findBlockById(blockId);
  return block != nullptr && block->type == ChainBlockType::EFFECT
             ? block->effectInstance : std::shared_ptr<juce::AudioPluginInstance>{};
}

// #############################
// FIRST-RUN HOSTED-EFFECT VALIDATION
// #############################
// A chain slot rejects a hosted AudioUnit for one of two reasons: the plug-in
// cannot be loaded at all, or it reports a channel layout a stereo slot cannot
// host (see instantiateEffect). Both are silent in normal use — the slot simply
// stays empty — so the first standalone run on a machine writes a per-plugin
// verdict to the app-data folder. See Processor.h for the contract.
//
// Validation is OUT OF PROCESS: each component is probed by running
// /usr/bin/auval as a child process, which loads the AU in its own process. The
// earlier in-process scan instantiated every AU with JUCE, and JUCE marshals AU
// instantiation onto the main thread, so one broken AU (2026-10-14: IK
// Multimedia's Lurssen Mastering Console segfaulted in APComponent::newInstance)
// took the whole app down. Running auval can no longer crash the app, and a
// per-component timeout bounds a hung probe. The report is written after every
// component, so a run killed mid-check resumes.

juce::File TONE3000Processor::hostedValidationReportFile() {
  return getSettingsFile().getParentDirectory().getChildFile("hosted-effects-validation.txt");
}

#if JUCE_MAC
namespace {

// auval can take ~a second per component; this bound is generous headroom so a
// merely slow load still passes, while a hung probe is killed rather than
// stalling the scan forever.
constexpr int kAuvalTimeoutMs = 60000;

// True once the report carries its final "result:" line (a completed run).
bool reportIsComplete(const juce::String& text) {
  for (const auto& line : juce::StringArray::fromLines(text))
    if (line.startsWith("result:"))
      return true;
  return false;
}

// The last non-empty, trimmed line of a child process's combined output — for
// auval that is normally "AU VALIDATION SUCCEEDED." / "AU VALIDATION FAILED.".
juce::String lastNonEmptyLine(const juce::String& text) {
  juce::StringArray lines;
  lines.addLines(text);
  for (int i = lines.size(); --i >= 0;)
    if (lines[i].trim().isNotEmpty())
      return lines[i].trim();
  return "(no output)";
}

// Converts a CFString to a juce String (same approach as bundleIdentifier).
juce::String cfStringToJuce(CFStringRef string) {
  if (string == nullptr || CFGetTypeID(string) != CFStringGetTypeID())
    return {};
  const CFRange range = CFRangeMake(0, CFStringGetLength(string));
  CFIndex used = 0;
  CFStringGetBytes(string, range, kCFStringEncodingUTF8, 0, false, nullptr, 0, &used);
  juce::HeapBlock<UInt8> buffer(static_cast<size_t>(used) + 1);
  CFStringGetBytes(string, range, kCFStringEncodingUTF8, 0, false,
                   reinterpret_cast<UInt8*>(buffer.getData()), used, nullptr);
  buffer[static_cast<size_t>(used)] = 0;
  return juce::String::fromUTF8(reinterpret_cast<const char*>(buffer.getData()));
}

// A 4-character AU code (type/subtype/manufacturer). Anything that is not
// exactly four characters is skipped, so auval never receives a malformed code.
juce::String fourCharCode(CFStringRef string) {
  const auto code = cfStringToJuce(string);
  return code.length() == 4 ? code : juce::String();
}

// One AudioComponents entry from a bundle's Info.plist.
struct AudioUnitComponent {
  juce::String type, subtype, manufacturer, name, code;  // code = type/subtype/manufacturer
};

// Parses a bundle's Contents/Info.plist AudioComponents array with
// CoreFoundation. CFBundleCreate only reads the plist; it never loads or
// instantiates the plug-in.
juce::Array<AudioUnitComponent> readAudioComponents(const juce::File& bundle) {
  juce::Array<AudioUnitComponent> result;
  const auto path = bundle.getFullPathName();
  CFURLRef url = CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(path.toRawUTF8()),
      static_cast<CFIndex>(path.getNumBytesAsUTF8()), true);
  if (url == nullptr)
    return result;
  CFBundleRef cfBundle = CFBundleCreate(kCFAllocatorDefault, url);
  CFRelease(url);
  if (cfBundle == nullptr)
    return result;
  CFTypeRef value = CFBundleGetValueForInfoDictionaryKey(cfBundle, CFSTR("AudioComponents"));
  if (value != nullptr && CFGetTypeID(value) == CFArrayGetTypeID()) {
    auto array = static_cast<CFArrayRef>(value);
    const CFIndex count = CFArrayGetCount(array);
    for (CFIndex i = 0; i < count; ++i) {
      auto* entry = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(array, i));
      if (entry == nullptr || CFGetTypeID(entry) != CFDictionaryGetTypeID())
        continue;
      AudioUnitComponent component;
      component.type = fourCharCode((CFStringRef) CFDictionaryGetValue(entry, CFSTR("type")));
      component.subtype = fourCharCode((CFStringRef) CFDictionaryGetValue(entry, CFSTR("subtype")));
      component.manufacturer =
          fourCharCode((CFStringRef) CFDictionaryGetValue(entry, CFSTR("manufacturer")));
      if (component.type.isEmpty() || component.subtype.isEmpty() ||
          component.manufacturer.isEmpty())
        continue;  // malformed entry: skip rather than hand auval a bad code
      component.name = cfStringToJuce((CFStringRef) CFDictionaryGetValue(entry, CFSTR("name")));
      if (component.name.isEmpty())
        component.name = bundle.getFileNameWithoutExtension();
      component.code = component.type + "/" + component.subtype + "/" + component.manufacturer;
      result.add(component);
    }
  }
  CFRelease(cfBundle);
  return result;
}

}  // namespace
#endif

void TONE3000Processor::validateHostedEffects(bool restart) {
#if JUCE_MAC
  const juce::File report = hostedValidationReportFile();
  if (restart)
    report.deleteFile();  // explicit re-run: start from the top
  // The report is the checkpoint. Without `restart`, a run that reached its
  // "result:" line has nothing left to do.
  if (report.existsAsFile() && reportIsComplete(report.loadFileAsString()))
    return;

  // One run at a time, process-wide (several windows share the app-data dir).
  // The worker clears the guard however it exits.
  static std::atomic<bool> running{false};
  if (running.exchange(true))
    return;

  // Everything below runs on a background thread: enumerating thousands of
  // bundles and spawning auval per component must never touch the message or
  // audio thread. Nothing is ever instantiated in-process.
  juce::Thread::launch([report] {
    auto scan = [&] {
      // Re-read on the worker: the resume set comes from what is already on
      // disk, so a run killed mid-check continues where it stopped.
      juce::StringArray lines;
      if (report.existsAsFile())
        lines.addLines(report.loadFileAsString());
      for (const auto& line : lines)
        if (line.startsWith("result:"))
          return;  // completed by another instance meanwhile
      // Drop a trailing TEST line (and anything after it) left by a run that
      // died mid-check, so that component is retried instead of trusted.
      for (int i = lines.size(); --i >= 0;)
        if (lines[i].startsWith("TEST"))
          lines.removeRange(i, lines.size());
      // Codes already decided by earlier (resumed) runs: the first field after
      // the "OK "/"FAIL" status.
      juce::StringArray tested;
      int ok = 0, fail = 0;
      for (const auto& line : lines) {
        auto fields = juce::StringArray::fromTokens(line, " \t", "");
        fields.removeEmptyStrings();
        if (fields.isEmpty())
          continue;
        if (fields[0] == "OK")
          ++ok;
        else if (fields[0] == "FAIL")
          ++fail;
        if (fields.size() >= 2 && (fields[0] == "OK" || fields[0] == "FAIL"))
          tested.add(fields[1]);
      }

      report.getParentDirectory().createDirectory();
      // Write after every line: a killed run still leaves the report naming the
      // component it was on, and resumes from there.
      auto flush = [&lines, &report] {
        report.replaceWithText(lines.joinIntoString("\n") + "\n");
      };
      if (lines.isEmpty()) {
        lines.add("zynamfx hosted Audio Unit validation");
        lines.add("generated: " + juce::Time::getCurrentTime().toISO8601(true));
        lines.add("out-of-process: each check runs /usr/bin/auval in its own process, so a");
        lines.add("broken plug-in cannot crash the app. Delete this file to start over.");
        lines.add("");
        flush();
      }

      // auval ships with Apple's CoreAudio tools; without it there is nothing
      // safe to run, so note it once and stop rather than guess.
      if (!juce::File("/usr/bin/auval").existsAsFile()) {
        const juce::String note =
            "auval not found at /usr/bin/auval; install the Xcode command line tools to "
            "validate hosted AudioUnits.";
        if (!lines.contains(note)) {
          lines.add(note);
          flush();
        }
        return;
      }

      const juce::File componentsDir("/Library/Audio/Plug-Ins/Components");
      if (componentsDir.isDirectory()) {
        for (const auto& entry : juce::RangedDirectoryIterator(
                 componentsDir, /*recursive*/ false, "*.component",
                 juce::File::findDirectories)) {
          const juce::File bundle = entry.getFile();
          for (const auto& component : readAudioComponents(bundle)) {
            if (tested.contains(component.code))
              continue;
            const juce::String label =
                component.code + "  " + component.name + " - " + bundle.getFullPathName();
            // "TEST" first, then rewritten: a worker killed between the two
            // lines leaves the TEST line last, so the next run retries it.
            lines.add("TEST  " + label);
            flush();

            const juce::StringArray arguments{"/usr/bin/auval", "-v", component.type,
                                              component.subtype, component.manufacturer};
            int exitCode = -1;
            bool timedOut = false, started = false;
            juce::String output;
            {
              juce::ChildProcess process;
              started = process.start(arguments);
              if (started) {
                // Bounded wait: a hung component is killed, never allowed to
                // stall the scan (auval's own output is tiny, so the pipe
                // cannot fill while we wait).
                if (process.waitForProcessToFinish(kAuvalTimeoutMs)) {
                  exitCode = static_cast<int>(process.getExitCode());
                } else {
                  timedOut = true;
                  process.kill();
                }
                output = process.readAllProcessOutput();
              }
            }

            juce::String verdict;
            if (!started) {
              verdict = "FAIL  " + label + "\n        auval: could not start process";
            } else if (timedOut) {
              verdict = "FAIL  " + label + "\n        auval: timeout after " +
                        juce::String(kAuvalTimeoutMs / 1000) + " s, process killed";
            } else if (exitCode == 0) {
              verdict = "OK    " + label;
            } else {
              verdict = "FAIL  " + label + "\n        auval exit " +
                        juce::String(exitCode) + ": " + lastNonEmptyLine(output);
            }
            if (verdict.startsWith("OK"))
              ++ok;
            else
              ++fail;
            lines.set(lines.size() - 1, verdict);
            flush();
          }
        }
      }

      lines.add("");
      lines.add("result: " + juce::String(ok) + " ok, " + juce::String(fail) + " fail, " +
                juce::String(ok + fail) + " total");
      flush();
      juce::Logger::writeToLog("[Hosted effect] Out-of-process AU validation wrote " +
                               report.getFullPathName() + " (" + juce::String(ok) + " ok / " +
                               juce::String(fail) + " fail)");
    };
    scan();
    running.store(false);  // release the one-run guard, however scan() returned
  });
#else
  // auval and AU component bundles are macOS-only, so there is nothing to run.
  (void)0;
#endif
}
