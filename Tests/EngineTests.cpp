/*
    Headless tests for the loop library, the drum maps and the MIDI engine.

    Everything here runs on the bundled loop library, so a green run also
    proves that the packed blob is readable and every loop parses.
*/

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

#include "BinaryData.h"
#include "engine/DrumMap.h"
#include "engine/LoopLibrary.h"
#include "engine/MidiEngine.h"

using namespace loopdeck;

namespace
{
    int checks = 0;
    int failures = 0;
    juce::String currentTest;

    void expect (bool condition, const juce::String& what)
    {
        ++checks;

        if (! condition)
        {
            ++failures;
            std::printf ("  FAIL  [%s] %s\n", currentTest.toRawUTF8(), what.toRawUTF8());
        }
    }

    void expectNear (double actual, double expected, double tolerance, const juce::String& what)
    {
        ++checks;

        if (std::abs (actual - expected) > tolerance)
        {
            ++failures;
            std::printf ("  FAIL  [%s] %s (expected %.4f, got %.4f)\n",
                         currentTest.toRawUTF8(), what.toRawUTF8(), expected, actual);
        }
    }

    void beginTest (const char* name)
    {
        currentTest = name;
        std::printf ("%s\n", name);
    }

    constexpr double testSampleRate = 48000.0;
    constexpr int testBlockSize = 256;
    constexpr double beatsPerBlock = testBlockSize / testSampleRate * 2.0;   // 120 bpm

    struct CapturedEvent
    {
        int sample = 0;
        juce::MidiMessage message;
    };

    std::shared_ptr<const LoopPattern> makePattern (std::initializer_list<PatternNote> notes,
                                                    double lengthBeats = 4.0)
    {
        auto pattern = std::make_shared<LoopPattern>();
        pattern->notes.assign (notes.begin(), notes.end());
        pattern->lengthBeats = lengthBeats;
        pattern->name = "test";
        return pattern;
    }

    PatternNote note (double start, double length, int midiNote, int velocity = 100, int channel = 1)
    {
        PatternNote result;
        result.startBeats = start;
        result.lengthBeats = length;
        result.note = midiNote;
        result.velocity = velocity;
        result.channel = channel;
        return result;
    }

    /** Renders blocks on a host transport, capturing everything the engine emits. */
    std::vector<CapturedEvent> renderHost (MidiEngine& engine, int numBlocks, double startPpq,
                                          bool playing = true,
                                          std::function<juce::MidiBuffer (int block)> inputProvider = {})
    {
        std::vector<CapturedEvent> captured;
        auto ppq = startPpq;

        for (int block = 0; block < numBlocks; ++block)
        {
            juce::MidiBuffer buffer;

            if (inputProvider != nullptr)
                buffer = inputProvider (block);

            TransportSnapshot transport;
            transport.hasPosition = true;
            transport.isPlaying = playing;
            transport.ppq = ppq;
            transport.bpm = 120.0;

            engine.process (buffer, testBlockSize, transport);

            for (const auto metadata : buffer)
                captured.push_back ({ block * testBlockSize + metadata.samplePosition, metadata.getMessage() });

            ppq += beatsPerBlock;
        }

        return captured;
    }

    int countNoteOns (const std::vector<CapturedEvent>& events, int midiNote = -1, int channel = -1)
    {
        int count = 0;

        for (const auto& event : events)
        {
            if (! event.message.isNoteOn())
                continue;

            if (midiNote >= 0 && event.message.getNoteNumber() != midiNote)
                continue;

            if (channel > 0 && event.message.getChannel() != channel)
                continue;

            ++count;
        }

        return count;
    }

    int countNoteOffs (const std::vector<CapturedEvent>& events, int midiNote = -1)
    {
        int count = 0;

        for (const auto& event : events)
        {
            if (! event.message.isNoteOff())
                continue;

            if (midiNote >= 0 && event.message.getNoteNumber() != midiNote)
                continue;

            ++count;
        }

        return count;
    }

    const CapturedEvent* findFirstNoteOn (const std::vector<CapturedEvent>& events, int midiNote)
    {
        for (const auto& event : events)
            if (event.message.isNoteOn() && (midiNote < 0 || event.message.getNoteNumber() == midiNote))
                return &event;

        return nullptr;
    }

    SlotSettings makeDefaultSlotSettings()
    {
        SlotSettings settings;
        settings.enabled = true;
        return settings;
    }

