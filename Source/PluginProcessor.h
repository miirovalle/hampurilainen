#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

#include "engine/LoopLibrary.h"
#include "engine/MidiEngine.h"
#include "model/Parameters.h"

namespace loopdeck
{

inline constexpr const char* productName = "LoopDeck";
inline constexpr const char* companyName = "Miiro Audio";
inline constexpr const char* pluginVersion = "1.0.0";

/** AUv3 MIDI processor: a four slot loop player that drives external synths
    and drum machines.
*/
class LoopDeckAudioProcessor final : public juce::AudioProcessor
{
public:
    LoopDeckAudioProcessor();
    ~LoopDeckAudioProcessor() override;

    //===== AudioProcessor =====
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return productName; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destination) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //===== library =====
    const LoopLibrary& getLibrary() const noexcept { return library; }

    //===== slots =====
    void loadLoopIntoSlot (int slot, int loopIndex);
    void clearSlot (int slot);
    int getLoopIndexForSlot (int slot) const noexcept;
    juce::String getLoopNameForSlot (int slot) const;

    int getSelectedSlot() const noexcept { return selectedSlot.load(); }
    void setSelectedSlot (int slot);

    //===== drum maps =====
    DrumMap getEffectiveDrumMap (int slot) const;
    void setCustomNoteMapping (int slot, int inputNote, int outputNote);
    void resetDrumMap (int slot);
    void copyDrumMap (int fromSlot, int toSlot);
    bool isCustomDrumMap (int slot) const;

    //===== transport / misc =====
    void panic() { engine.requestPanic(); }
    bool isHostTransportAvailable() const noexcept { return hostPlayheadSeen.load(); }
    void togglePlayStop();

    float getInputActivity() const noexcept { return inputActivity.load(); }
    float getOutputActivity() const noexcept { return outputActivity.load(); }

    MidiEngine& getEngine() noexcept { return engine; }
    const MidiEngine& getEngine() const noexcept { return engine; }

    /** Incremented whenever something outside the parameters changes, so the
        editor can refresh its labels without a change broadcast. */
    std::atomic<int> uiRevision { 0 };

    juce::AudioProcessorValueTreeState apvts;

private:
    void syncDrumMaps();
    EngineSettings buildSettings() const;
    float raw (const juce::String& id) const;
    int rawInt (const juce::String& id) const;

    LoopLibrary library;
    MidiEngine engine;

    std::array<std::atomic<int>, numSlots> slotLoopIndex;

    mutable juce::CriticalSection mapLock;
    std::array<DrumMap, numSlots> customMaps;
    std::array<int, numSlots> lastMapPreset { -1, -1, -1, -1 };

    std::atomic<int> selectedSlot { 0 };
    std::atomic<bool> hostPlayheadSeen { false };
    std::atomic<float> inputActivity { 0.0f };
    std::atomic<float> outputActivity { 0.0f };
    int lastBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopDeckAudioProcessor)
};

} // namespace loopdeck
