#include "MidiEngine.h"

#include <algorithm>
#include <cmath>

namespace loopdeck
{

namespace
{
    int wrapNote (int note)
    {
        return juce::jlimit (0, 127, note);
    }
}

double getLengthChoiceBeats (int choice)
{
    static constexpr double barCounts[numLengthChoices] = { 0.0, 1.0, 2.0, 4.0, 8.0, 16.0 };

    return juce::isPositiveAndBelow (choice, numLengthChoices) ? barCounts[choice] * 4.0 : 0.0;
}

double getSpeedChoiceMultiplier (int choice)
{
    static constexpr double multipliers[numSpeedChoices] = { 0.25, 0.5, 1.0, 2.0, 4.0 };

    return juce::isPositiveAndBelow (choice, numSpeedChoices) ? multipliers[choice] : 1.0;
}

//==============================================================================
std::shared_ptr<const LoopPattern> LoopPattern::fromMidiFile (const juce::MidiFile& file,
                                                              double fallbackLengthBeats,
                                                              juce::String patternName)
{
    auto pattern = std::make_shared<LoopPattern>();
    pattern->name = std::move (patternName);
    pattern->lengthBeats = fallbackLengthBeats > 0.0 ? fallbackLengthBeats : 4.0;

    auto ppq = file.getTimeFormat();

    if (ppq <= 0)
        ppq = 480;

    for (int trackIndex = 0; trackIndex < file.getNumTracks(); ++trackIndex)
    {
        const auto* track = file.getTrack (trackIndex);

        if (track == nullptr)
            continue;

        for (int eventIndex = 0; eventIndex < track->getNumEvents(); ++eventIndex)
        {
            const auto* holder = track->getEventPointer (eventIndex);

            if (holder == nullptr)
                continue;

            const auto& message = holder->message;

            if (! message.isNoteOn() || message.getVelocity() <= 0)
                continue;

            PatternNote note;
            note.startBeats = message.getTimeStamp() / (double) ppq;
            note.channel = message.getChannel();
            note.note = message.getNoteNumber();
            note.velocity = message.getVelocity();

            auto endBeats = note.startBeats + 0.25;

            if (holder->noteOffObject != nullptr)
                endBeats = holder->noteOffObject->message.getTimeStamp() / (double) ppq;

            note.lengthBeats = juce::jmax (0.02, endBeats - note.startBeats);
            pattern->notes.push_back (note);
        }
    }

    std::stable_sort (pattern->notes.begin(), pattern->notes.end(),
                      [] (const PatternNote& a, const PatternNote& b)
                      {
                          if (! juce::approximatelyEqual (a.startBeats, b.startBeats))
                              return a.startBeats < b.startBeats;

                          return a.note < b.note;
                      });

    return pattern;
}

//==============================================================================
bool MidiEngine::WarpKey::operator== (const WarpKey& other) const noexcept
{
    return channel == other.channel
        && transpose == other.transpose
        && lengthChoice == other.lengthChoice
        && direction == other.direction
        && mapPreset == other.mapPreset
        && fixedVelocity == other.fixedVelocity
        && juce::approximatelyEqual (speed, other.speed)
        && juce::approximatelyEqual (gate, other.gate)
        && juce::approximatelyEqual (velocityScale, other.velocityScale)
        && juce::approximatelyEqual (swing, other.swing)
        && juce::approximatelyEqual (startTrim, other.startTrim);
}

MidiEngine::WarpKey MidiEngine::WarpKey::from (const SlotSettings& slot)
{
    WarpKey key;
    key.channel = slot.channel;
    key.transpose = slot.transpose;
    key.lengthChoice = slot.lengthChoice;
    key.direction = slot.direction;
    key.mapPreset = slot.mapPreset;
    key.fixedVelocity = slot.fixedVelocity;
    key.speed = slot.speed;
    key.gate = slot.gate;
    key.velocityScale = slot.velocityScale;
    key.swing = slot.swing;
    key.startTrim = slot.startTrim;
    return key;
}

//==============================================================================
MidiEngine::MidiEngine()
{
    for (int i = 0; i < numSlots; ++i)
        drumMaps[(size_t) i] = makeIdentityDrumMap();
}

MidiEngine::~MidiEngine() = default;

void MidiEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    reset();
}