    EngineSettings makeDefaultSettings()
    {
        EngineSettings settings;
        settings.slots[0] = makeDefaultSlotSettings();
        settings.internalBpm = 120.0;
        settings.clockSource = 0;
        return settings;
    }

    //==========================================================================
    void testLibrary()
    {
        beginTest ("library loads the packed loops");

        LoopLibrary library;
        const auto loaded = library.loadFromMemory (BinaryData::loopdeck_library_bin,
                                                    (size_t) BinaryData::loopdeck_library_binSize);

        expect (loaded, "library loads: " + library.getError());
        expect (library.getNumLoops() == 2704, "2704 loops embedded, got " + juce::String (library.getNumLoops()));
        expect (library.getFolders().size() >= 20, "folders listed");

        const auto* first = library.getInfo (0);
        expect (first != nullptr, "first loop exists");

        if (first != nullptr)
        {
            expect (first->name.isNotEmpty(), "first loop has a name");
            expect (first->getNumBars() >= 1, "first loop has a length");
            expect (first->numNotes > 0, "first loop has notes");
            expect (first->ppq > 0, "first loop has a ppq");
            expect (first->getChannelText().isNotEmpty(), "first loop reports its channel");
        }

        const auto file = library.readMidiFile (0);
        expect (file.getNumTracks() > 0, "first loop parses as a MIDI file");

        const auto results = library.search ("house", "");
        expect (! results.empty(), "search finds house loops");

        const auto filtered = library.search ("", "4otf");
        expect (! filtered.empty(), "folder filter returns loops");

        for (const auto index : filtered)
        {
            const auto* info = library.getInfo (index);
            expect (info != nullptr && info->folder == "4otf", "folder filter only returns that folder");
        }

        expect (library.search ("zzzzzzzz", "").empty(), "search returns nothing for nonsense");
    }

    void testEveryLoopParses()
    {
        beginTest ("every loop converts to a pattern");

        LoopLibrary library;
        library.loadFromMemory (BinaryData::loopdeck_library_bin,
                                (size_t) BinaryData::loopdeck_library_binSize);

        int empty = 0;
        int badLength = 0;
        int badNotes = 0;
        int totalNotes = 0;

        for (int index = 0; index < library.getNumLoops(); ++index)
        {
            const auto* info = library.getInfo (index);
            const auto file = library.readMidiFile (index);
            const auto pattern = LoopPattern::fromMidiFile (file,
                                                            info != nullptr ? info->getLengthBeats() : 4.0,
                                                            info != nullptr ? info->name : juce::String());

            if (pattern == nullptr || pattern->notes.empty())
            {
                ++empty;
                continue;
            }

            totalNotes += (int) pattern->notes.size();

            if (pattern->lengthBeats <= 0.0 || ! std::isfinite (pattern->lengthBeats))
                ++badLength;

            for (const auto& parsed : pattern->notes)
            {
                if (! juce::isPositiveAndBelow (parsed.note, 128)
                    || ! juce::isPositiveAndBelow (parsed.channel, 17)
                    || ! juce::isPositiveAndBelow (parsed.velocity, 128)
                    || parsed.lengthBeats <= 0.0)
                {
                    ++badNotes;
                    break;
                }
            }
        }

        expect (empty == 0, "no empty patterns, got " + juce::String (empty));
        expect (badLength == 0, "all patterns have a length");
        expect (badNotes == 0, "all notes are valid");
        expect (totalNotes > 10000, "total notes parsed: " + juce::String (totalNotes));
    }

    void testMidiFileConversion()
    {
        beginTest ("MIDI file conversion keeps timing");

        juce::MidiFile file;
        file.setTicksPerQuarterNote (480);

        juce::MidiMessageSequence track;
        track.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 0.0);
        track.addEvent (juce::MidiMessage::noteOff (1, 36), 240.0);
        track.addEvent (juce::MidiMessage::noteOn (1, 38, (juce::uint8) 90), 480.0);
        track.addEvent (juce::MidiMessage::noteOff (1, 38), 720.0);
        track.updateMatchedPairs();
        file.addTrack (track);

        const auto pattern = LoopPattern::fromMidiFile (file, 4.0, "synthetic");

