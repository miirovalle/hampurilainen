#include "LoopLibrary.h"

#include <cstring>

namespace loopdeck
{

namespace
{
    constexpr char magic[4] = { 'L', 'D', 'L', 'B' };
    constexpr juce::uint16 expectedVersion = 1;
    constexpr size_t headerSize = 10;

    struct Reader
    {
        const juce::uint8* data = nullptr;
        size_t size = 0;
        size_t pos = 0;
        bool failed = false;

        void need (size_t bytes)
        {
            if (failed || pos + bytes > size)
                failed = true;
        }

        juce::uint8 readByte()
        {
            need (1);
            return failed ? 0 : data[pos++];
        }

        juce::uint16 readShort()
        {
            need (2);
            if (failed)
                return 0;

            const auto value = (juce::uint16) ((data[pos] << 8) | data[pos + 1]);
            pos += 2;
            return value;
        }

        juce::uint32 readInt()
        {
            need (4);
            if (failed)
                return 0;

            const juce::uint32 value = ((juce::uint32) data[pos] << 24)
                                     | ((juce::uint32) data[pos + 1] << 16)
                                     | ((juce::uint32) data[pos + 2] << 8)
                                     | (juce::uint32) data[pos + 3];
            pos += 4;
            return value;
        }

        // The packer writes little-endian values for the entry table.
        juce::uint16 readShortLE()
        {
            need (2);
            if (failed)
                return 0;

            const auto value = (juce::uint16) (data[pos] | (data[pos + 1] << 8));
            pos += 2;
            return value;
        }

        juce::uint32 readIntLE()
        {
            need (4);
            if (failed)
                return 0;

            const juce::uint32 value = (juce::uint32) data[pos]
                                     | ((juce::uint32) data[pos + 1] << 8)
                                     | ((juce::uint32) data[pos + 2] << 16)
                                     | ((juce::uint32) data[pos + 3] << 24);
            pos += 4;
            return value;
        }

        float readFloatLE()
        {
            const auto bits = readIntLE();
            float value = 0.0f;
            std::memcpy (&value, &bits, sizeof (value));
            return value;
        }

        juce::String readString (int numBytes)
        {
            need ((size_t) numBytes);
            if (failed)
                return {};

            juce::String result (juce::CharPointer_UTF8 (reinterpret_cast<const char*> (data + pos)), (size_t) numBytes);
            pos += (size_t) numBytes;
            return result;
        }
    };
}

int LoopInfo::getNumBars() const noexcept
{
    const auto beats = getLengthBeats();
    return juce::jmax (1, (int) std::lround (beats / 4.0));
}

juce::String LoopInfo::getChannelText() const
{
    juce::StringArray channels;

    for (int i = 0; i < 16; ++i)
        if ((channelsMask & (1 << i)) != 0)
            channels.add (juce::String (i + 1));

    return channels.isEmpty() ? juce::String() : channels.joinIntoString ("+");
}

bool LoopLibrary::loadFromMemory (const void* data, size_t sizeInBytes)
{
    loops.clear();
    blob = nullptr;
    blobSize = 0;
    error = {};

    if (data == nullptr || sizeInBytes < headerSize)
    {
        error = "Loop library is empty";
        return false;
    }

    Reader reader { static_cast<const juce::uint8*> (data), sizeInBytes, 0, false };

    if (std::memcmp (reader.data, magic, 4) != 0)
    {
        error = "Loop library header is corrupt";
        return false;
    }

    reader.pos = 4;

    if (reader.readShortLE() != expectedVersion)
    {
        error = "Unsupported loop library version";
        return false;
    }

    const auto count = reader.readIntLE();

    if (count == 0 || count > 200000)
    {
        error = "Loop library entry count is invalid";
        return false;
    }

    loops.reserve (count);

    for (juce::uint32 i = 0; i < count && ! reader.failed; ++i)
    {
        LoopInfo info;

        const auto nameLen = (int) reader.readByte();
        info.name = reader.readString (nameLen);

        const auto folderLen = (int) reader.readByte();
        info.folder = reader.readString (folderLen);

        reader.readShortLE(); // flags, reserved for future use
        info.numNotes = (int) reader.readShortLE();
        info.minNote = (int) reader.readByte();
        info.maxNote = (int) reader.readByte();
        info.ppq = (int) reader.readShortLE();
        info.channelsMask = (int) reader.readShortLE();
        info.lengthTicks = (int) reader.readIntLE();
        info.dataOffset = (int) reader.readIntLE();
        info.dataSize = (int) reader.readIntLE();
        info.bpm = (double) reader.readFloatLE();

        if (info.name.isEmpty() || info.dataSize <= 0)
        {
            error = "Loop library entry is incomplete";
            loops.clear();
            return false;
        }

        loops.push_back (std::move (info));
    }

    if (reader.failed)
    {
        error = "Loop library is truncated";
        loops.clear();
        return false;
    }

    blob = reader.data + reader.pos;
    blobSize = sizeInBytes - reader.pos;
    return true;
}

const LoopInfo* LoopLibrary::getInfo (int index) const noexcept
{
    return isValidIndex (index) ? &loops[(size_t) index] : nullptr;
}

juce::MidiFile LoopLibrary::readMidiFile (int index) const
{
    juce::MidiFile result;

    const auto* info = getInfo (index);

    if (info == nullptr || blob == nullptr)
        return result;

    const auto offset = (size_t) info->dataOffset;
    const auto length = (size_t) info->dataSize;

    if (offset + length > blobSize)
        return result;

    juce::MemoryInputStream stream (blob + offset, length, false);

    if (! result.readFrom (stream, true))
        return {};

    return result;
}

juce::StringArray LoopLibrary::getFolders() const
{
    juce::StringArray folders;

    for (const auto& info : loops)
        if (! info.folder.isEmpty())
            folders.addIfNotAlreadyThere (info.folder);

    return folders;
}

std::vector<int> LoopLibrary::search (const juce::String& text, const juce::String& folder) const
{
    std::vector<int> result;

    const auto trimmed = text.trim();
    const bool filterFolder = folder.isNotEmpty();

    for (int i = 0; i < (int) loops.size(); ++i)
    {
        const auto& info = loops[(size_t) i];

        if (filterFolder && info.folder != folder)
            continue;

        if (trimmed.isNotEmpty()
            && ! info.getFullName().containsIgnoreCase (trimmed))
            continue;

        result.push_back (i);
    }

    return result;
}

} // namespace loopdeck
