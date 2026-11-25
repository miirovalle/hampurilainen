#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include <vector>

namespace loopdeck
{

/** Metadata for one MIDI loop embedded in the binary library. */
struct LoopInfo
{
    juce::String name;
    juce::String folder;
    int numNotes = 0;
    int minNote = 0;
    int maxNote = 0;
    int ppq = 480;
    int channelsMask = 0;
    int lengthTicks = 0;
    int dataOffset = 0;
    int dataSize = 0;
    double bpm = 120.0;

    double getLengthBeats() const noexcept { return ppq > 0 ? (double) lengthTicks / (double) ppq : 4.0; }
    int getNumBars() const noexcept;
    juce::String getFullName() const { return folder.isEmpty() ? name : folder + " / " + name; }

    /** "ch 10" style string, or an empty string when the loop has no notes. */
    juce::String getChannelText() const;
};

/** Reads the packed blob produced by tools/build_loop_library.py.

    The blob lives in read-only memory (BinaryData), so nothing is copied
    until a loop is actually loaded into a slot.
*/
class LoopLibrary
{
public:
    bool loadFromMemory (const void* data, size_t sizeInBytes);

    int getNumLoops() const noexcept { return (int) loops.size(); }
    const LoopInfo* getInfo (int index) const noexcept;
    bool isValidIndex (int index) const noexcept { return index >= 0 && index < (int) loops.size(); }

    /** Parses the raw bytes of a loop. Returns an invalid MidiFile on failure. */
    juce::MidiFile readMidiFile (int index) const;

    juce::StringArray getFolders() const;

    /** Case-insensitive search over name and folder. An empty folder means "everything". */
    std::vector<int> search (const juce::String& text, const juce::String& folder) const;

    const juce::String& getError() const noexcept { return error; }

private:
    std::vector<LoopInfo> loops;
    const juce::uint8* blob = nullptr;
    size_t blobSize = 0;
    juce::String error;

    JUCE_LEAK_DETECTOR (LoopLibrary)
};

} // namespace loopdeck