void MidiEngine::reset()
{
    const juce::ScopedLock lock (stateLock);

    pendingOffs.clear();
    freePpq = 0.0;
    padPpq = 0.0;
    lastHostEndPpq = 0.0;
    transportWasPlaying = false;
    lastFreeRunning = false;
    lastClockTime = -1.0;
    clockTimeNow = 0.0;
    usedChannelMask = 0;
    activity.store (0.0f);

    for (auto& slot : slots)
    {
        slot.triggered = false;
        slot.oneShotActive = false;
        slot.wasAudible = false;
        slot.phase.store (0.0f);
        slot.running.store (false);
        slot.resetRequested.store (false);
    }
}

void MidiEngine::setSettings (const EngineSettings& newSettings)
{
    std::array<std::shared_ptr<const LoopPattern>, numSlots> sources;
    std::array<DrumMap, numSlots> maps;
    std::array<bool, numSlots> needsRebuild {};
    bool anyRebuild = false;

    {
        const juce::ScopedLock lock (stateLock);

        settings = newSettings;

        for (int i = 0; i < numSlots; ++i)
        {
            const auto key = WarpKey::from (newSettings.slots[(size_t) i]);
            needsRebuild[(size_t) i] = ! (key == lastWarpKeys[(size_t) i]);
            lastWarpKeys[(size_t) i] = key;
            anyRebuild = anyRebuild || needsRebuild[(size_t) i];

            sources[(size_t) i] = slots[(size_t) i].source;
            maps[(size_t) i] = drumMaps[(size_t) i];
        }
    }

    if (! anyRebuild)
        return;

    // Warping allocates, so it happens outside the lock.
    for (int i = 0; i < numSlots; ++i)
    {
        if (! needsRebuild[(size_t) i])
            continue;

        auto warped = buildWarpedPattern (sources[(size_t) i], newSettings.slots[(size_t) i], maps[(size_t) i]);

        const juce::ScopedLock lock (stateLock);
        slots[(size_t) i].warped = warped;
        slots[(size_t) i].resetRequested.store (true);
    }
}

EngineSettings MidiEngine::getSettings() const
{
    const juce::ScopedLock lock (stateLock);
    return settings;
}

void MidiEngine::setDrumMap (int slot, const DrumMap& map)
{
    if (! juce::isPositiveAndBelow (slot, numSlots))
        return;

    std::shared_ptr<const LoopPattern> source;
    SlotSettings slotSettings;

    {
        const juce::ScopedLock lock (stateLock);
        drumMaps[(size_t) slot] = map;
        source = slots[(size_t) slot].source;
        slotSettings = settings.slots[(size_t) slot];
    }

    auto warped = buildWarpedPattern (source, slotSettings, map);

    const juce::ScopedLock lock (stateLock);
    slots[(size_t) slot].warped = warped;
    slots[(size_t) slot].resetRequested.store (true);
}

DrumMap MidiEngine::getDrumMap (int slot) const
{
    const juce::ScopedLock lock (stateLock);
    return juce::isPositiveAndBelow (slot, numSlots) ? drumMaps[(size_t) slot] : makeIdentityDrumMap();
}

void MidiEngine::setPattern (int slot, std::shared_ptr<const LoopPattern> pattern)
{
    if (! juce::isPositiveAndBelow (slot, numSlots))
        return;

    SlotSettings slotSettings;
    DrumMap map;

    {
        const juce::ScopedLock lock (stateLock);
        slots[(size_t) slot].source = pattern;
        slotSettings = settings.slots[(size_t) slot];
        map = drumMaps[(size_t) slot];
    }

    auto warped = buildWarpedPattern (pattern, slotSettings, map);

    const juce::ScopedLock lock (stateLock);
    slots[(size_t) slot].warped = warped;
    slots[(size_t) slot].resetRequested.store (true);
}

void MidiEngine::clearPatterns()
{
    for (int i = 0; i < numSlots; ++i)
        setPattern (i, nullptr);
}

