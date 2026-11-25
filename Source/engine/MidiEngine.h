#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>

#include "DrumMap.h"

namespace loopdeck
{

inline constexpr int numSlots = 4;
inline constexpr int numLengthChoices = 6;
inline constexpr int numSpeedChoices = 5;
inline constexpr int numDirectionChoices = 3;
inline constexpr int numClockChoices = 3;
inline constexpr int numTriggerModes = 3;

double getLengthChoiceBeats (int choice);      // 0 -> use the loop, 1..5 -> 1, 2, 4, 8, 16 bars
double getSpeedChoiceMultiplier (int choice);

//==============================================================================
struct PatternNote
{
    double startBeats = 0.0;
    double lengthBeats = 0.25;
    int channel = 1;
    int note = 36;
    int velocity = 100;
};

/** One loop, converted from a MIDI file into beat-based notes. */
struct LoopPattern
{
    std::vector<PatternNote> notes;
    double lengthBeats = 4.0;
    juce::String name;

    static std::shared_ptr<const LoopPattern> fromMidiFile (const juce::MidiFile& file,
                                                            double fallbackLengthBeats,
                                                            juce::String patternName);
};

//==============================================================================
struct SlotSettings
{
    bool enabled = false;
    bool mute = false;
    bool solo = false;

    int channel = 0;          // 0 -> keep the channel stored in the file
    int transpose = 0;        // semitones, applied after the drum map
    int lengthChoice = 0;     // index into the length choices
    int direction = 0;        // 0 forward, 1 reverse, 2 ping-pong
    int mapPreset = 0;        // DrumMapPreset
    int fixedVelocity = -1;   // -1 -> use the recorded velocity

    double speed = 1.0;
    double gate = 1.0;
    double velocityScale = 1.0;
    double swing = 0.0;
    double startTrim = 0.0;   // 0..1 rotation of the loop
};

struct EngineSettings
{
    std::array<SlotSettings, numSlots> slots {};

    bool master = true;
    bool thru = false;
    bool followMidiTransport = true;
    bool allNotesOffOnStop = true;

    int inputChannel = 0;     // 0 -> all
    int thruChannel = 0;      // 0 -> as incoming
    int clockSource = 0;      // 0 host, 1 internal, 2 MIDI clock
    int triggerNote = 36;
    int triggerMode = 0;      // 0 off, 1 toggle, 2 one-shot

    double internalBpm = 120.0;
};

struct TransportSnapshot
{
    bool hasPosition = false;
    bool isPlaying = false;
    double ppq = 0.0;
    double bpm = 120.0;
};

//==============================================================================
/** The MIDI generation and routing core.

    It has no dependency on JUCE parameters or the GUI, so unit tests can drive
    it directly with synthetic patterns and transport snapshots.
*/
class MidiEngine
{
public:
    MidiEngine();
    ~MidiEngine();

    void prepare (double newSampleRate);
    void reset();

    void setSettings (const EngineSettings& newSettings);
    EngineSettings getSettings() const;

    void setDrumMap (int slot, const DrumMap& map);
    DrumMap getDrumMap (int slot) const;

    /** Takes ownership of the pattern; pass nullptr to empty the slot. */
    void setPattern (int slot, std::shared_ptr<const LoopPattern> pattern);
    void clearPatterns();

    void process (juce::MidiBuffer& midi, int numSamples, const TransportSnapshot& transport);

    void setManualTransport (bool shouldPlay);
    bool isManualTransportRunning() const noexcept { return manualRunning.load(); }

    void requestPanic() noexcept { panicRequested.store (true); }

    //===== UI feedback =====
    float getSlotPhase (int slot) const noexcept;
    bool isSlotRunning (int slot) const noexcept;
    float getActivity() const noexcept { return activity.load(); }
    juce::String getStatusText() const;

    double getMeasuredClockBpm() const noexcept { return clockBpm.load(); }

private:
    struct WarpedNote
    {
        double on = 0.0;
        double length = 0.25;
        int channel = 1;
        int note = 36;
        int velocity = 100;
    };

    struct WarpedPattern
    {
        std::vector<WarpedNote> notes;
        double lengthBeats = 4.0;
    };

    struct Slot
    {
        std::shared_ptr<const LoopPattern> source;
        std::shared_ptr<const WarpedPattern> warped;

        bool triggered = false;
        double triggerAnchor = 0.0;
        bool oneShotActive = false;
        double oneShotStart = 0.0;
        double oneShotEnd = 0.0;

        bool wasAudible = false;
        std::atomic<float> phase { 0.0f };
        std::atomic<bool> running { false };
        std::atomic<bool> resetRequested { false };
    };

    struct PendingOff
    {
        double beatsRemaining = 0.0;
        juce::MidiMessage message;
        int slot = 0;
    };

    struct ScheduledEvent
    {
        int samplePosition = 0;
        int order = 2;
        juce::MidiMessage message;

        bool operator< (const ScheduledEvent& other) const noexcept
        {
            if (samplePosition != other.samplePosition)
                return samplePosition < other.samplePosition;

            return order < other.order;
        }
    };

    struct WarpKey
    {
        int channel = 0, transpose = 0, lengthChoice = 0, direction = 0, mapPreset = 0, fixedVelocity = -1;
        double speed = 1.0, gate = 1.0, velocityScale = 1.0, swing = 0.0, startTrim = 0.0;

        bool operator== (const WarpKey& other) const noexcept;

        static WarpKey from (const SlotSettings& slot);
    };

    std::shared_ptr<const WarpedPattern> buildWarpedPattern (const std::shared_ptr<const LoopPattern>& source,
                                                             const SlotSettings& slotSettings,
                                                             const DrumMap& map);

    void flushSlot (int slot, std::vector<ScheduledEvent>& events, int samplePosition);
    void flushAll (std::vector<ScheduledEvent>& events, int samplePosition, bool sendAllNotesOff);
    void addEvent (std::vector<ScheduledEvent>& events, int order, int samplePosition,
                   const juce::MidiMessage& message);
    void emitNoteOn (std::vector<ScheduledEvent>& events, std::vector<PendingOff>& offs,
                     int slot, int samplePosition, const WarpedNote& note);
    void handleTransportMessage (const juce::MidiMessage& message);
    void startSlot (int slot, bool oneShot, double startTime);

    mutable juce::CriticalSection stateLock;

    EngineSettings settings;
    std::array<DrumMap, numSlots> drumMaps;
    std::array<WarpKey, numSlots> lastWarpKeys;

    Slot slots[numSlots];
    std::vector<PendingOff> pendingOffs;

    double sampleRate = 44100.0;
    double freePpq = 0.0;
    double padPpq = 0.0;
    double lastHostEndPpq = 0.0;
    bool transportWasPlaying = false;
    bool lastFreeRunning = false;

    std::atomic<bool> manualRunning { false };
    std::atomic<bool> restartRequested { false };
    std::atomic<bool> panicRequested { false };
    std::atomic<float> activity { 0.0f };
    std::atomic<double> clockBpm { 0.0 };

    double lastClockTime = -1.0;
    double clockTimeNow = 0.0;
    int usedChannelMask = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiEngine)
};

} // namespace loopdeck