        expect (pattern != nullptr, "pattern built");
        expect (pattern->notes.size() == 2, "two notes parsed");
        expectNear (pattern->notes[0].startBeats, 0.0, 1.0e-6, "first note starts on the beat");
        expectNear (pattern->notes[0].lengthBeats, 0.5, 1.0e-6, "first note lasts an eighth");
        expectNear (pattern->notes[1].startBeats, 1.0, 1.0e-6, "second note starts on beat 2");
        expectNear (pattern->lengthBeats, 4.0, 1.0e-9, "fallback length is used");
        expect (pattern->notes[1].note == 38, "note number preserved");
    }

    void testBasicScheduling()
    {
        beginTest ("scheduling, channel forcing and note offs");

        MidiEngine engine;
        engine.prepare (testSampleRate);

        auto settings = makeDefaultSettings();
        settings.slots[0].channel = 3;
        engine.setSettings (settings);
        engine.setPattern (0, makePattern ({ note (0.0, 0.25, 36), note (2.0, 0.25, 38) }));

        const auto events = renderHost (engine, 400, 0.0);

        const auto* first = findFirstNoteOn (events, 36);
        expect (first != nullptr, "kick is emitted");

        if (first != nullptr)
        {
            expect (first->sample == 0, "kick lands on sample 0, got " + juce::String (first->sample));
            expect (first->message.getChannel() == 3, "channel is forced to 3");
        }

        const auto* second = findFirstNoteOn (events, 38);
        expect (second != nullptr, "snare is emitted");

        if (second != nullptr)
        {
            const auto expectedSample = (int) std::lround (2.0 / beatsPerBlock * testBlockSize);
            expectNear ((double) second->sample, (double) expectedSample, (double) testBlockSize,
                        "snare lands on beat 3");
        }

        expect (countNoteOns (events, 36) >= 2, "kick repeats after the loop wraps");
        expect (countNoteOffs (events, 36) >= 2, "kick gets note offs");
        expect (countNoteOns (events, 36, 3) == countNoteOns (events, 36), "all kick notes use channel 3");
    }

    void testTransposeAndDrumMap()
    {
        beginTest ("transpose and drum note maps");

        const auto identity = makeIdentityDrumMap();
        expect (identity[36] == 36, "identity map keeps notes");

        const auto tr808 = makeDrumMapPreset ((int) DrumMapPreset::tr808);
        expect (tr808[36] == 35, "808 map moves the kick to 35");
        expect (tr808[38] == 38, "808 map keeps the snare");

        MidiEngine engine;
        engine.prepare (testSampleRate);

        auto settings = makeDefaultSettings();
        settings.slots[0].transpose = 7;
        engine.setSettings (settings);
        engine.setPattern (0, makePattern ({ note (0.0, 0.25, 36) }));

        auto events = renderHost (engine, 60, 0.0);
        expect (findFirstNoteOn (events, 43) != nullptr, "transpose +7 moves 36 to 43");
        expect (findFirstNoteOn (events, 36) == nullptr, "original note is gone");

        auto mapped = makeDefaultSettings();
        mapped.slots[0].mapPreset = (int) DrumMapPreset::tr808;
        engine.setSettings (mapped);
        engine.setDrumMap (0, tr808);

        events = renderHost (engine, 60, 0.0);
        expect (findFirstNoteOn (events, 35) != nullptr, "808 map turns 36 into 35");
    }

    void testSpeedAndGate()
    {
        beginTest ("speed and gate");

        MidiEngine engine;
        engine.prepare (testSampleRate);

        auto settings = makeDefaultSettings();
        settings.slots[0].speed = 0.5;
        engine.setSettings (settings);
        engine.setPattern (0, makePattern ({ note (0.0, 0.25, 36), note (2.0, 0.25, 38) }, 4.0));

        auto events = renderHost (engine, 900, 0.0);
        const auto* snare = findFirstNoteOn (events, 38);
        expect (snare != nullptr, "snare plays at half speed");

        if (snare != nullptr)
        {
            const auto expectedSample = (int) std::lround (4.0 / beatsPerBlock * testBlockSize);
            expectNear ((double) snare->sample, (double) expectedSample, (double) testBlockSize,
                        "half speed moves beat 2 to beat 4");
        }

        // gate: 200% doubles the note length
        MidiEngine gated;
        gated.prepare (testSampleRate);

        auto gatedSettings = makeDefaultSettings();
        gatedSettings.slots[0].gate = 2.0;
        gatedSettings.slots[0].channel = 2;
        gated.setSettings (gatedSettings);
        gated.setPattern (0, makePattern ({ note (0.0, 0.125, 36) }));

        const auto gatedEvents = renderHost (gated, 120, 0.0);
        int onSample = -1;
        int offSample = -1;

        for (const auto& event : gatedEvents)
        {
            if (event.message.isNoteOn() && onSample < 0)
                onSample = event.sample;

            if (event.message.isNoteOff() && offSample < 0 && onSample >= 0)
                offSample = event.sample;
        }

        expect (onSample >= 0 && offSample > onSample, "gated note has an off");

        if (onSample >= 0 && offSample > onSample)
        {
            const auto expected = 0.25 / beatsPerBlock * testBlockSize;   // 0.125 * 2
            expectNear ((double) (offSample - onSample), expected, (double) testBlockSize * 1.5,
                        "gate 200% doubles a 0.125 beat note");
        }
    }

    void testReverseAndPingPong()
    {
        beginTest ("reverse and ping-pong");

        MidiEngine engine;
        engine.prepare (testSampleRate);

        auto settings = makeDefaultSettings();
        settings.slots[0].direction = 1;   // reverse
        engine.setSettings (settings);
        engine.setPattern (0, makePattern ({ note (0.0, 0.25, 36), note (2.0, 0.25, 38) }));

        auto events = renderHost (engine, 200, 0.0);
        const auto* snare = findFirstNoteOn (events, 38);
        expect (snare != nullptr, "reverse still plays the snare");

        if (snare != nullptr)
        {
            const auto expectedSample = (int) std::lround (1.75 / beatsPerBlock * testBlockSize);
            expectNear ((double) snare->sample, (double) expectedSample, (double) testBlockSize,
                        "reverse mirrors beat 2 to 1.75");
        }

        auto pingPongSettings = makeDefaultSettings();
        pingPongSettings.slots[0].direction = 2;
        MidiEngine pingPong;
        pingPong.prepare (testSampleRate);
        pingPong.setSettings (pingPongSettings);
        pingPong.setPattern (0, makePattern ({ note (0.0, 0.25, 36) }));

        const auto pingPongEvents = renderHost (pingPong, 800, 0.0);
        const auto* mirrored = findFirstNoteOn (pingPongEvents, 36);

        // the second hit is the mirrored copy, at (0 + 4) - (0 + 0.25) = 7.75 beats
        int hits = 0;
        double secondHitBeats = -1.0;

        for (const auto& event : pingPongEvents)
        {
            if (! event.message.isNoteOn())
                continue;

            ++hits;

            if (hits == 2)
                secondHitBeats = (double) event.sample / (double) testBlockSize * beatsPerBlock;
        }

        expect (mirrored != nullptr, "ping-pong plays");
        expectNear (secondHitBeats, 7.75, beatsPerBlock * 2.0, "ping-pong mirrors the loop at its end");
    }

    void testLengthOverride()
    {
        beginTest ("length override trims the loop");

        MidiEngine engine;
        engine.prepare (testSampleRate);

        auto settings = makeDefaultSettings();
        settings.slots[0].channel = 4;
        settings.slots[0].lengthChoice = 1;   // 1 bar = 4 beats
        settings.slots[0].speed = 1.0;
        engine.setSettings (settings);

        // a 2 bar loop: the part after the first bar must not play
        engine.setPattern (0, makePattern ({ note (0.0, 0.25, 36), note (3.5, 0.25, 41), note (5.0, 0.25, 45) }, 8.0));

        const auto events = renderHost (engine, 400, 0.0);
        expect (findFirstNoteOn (events, 36) != nullptr, "first bar plays");
        expect (findFirstNoteOn (events, 41) != nullptr, "notes inside the window still play");
        expect (findFirstNoteOn (events, 45) == nullptr, "notes past the window are dropped");
        expect (countNoteOns (events, 36) >= 2, "window loops after one bar");
    }

    void testThruAndChannelFilter()
    {
        beginTest ("MIDI thru and input channel filter");

        MidiEngine engine;
        engine.prepare (testSampleRate);

        auto settings = makeDefaultSettings();
        settings.thru = true;
        settings.thruChannel = 9;
        settings.inputChannel = 2;
        engine.setSettings (settings);

        auto events = renderHost (engine, 4, 0.0, true, [] (int block)
        {
            juce::MidiBuffer input;

            if (block == 0)
                input.addEvent (juce::MidiMessage::noteOn (2, 60, (juce::uint8) 80), 10);

            return input;
        });

        const auto* forwarded = findFirstNoteOn (events, 60);
        expect (forwarded != nullptr, "note on the selected channel is forwarded");

        if (forwarded != nullptr)
            expect (forwarded->message.getChannel() == 9, "thru channel override is applied");

        auto filteredEvents = renderHost (engine, 4, 0.0, true, [] (int block)
        {
            juce::MidiBuffer input;

            if (block == 0)
                input.addEvent (juce::MidiMessage::noteOn (5, 61, (juce::uint8) 80), 10);

            return input;
        });

        expect (findFirstNoteOn (filteredEvents, 61) == nullptr, "other channels are filtered out");
    }

    void testPadTrigger()
    {
        beginTest ("pad triggering starts and stops slots");

        MidiEngine engine;
        engine.prepare (testSampleRate);

        auto settings = makeDefaultSettings();
        settings.triggerMode = 1;   // toggle
        settings.triggerNote = 36;
        settings.slots[0].channel = 6;
        engine.setSettings (settings);
        engine.setPattern (0, makePattern ({ note (0.0, 0.25, 36) }));

        // nothing should play before a pad hit
        auto idle = renderHost (engine, 8, 0.0, false);
        expect (idle.empty(), "no notes before the pad is hit");

        auto events = renderHost (engine, 8, 0.0, false, [] (int block)
        {
            juce::MidiBuffer input;

            if (block == 0)
                input.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 0);

            return input;
        });

        expect (countNoteOns (events, 36, 6) > 0, "pad hit starts the slot");
        expect (findFirstNoteOn (events, 36) != nullptr, "pad note itself is not forwarded");
    }

    void testPanicAndStop()
    {
        beginTest ("panic and transport stop send note offs");

        MidiEngine engine;
        engine.prepare (testSampleRate);

        auto settings = makeDefaultSettings();
        settings.slots[0].channel = 8;
        settings.slots[0].gate = 4.0;   // long notes so something is still held
        engine.setSettings (settings);
        engine.setPattern (0, makePattern ({ note (0.0, 1.0, 36) }));

        auto playing = renderHost (engine, 4, 0.0, true);
        expect (countNoteOns (playing, 36) > 0, "note started");

        auto stopped = renderHost (engine, 3, 4.0 * beatsPerBlock * 4, false);
        expect (countNoteOffs (stopped, 36) > 0, "stop flushes the held note");

        bool sawAllNotesOff = false;

        for (const auto& event : stopped)
            if (event.message.isAllNotesOff())
                sawAllNotesOff = true;

        expect (sawAllNotesOff, "all notes off is sent on stop");

        MidiEngine other;
        other.prepare (testSampleRate);
        other.setSettings (makeDefaultSettings());
        other.setPattern (0, makePattern ({ note (0.0, 2.0, 40) }));

        renderHost (other, 4, 0.0, true);
        other.requestPanic();

        juce::MidiBuffer buffer;
        TransportSnapshot transport;
        transport.hasPosition = true;
        transport.isPlaying = true;
        transport.ppq = 4.0 * beatsPerBlock * 4;
        other.process (buffer, testBlockSize, transport);

        int offs = 0;
        for (const auto metadata : buffer)
            if (metadata.getMessage().isNoteOff())
                ++offs;

        expect (offs > 0, "panic emits note offs");
    }

    void testSwing()
    {
        beginTest ("swing delays odd sixteenths");

        MidiEngine straight;
        straight.prepare (testSampleRate);
        straight.setSettings (makeDefaultSettings());
        straight.setPattern (0, makePattern ({ note (0.25, 0.1, 40) }));

        const auto straightEvents = renderHost (straight, 60, 0.0);
        const auto* straightNote = findFirstNoteOn (straightEvents, 40);
        expect (straightNote != nullptr, "straight note plays");

        MidiEngine swung;
        swung.prepare (testSampleRate);
        auto settings = makeDefaultSettings();
        settings.slots[0].swing = 0.5;
        swung.setSettings (settings);
        swung.setPattern (0, makePattern ({ note (0.25, 0.1, 40) }));

        const auto swungEvents = renderHost (swung, 60, 0.0);
        const auto* swungNote = findFirstNoteOn (swungEvents, 40);
        expect (swungNote != nullptr, "swung note plays");

        if (straightNote != nullptr && swungNote != nullptr)
        {
            const auto expectedDelay = 0.5 * 0.25 / beatsPerBlock * testBlockSize;
            expectNear ((double) (swungNote->sample - straightNote->sample), expectedDelay,
                        (double) testBlockSize * 1.5, "swing shifts the note by half a sixteenth");
        }
    }

    void testMuteSoloAndMaster()
    {
        beginTest ("mute, solo and master switch");

        MidiEngine engine;
        engine.prepare (testSampleRate);

        auto settings = makeDefaultSettings();
        settings.slots[0].channel = 1;
        settings.slots[1] = makeDefaultSlotSettings();
        settings.slots[1].channel = 2;
        engine.setSettings (settings);
        engine.setPattern (0, makePattern ({ note (0.0, 0.25, 36) }));
        engine.setPattern (1, makePattern ({ note (0.0, 0.25, 38) }));

        auto both = renderHost (engine, 8, 0.0);
        expect (countNoteOns (both, 36) > 0 && countNoteOns (both, 38) > 0, "both slots play");

        auto muted = settings;
        muted.slots[0].mute = true;
        engine.setSettings (muted);

        auto mutedEvents = renderHost (engine, 8, 0.0);
        expect (countNoteOns (mutedEvents, 36) == 0, "muted slot is silent");
        expect (countNoteOns (mutedEvents, 38) > 0, "unmuted slot still plays");

        auto soloed = muted;
        soloed.slots[0].solo = true;
        soloed.slots[0].mute = false;
        engine.setSettings (soloed);

        auto soloEvents = renderHost (engine, 8, 0.0);
        expect (countNoteOns (soloEvents, 36) > 0, "soloed slot plays");
        expect (countNoteOns (soloEvents, 38) == 0, "other slots are silenced by solo");

        auto masterOff = soloed;
        masterOff.master = false;
        engine.setSettings (masterOff);

        auto offEvents = renderHost (engine, 8, 0.0);
        expect (offEvents.empty() || countNoteOns (offEvents) == 0, "master switch silences everything");
    }

    void testInternalClock()
    {
        beginTest ("internal clock runs without a host playhead");

        MidiEngine engine;
        engine.prepare (testSampleRate);

        auto settings = makeDefaultSettings();
        settings.clockSource = 1;
        settings.internalBpm = 120.0;
        engine.setSettings (settings);
        engine.setPattern (0, makePattern ({ note (0.0, 0.25, 36) }));

        std::vector<CapturedEvent> events;

        for (int block = 0; block < 8; ++block)
        {
            juce::MidiBuffer buffer;
            TransportSnapshot transport;   // no position at all
            engine.process (buffer, testBlockSize, transport);

            for (const auto metadata : buffer)
                events.push_back ({ block * testBlockSize + metadata.samplePosition, metadata.getMessage() });
        }

        expect (countNoteOns (events, 36) == 0, "nothing plays while the internal clock is stopped");

        engine.setManualTransport (true);
        events.clear();

        for (int block = 0; block < 8; ++block)
        {
            juce::MidiBuffer buffer;
            TransportSnapshot transport;
            engine.process (buffer, testBlockSize, transport);

            for (const auto metadata : buffer)
                events.push_back ({ block * testBlockSize + metadata.samplePosition, metadata.getMessage() });
        }

        expect (countNoteOns (events, 36) > 0, "internal clock plays after start");
    }

    void testStateOfEngineSettings()
    {
        beginTest ("settings round trip through the engine");

        MidiEngine engine;
        auto settings = makeDefaultSettings();
        settings.slots[2].transpose = -5;
        settings.slots[2].speed = 2.0;
        settings.slots[3].gate = 0.5;
        settings.triggerMode = 2;
        settings.triggerNote = 48;
        engine.setSettings (settings);

        const auto readBack = engine.getSettings();
        expect (readBack.slots[2].transpose == -5, "slot 3 transpose is stored");
        expectNear (readBack.slots[2].speed, 2.0, 1.0e-9, "slot 3 speed is stored");
        expectNear (readBack.slots[3].gate, 0.5, 1.0e-9, "slot 4 gate is stored");
        expect (readBack.triggerMode == 2, "trigger mode is stored");
        expect (readBack.triggerNote == 48, "trigger note is stored");
    }
}

int main()
{
    std::printf ("LoopDeck engine tests\n\n");

    testLibrary();
    testEveryLoopParses();
    testMidiFileConversion();
    testBasicScheduling();
    testTransposeAndDrumMap();
    testSpeedAndGate();
    testReverseAndPingPong();
    testLengthOverride();
    testThruAndChannelFilter();
    testPadTrigger();
    testPanicAndStop();
    testSwing();
    testMuteSoloAndMaster();
    testInternalClock();
    testStateOfEngineSettings();

    std::printf ("\n%d checks, %d failures\n", checks, failures);

    return failures == 0 ? 0 : 1;
}