std::shared_ptr<const MidiEngine::WarpedPattern> MidiEngine::buildWarpedPattern (
    const std::shared_ptr<const LoopPattern>& source, const SlotSettings& slotSettings, const DrumMap& map)
{
    if (source == nullptr || source->notes.empty())
        return nullptr;

    auto out = std::make_shared<WarpedPattern>();

    const auto speed = juce::jlimit (0.125, 8.0, slotSettings.speed);
    const auto gate = juce::jlimit (0.01, 8.0, slotSettings.gate);
    const auto velocityScale = juce::jlimit (0.0, 4.0, slotSettings.velocityScale);
    const auto swing = juce::jlimit (0.0, 0.75, slotSettings.swing);
    const auto trim = juce::jlimit (0.0, 1.0, slotSettings.startTrim);

    const auto fileWindow = juce::jmax (0.25, source->lengthBeats / speed);
    const auto requestedWindow = getLengthChoiceBeats (slotSettings.lengthChoice);

    const auto playLength = requestedWindow > 0.0 ? juce::jmax (0.5, requestedWindow) : fileWindow;

    const bool reverse = slotSettings.direction == 1;
    const bool pingPong = slotSettings.direction == 2;

    const auto trimBeats = trim * playLength;
    const auto mapHasPreset = (DrumMapPreset) slotSettings.mapPreset != DrumMapPreset::custom;
    juce::ignoreUnused (mapHasPreset);

    out->notes.reserve (source->notes.size() * (pingPong ? (size_t) 2 : (size_t) 1));

    for (const auto& note : source->notes)
    {
        auto start = note.startBeats / speed;
        auto length = juce::jmax (0.02, note.lengthBeats * gate / speed);

        if (start >= playLength)
            continue;

        start -= trimBeats;

        while (start < 0.0)
            start += playLength;

        length = juce::jmin (length, playLength);

        const auto velocity = slotSettings.fixedVelocity > 0
                                ? slotSettings.fixedVelocity
                                : juce::roundToInt ((double) note.velocity * velocityScale);

        const auto mapped = (int) map[(size_t) wrapNote (note.note)];
        const auto outNote = wrapNote (mapped + slotSettings.transpose);
        const auto outChannel = slotSettings.channel > 0 ? slotSettings.channel : note.channel;
        const auto outVelocity = juce::jlimit (1, 127, velocity);

        const auto addNote = [&] (double on)
        {
            auto swingOffset = 0.0;

            if (swing > 0.0)
            {
                const auto gridIndex = std::llround (on / 0.25);

                if ((gridIndex % 2) != 0)
                    swingOffset = swing * 0.25;
            }

            WarpedNote warped;
            warped.on = juce::jmax (0.0, on + swingOffset);
            warped.length = length;
            warped.channel = juce::jlimit (1, 16, outChannel);
            warped.note = outNote;
            warped.velocity = outVelocity;
            out->notes.push_back (warped);
        };

        if (reverse)
        {
            addNote (playLength - (start + length));
        }
        else
        {
            addNote (start);

            if (pingPong)
                addNote (playLength * 2.0 - (start + length));
        }
    }

    std::stable_sort (out->notes.begin(), out->notes.end(),
                      [] (const WarpedNote& a, const WarpedNote& b)
                      {
                          if (! juce::approximatelyEqual (a.on, b.on))
                              return a.on < b.on;

                          return a.note < b.note;
                      });

    out->lengthBeats = pingPong ? playLength * 2.0 : playLength;
    return out;
}

//==============================================================================
void MidiEngine::setManualTransport (bool shouldPlay)
{
    manualRunning.store (shouldPlay);
    restartRequested.store (true);
    panicRequested.store (true);
}

void MidiEngine::addEvent (std::vector<ScheduledEvent>& events, int order, int samplePosition,
                           const juce::MidiMessage& message)
{
    ScheduledEvent event;
    event.samplePosition = samplePosition;
    event.order = order;
    event.message = message;
    events.push_back (event);
}

void MidiEngine::flushSlot (int slot, std::vector<ScheduledEvent>& events, int samplePosition)
{
    for (size_t i = 0; i < pendingOffs.size();)
    {
        if (pendingOffs[i].slot == slot)
        {
            addEvent (events, 0, samplePosition, pendingOffs[i].message);
            pendingOffs.erase (pendingOffs.begin() + (std::ptrdiff_t) i);
        }
        else
        {
            ++i;
        }
    }
}

void MidiEngine::flushAll (std::vector<ScheduledEvent>& events, int samplePosition, bool sendAllNotesOff)
{
    for (const auto& pending : pendingOffs)
        addEvent (events, 0, samplePosition, pending.message);

    pendingOffs.clear();

    if (! sendAllNotesOff)
        return;

    for (int channel = 1; channel <= 16; ++channel)
        if ((usedChannelMask & (1 << (channel - 1))) != 0)
            addEvent (events, 0, samplePosition, juce::MidiMessage::allNotesOff (channel));
}

