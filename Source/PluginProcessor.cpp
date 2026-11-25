#include "PluginProcessor.h"

#include "BinaryData.h"
#include "PluginEditor.h"

#include <cstring>

namespace loopdeck
{

LoopDeckAudioProcessor::LoopDeckAudioProcessor()
    : AudioProcessor (BusesProperties()),
      apvts (*this, nullptr, "LoopDeck", params::createLayout())
{
    for (auto& index : slotLoopIndex)
        index.store (-1);

    for (auto& map : customMaps)
        map = makeIdentityDrumMap();

    library.loadFromMemory (BinaryData::loopdeck_library_bin,
                            (size_t) BinaryData::loopdeck_library_binSize);

    if (library.getNumLoops() > 0)
        loadLoopIntoSlot (0, 0);
}

LoopDeckAudioProcessor::~LoopDeckAudioProcessor() = default;

void LoopDeckAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    lastBlockSize = juce::jmax (1, samplesPerBlock);
    engine.prepare (sampleRate);
}

void LoopDeckAudioProcessor::releaseResources()
{
}

bool LoopDeckAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.inputBuses.isEmpty() && layouts.outputBuses.isEmpty();
}

float LoopDeckAudioProcessor::raw (const juce::String& id) const
{
    if (auto* value = apvts.getRawParameterValue (id))
        return value->load();

    return 0.0f;
}

int LoopDeckAudioProcessor::rawInt (const juce::String& id) const
{
    return (int) raw (id);
}

EngineSettings LoopDeckAudioProcessor::buildSettings() const
{
    EngineSettings settings;

    settings.master = raw (params::master) > 0.5f;
    settings.thru = raw (params::thru) > 0.5f;
    settings.followMidiTransport = raw (params::followMidiTransport) > 0.5f;
    settings.allNotesOffOnStop = raw (params::stopNotes) > 0.5f;
    settings.triggerMode = rawInt (params::triggerMode);
    settings.inputChannel = rawInt (params::inputChannel);
    settings.thruChannel = rawInt (params::thruChannel);
    settings.clockSource = rawInt (params::clockSource);
    settings.triggerNote = rawInt (params::triggerNote);
    settings.internalBpm = (double) raw (params::internalBpm);

    for (int slot = 0; slot < numSlots; ++slot)
    {
        auto& target = settings.slots[(size_t) slot];

        target.enabled = raw (params::slotId (slot, params::slotEnabled)) > 0.5f;
        target.mute = raw (params::slotId (slot, params::slotMute)) > 0.5f;
        target.solo = raw (params::slotId (slot, params::slotSolo)) > 0.5f;
        target.channel = rawInt (params::slotId (slot, params::slotChannel));
        target.transpose = rawInt (params::slotId (slot, params::slotTranspose));
        target.lengthChoice = rawInt (params::slotId (slot, params::slotLength));
        target.direction = rawInt (params::slotId (slot, params::slotDirection));
        target.mapPreset = rawInt (params::slotId (slot, params::slotMap));
        target.fixedVelocity = rawInt (params::slotId (slot, params::slotFixedVelocity));
        target.speed = getSpeedChoiceMultiplier (rawInt (params::slotId (slot, params::slotSpeed)));
        target.gate = (double) raw (params::slotId (slot, params::slotGate)) / 100.0;
        target.velocityScale = (double) raw (params::slotId (slot, params::slotVelocity)) / 100.0;
        target.swing = (double) raw (params::slotId (slot, params::slotSwing)) / 100.0;
        target.startTrim = (double) raw (params::slotId (slot, params::slotTrim)) / 100.0;
    }

    return settings;
}

void LoopDeckAudioProcessor::syncDrumMaps()
{
    const juce::ScopedLock lock (mapLock);

    for (int slot = 0; slot < numSlots; ++slot)
    {
        const auto preset = rawInt (params::slotId (slot, params::slotMap));

        if (preset == lastMapPreset[(size_t) slot])
            continue;

        lastMapPreset[(size_t) slot] = preset;
        engine.setDrumMap (slot, makeDrumMapPreset (preset, customMaps[(size_t) slot]));
    }
}

void LoopDeckAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    const juce::ScopedNoDenormals noDenormals;

    const bool hadInput = ! midi.isEmpty();

    syncDrumMaps();
    engine.setSettings (buildSettings());

    TransportSnapshot snapshot;

    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            snapshot.isPlaying = position->getIsPlaying();

            if (const auto ppq = position->getPpqPosition())
            {
                snapshot.hasPosition = true;
                snapshot.ppq = *ppq;
            }

            if (const auto bpm = position->getBpm())
                snapshot.bpm = *bpm;
        }
    }

    if (snapshot.hasPosition)
        hostPlayheadSeen.store (true);

    auto numSamples = buffer.getNumSamples();

    if (numSamples <= 0)
        numSamples = lastBlockSize;

    engine.process (midi, numSamples, snapshot);
    buffer.clear();

    const auto sampleRate = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
    const auto decay = (float) ((double) numSamples / sampleRate * 6.0);

    inputActivity.store (hadInput ? 1.0f : juce::jmax (0.0f, inputActivity.load() - decay));
    outputActivity.store (engine.getActivity());
}

juce::AudioProcessorEditor* LoopDeckAudioProcessor::createEditor()
{
    return new LoopDeckAudioProcessorEditor (*this);
}

//==============================================================================
void LoopDeckAudioProcessor::loadLoopIntoSlot (int slot, int loopIndex)
{
    if (! juce::isPositiveAndBelow (slot, numSlots))
        return;

    if (! library.isValidIndex (loopIndex))
    {
        clearSlot (slot);
        return;
    }

    const auto* info = library.getInfo (loopIndex);
    const auto midiFile = library.readMidiFile (loopIndex);
    auto pattern = LoopPattern::fromMidiFile (midiFile,
                                              info != nullptr ? info->getLengthBeats() : 4.0,
                                              info != nullptr ? info->name : juce::String());

    engine.setPattern (slot, pattern);
    slotLoopIndex[(size_t) slot].store (loopIndex);
    uiRevision.fetch_add (1);
}

void LoopDeckAudioProcessor::clearSlot (int slot)
{
    if (! juce::isPositiveAndBelow (slot, numSlots))
        return;

    engine.setPattern (slot, nullptr);
    slotLoopIndex[(size_t) slot].store (-1);
    uiRevision.fetch_add (1);
}

int LoopDeckAudioProcessor::getLoopIndexForSlot (int slot) const noexcept
{
    return juce::isPositiveAndBelow (slot, numSlots) ? slotLoopIndex[(size_t) slot].load() : -1;
}

juce::String LoopDeckAudioProcessor::getLoopNameForSlot (int slot) const
{
    const auto index = getLoopIndexForSlot (slot);

    if (const auto* info = library.getInfo (index))
        return info->name;

    return "Empty";
}

void LoopDeckAudioProcessor::setSelectedSlot (int slot)
{
    if (juce::isPositiveAndBelow (slot, numSlots))
        selectedSlot.store (slot);
}

//==============================================================================
DrumMap LoopDeckAudioProcessor::getEffectiveDrumMap (int slot) const
{
    const juce::ScopedLock lock (mapLock);

    if (! juce::isPositiveAndBelow (slot, numSlots))
        return makeIdentityDrumMap();

    const auto preset = (int) raw (params::slotId (slot, params::slotMap));
    return makeDrumMapPreset (preset, customMaps[(size_t) slot]);
}

bool LoopDeckAudioProcessor::isCustomDrumMap (int slot) const
{
    return rawInt (params::slotId (slot, params::slotMap)) == (int) DrumMapPreset::custom;
}