void MidiEngine::emitNoteOn (std::vector<ScheduledEvent>& events, std::vector<PendingOff>& offs,
                             int slot, int samplePosition, const WarpedNote& note)
{
    for (size_t i = 0; i < offs.size(); ++i)
    {
        if (offs[i].message.getChannel() == note.channel
            && offs[i].message.getNoteNumber() == note.note)
        {
            addEvent (events, 0, samplePosition, offs[i].message);
            offs.erase (offs.begin() + (std::ptrdiff_t) i);
            break;
        }
    }

    addEvent (events, 1, samplePosition,
              juce::MidiMessage::noteOn (note.channel, note.note, (juce::uint8) note.velocity));

    PendingOff pending;
    pending.beatsRemaining = note.length;
    pending.message = juce::MidiMessage::noteOff (note.channel, note.note);
    pending.slot = slot;
    offs.push_back (pending);

    usedChannelMask |= 1 << (note.channel - 1);
}

void MidiEngine::handleTransportMessage (const juce::MidiMessage& message)
{
    if (message.isMidiStart())
    {
        freePpq = 0.0;
        manualRunning.store (true);
        panicRequested.store (true);
        lastClockTime = -1.0;
    }
    else if (message.isMidiContinue())
    {
        manualRunning.store (true);
    }
    else if (message.isMidiStop())
    {
        manualRunning.store (false);
        panicRequested.store (true);
        lastClockTime = -1.0;
    }
    else if (message.isSongPositionPointer())
    {
        freePpq = message.getSongPositionPointerMidiBeat();
        panicRequested.store (true);
    }
}

void MidiEngine::startSlot (int slot, bool oneShot, double startTime)
{
    std::shared_ptr<const WarpedPattern> warped;

    {
        const juce::ScopedLock lock (stateLock);
        warped = slots[(size_t) slot].warped;
    }

    const auto length = warped != nullptr ? warped->lengthBeats : 4.0;
    auto& runtime = slots[(size_t) slot];

    if (oneShot)
    {
        runtime.oneShotActive = true;
        runtime.oneShotStart = startTime;
        runtime.oneShotEnd = startTime + length;
    }
    else
    {
        runtime.triggered = true;
        runtime.triggerAnchor = startTime;
    }
}