void LoopDeckAudioProcessor::setCustomNoteMapping (int slot, int inputNote, int outputNote)
{
    if (! juce::isPositiveAndBelow (slot, numSlots)
        || ! juce::isPositiveAndBelow (inputNote, 128)
        || ! juce::isPositiveAndBelow (outputNote, 128))
        return;

    auto map = getEffectiveDrumMap (slot);
    map[(size_t) inputNote] = (juce::uint8) outputNote;

    {
        const juce::ScopedLock lock (mapLock);
        customMaps[(size_t) slot] = map;
        lastMapPreset[(size_t) slot] = (int) DrumMapPreset::custom;
    }

    if (auto* parameter = apvts.getParameter (params::slotId (slot, params::slotMap)))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) DrumMapPreset::custom));

    engine.setDrumMap (slot, map);
    uiRevision.fetch_add (1);
}

void LoopDeckAudioProcessor::resetDrumMap (int slot)
{
    if (! juce::isPositiveAndBelow (slot, numSlots))
        return;

    const auto identity = makeIdentityDrumMap();

    {
        const juce::ScopedLock lock (mapLock);
        customMaps[(size_t) slot] = identity;
        lastMapPreset[(size_t) slot] = (int) DrumMapPreset::custom;
    }

    if (auto* parameter = apvts.getParameter (params::slotId (slot, params::slotMap)))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) DrumMapPreset::custom));

    engine.setDrumMap (slot, identity);
    uiRevision.fetch_add (1);
}

void LoopDeckAudioProcessor::copyDrumMap (int fromSlot, int toSlot)
{
    if (! juce::isPositiveAndBelow (fromSlot, numSlots) || ! juce::isPositiveAndBelow (toSlot, numSlots))
        return;

    const auto source = getEffectiveDrumMap (fromSlot);

    {
        const juce::ScopedLock lock (mapLock);
        customMaps[(size_t) toSlot] = source;
        lastMapPreset[(size_t) toSlot] = (int) DrumMapPreset::custom;
    }

    if (auto* parameter = apvts.getParameter (params::slotId (toSlot, params::slotMap)))
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) DrumMapPreset::custom));

    engine.setDrumMap (toSlot, source);
    uiRevision.fetch_add (1);
}

void LoopDeckAudioProcessor::togglePlayStop()
{
    if (isHostTransportAvailable())
        return;

    engine.setManualTransport (! engine.isManualTransportRunning());
}

//==============================================================================
void LoopDeckAudioProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    auto state = apvts.copyState();

    state.setProperty ("version", pluginVersion, nullptr);

    for (int slot = 0; slot < numSlots; ++slot)
    {
        state.setProperty ("loop" + juce::String (slot), getLoopIndexForSlot (slot), nullptr);

        const auto map = getEffectiveDrumMap (slot);
        state.setProperty ("map" + juce::String (slot),
                           juce::MemoryBlock (map.data(), map.size()), nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destination);
}

void LoopDeckAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    const auto tree = juce::ValueTree::fromXml (*xml);

    if (! tree.isValid())
        return;

    apvts.replaceState (tree);

    for (int slot = 0; slot < numSlots; ++slot)
    {
        const auto mapBlock = tree.getProperty ("map" + juce::String (slot));

        if (auto* block = mapBlock.getBinaryData())
        {
            if (block->getSize() == (size_t) 128)
            {
                const juce::ScopedLock lock (mapLock);
                std::memcpy (customMaps[(size_t) slot].data(), block->getData(), 128);
            }
        }

        const auto index = (int) tree.getProperty ("loop" + juce::String (slot), -1);

        if (library.isValidIndex (index))
            loadLoopIntoSlot (slot, index);
        else
            clearSlot (slot);
    }

    lastMapPreset.fill (-1);
    uiRevision.fetch_add (1);
}

} // namespace loopdeck

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new loopdeck::LoopDeckAudioProcessor();
}