//==============================================================================
void MidiEngine::process (juce::MidiBuffer& midi, int numSamples, const TransportSnapshot& transport)
{
    if (numSamples <= 0)
        return;

    const auto sr = sampleRate > 0.0 ? sampleRate : 44100.0;

    EngineSettings snapshot;
    std::array<std::shared_ptr<const WarpedPattern>, numSlots> warped;

    {
        const juce::ScopedLock lock (stateLock);
        snapshot = settings;

        for (int i = 0; i < numSlots; ++i)
            warped[(size_t) i] = slots[(size_t) i].warped;
    }

    std::vector<ScheduledEvent> events;
    events.reserve (256);

    if (restartRequested.exchange (false))
    {
        freePpq = 0.0;
        lastFreeRunning = false;
    }

    bool anySolo = false;

    for (const auto& slot : snapshot.slots)
        anySolo = anySolo || slot.solo;

    // ---- clock ------------------------------------------------------------
    const bool hostClock = snapshot.clockSource == 0 && transport.hasPosition;
    auto bpm = snapshot.internalBpm > 1.0 ? snapshot.internalBpm : 120.0;

    if (snapshot.clockSource == 0 && transport.bpm > 1.0)
        bpm = transport.bpm;

    if (snapshot.clockSource == 2)
    {
        const auto measured = clockBpm.load();

        if (measured > 10.0)
            bpm = measured;
    }

    const auto blockBeats = juce::jmax (0.0, (double) numSamples / sr * bpm / 60.0);
    const auto blockSeconds = (double) numSamples / sr;

    if (lastClockTime >= 0.0 && clockTimeNow - lastClockTime > 2.0)
    {
        lastClockTime = -1.0;
        clockBpm.store (0.0);
    }

    const bool transportRunning = hostClock ? transport.isPlaying : manualRunning.load();
    const double timelineStart = hostClock ? transport.ppq : freePpq;

    // ---- transport change detection ---------------------------------------
    bool needFlush = false;

    if (hostClock)
    {
        if (transportWasPlaying && ! transport.isPlaying)
            needFlush = true;
        else if (transport.isPlaying && transportWasPlaying
                 && std::abs (timelineStart - lastHostEndPpq) > 0.05)
            needFlush = true;

        lastHostEndPpq = timelineStart + blockBeats;
        transportWasPlaying = transport.isPlaying;
        lastFreeRunning = false;
    }
    else
    {
        if (lastFreeRunning && ! transportRunning)
            needFlush = true;

        lastFreeRunning = transportRunning;
        transportWasPlaying = false;
    }

    if (panicRequested.exchange (false))
        needFlush = true;

    if (needFlush)
    {
        if (snapshot.triggerMode != 0)
        {
            for (int i = 0; i < numSlots; ++i)
                flushSlot (i, events, 0);
        }
        else
        {
            flushAll (events, 0, snapshot.allNotesOffOnStop);
        }
    }

    for (int i = 0; i < numSlots; ++i)
        if (slots[(size_t) i].resetRequested.exchange (false))
            flushSlot (i, events, 0);

    // ---- incoming MIDI -----------------------------------------------------
    bool emittedNote = false;

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        const auto position = juce::jlimit (0, numSamples - 1, metadata.samplePosition);

        if (message.isMidiClock())
        {
            const auto now = clockTimeNow + (double) position / sr;

            if (lastClockTime >= 0.0 && now > lastClockTime)
            {
                const auto interval = now - lastClockTime;
                const auto measured = 60.0 / (interval * 24.0);

                if (measured > 20.0 && measured < 400.0)
                {
                    const auto previous = clockBpm.load();
                    clockBpm.store (previous > 0.0 ? previous * 0.85 + measured * 0.15 : measured);
                }
            }

            lastClockTime = now;
            continue;
        }

        if (message.isMidiStart() || message.isMidiStop()
            || message.isMidiContinue() || message.isSongPositionPointer())
        {
            if (snapshot.followMidiTransport || snapshot.clockSource == 2)
                handleTransportMessage (message);

            continue;
        }

        const bool passesChannel = message.getChannel() <= 0
                                    || snapshot.inputChannel == 0
                                    || message.getChannel() == snapshot.inputChannel;

        if (! passesChannel)
            continue;

        if (snapshot.triggerMode != 0 && message.isNoteOnOrOff())
        {
            const auto slotIndex = message.getNoteNumber() - snapshot.triggerNote;

            if (juce::isPositiveAndBelow (slotIndex, numSlots))
            {
                if (message.isNoteOn())
                {
                    const auto startTime = padPpq + blockBeats * (double) position / (double) numSamples;

                    startSlot (slotIndex, snapshot.triggerMode == 2, startTime);
                }
                else if (snapshot.triggerMode == 1)
                {
                    slots[(size_t) slotIndex].triggered = false;
                    flushSlot (slotIndex, events, position);
                }

                continue;
            }
        }

        if (snapshot.thru)
        {
            auto forwarded = message;

            if (snapshot.thruChannel > 0 && forwarded.getChannel() > 0)
                forwarded.setChannel (snapshot.thruChannel);

            addEvent (events, 2, position, forwarded);
        }
    }

    // ---- scheduled notes ---------------------------------------------------
    for (int i = 0; i < numSlots; ++i)
    {
        const auto& slotSettings = snapshot.slots[(size_t) i];
        auto& runtime = slots[(size_t) i];

        const bool audible = snapshot.master
                             && slotSettings.enabled
                             && ! slotSettings.mute
                             && (! anySolo || slotSettings.solo)
                             && warped[(size_t) i] != nullptr;

        if (! audible)
        {
            if (runtime.wasAudible)
            {
                flushSlot (i, events, 0);
                runtime.wasAudible = false;
            }

            runtime.running.store (false);
            runtime.phase.store (0.0f);
            continue;
        }

        runtime.wasAudible = true;

        const bool manualClock = snapshot.triggerMode != 0;

        if (manualClock)
        {
            if (runtime.oneShotActive && padPpq >= runtime.oneShotEnd - 1.0e-9)
            {
                runtime.oneShotActive = false;
                flushSlot (i, events, 0);
            }

            if (! runtime.triggered && ! runtime.oneShotActive)
            {
                runtime.running.store (false);
                runtime.phase.store (0.0f);
                continue;
            }
        }
        else if (! transportRunning)
        {
            runtime.running.store (false);
            continue;
        }

        const auto& pattern = *warped[(size_t) i];
        const auto loopLength = pattern.lengthBeats;

        if (loopLength <= 1.0e-6 || pattern.notes.empty() || blockBeats <= 1.0e-9)
            continue;

        const auto t0 = manualClock ? padPpq : timelineStart;
        const auto t1 = t0 + blockBeats;
        const auto anchor = manualClock
                              ? (runtime.oneShotActive ? runtime.oneShotStart : runtime.triggerAnchor)
                              : 0.0;

        const auto kFrom = (long long) std::floor ((t0 - anchor) / loopLength) - 1;
        const auto kTo = (long long) std::floor ((t1 - anchor) / loopLength) + 1;

        for (auto k = kFrom; k <= kTo; ++k)
        {
            const auto cycle = anchor + (double) k * loopLength;

            if (cycle < anchor - 1.0e-9)
                continue;

            if (runtime.oneShotActive && cycle >= runtime.oneShotEnd - 1.0e-9)
                continue;

            for (const auto& note : pattern.notes)
            {
                const auto on = cycle + note.on;

                if (on < t0 - 1.0e-9 || on >= t1)
                    continue;

                const auto fraction = (on - t0) / (t1 - t0);
                const auto position = juce::jlimit (0, numSamples - 1,
                                                    juce::roundToInt (fraction * (double) numSamples));

                emitNoteOn (events, pendingOffs, i, position, note);
                emittedNote = true;
            }
        }

        const auto phaseTime = t1 - anchor;
        runtime.phase.store ((float) (std::fmod (juce::jmax (0.0, phaseTime), loopLength) / loopLength));
        runtime.running.store (true);
    }

    // ---- pending note-offs -------------------------------------------------
    if (blockBeats > 1.0e-9)
    {
        const auto inverseWindow = 1.0 / blockBeats;

        for (size_t i = 0; i < pendingOffs.size();)
        {
            auto& pending = pendingOffs[i];
            pending.beatsRemaining -= blockBeats;

            if (pending.beatsRemaining <= 0.0)
            {
                const auto fraction = juce::jlimit (0.0, 1.0, 1.0 + pending.beatsRemaining * inverseWindow);
                const auto position = juce::jlimit (0, numSamples - 1,
                                                    juce::roundToInt (fraction * (double) juce::jmax (1, numSamples - 1)));
                addEvent (events, 0, position, pending.message);
                pendingOffs.erase (pendingOffs.begin() + (std::ptrdiff_t) i);
            }
            else
            {
                ++i;
            }
        }
    }

    // ---- write output ------------------------------------------------------
    midi.clear();

    if (! events.empty())
    {
        std::stable_sort (events.begin(), events.end());

        for (const auto& event : events)
            if (juce::isPositiveAndBelow (event.samplePosition, numSamples))
                midi.addEvent (event.message, event.samplePosition);
    }

    if (emittedNote)
        activity.store (1.0f);
    else
        activity.store (juce::jmax (0.0f, activity.load() - (float) (blockSeconds * 4.0)));

    freePpq += blockBeats;
    padPpq += blockBeats;
    clockTimeNow += blockSeconds;
}

//==============================================================================
float MidiEngine::getSlotPhase (int slot) const noexcept
{
    return juce::isPositiveAndBelow (slot, numSlots) ? slots[(size_t) slot].phase.load() : 0.0f;
}

bool MidiEngine::isSlotRunning (int slot) const noexcept
{
    return juce::isPositiveAndBelow (slot, numSlots) && slots[(size_t) slot].running.load();
}

juce::String MidiEngine::getStatusText() const
{
    const juce::ScopedLock lock (stateLock);

    if (settings.clockSource == 0)
        return "HOST";

    if (settings.clockSource == 2)
    {
        const auto measured = clockBpm.load();
        return measured > 10.0 ? "CLOCK " + juce::String (measured, 1) : juce::String ("CLOCK");
    }

    return "INT " + juce::String (settings.internalBpm, 1);
}

} // namespace loopdeck
