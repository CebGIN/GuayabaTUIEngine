#ifndef GUAYABA_TERMINAL_KEYSEQUENCE_HPP
#define GUAYABA_TERMINAL_KEYSEQUENCE_HPP

// Private backend helper. NOT part of the public API: the escape tables and the
// byte decoding live here and only here, so both platform backends parse the
// terminal byte stream the same way, and so the pure functions can be tested
// headlessly. See tests/key_parsing.cpp.

#include <cstddef>
#include <string>

#include <guayaba/Core/Key.hpp>

namespace guayaba {
namespace keyseq {

// A byte above ASCII has no named key, so it is reported as UNKNOWN with the
// character attached.
inline Key key_from_ascii(unsigned char byte) {
    if (byte >= 'a' && byte <= 'z')
        return static_cast<Key>(static_cast<uint16_t>(byte) - 'a' + 'A');
    if (byte >= 'A' && byte <= 'Z') return static_cast<Key>(byte);
    if (byte >= '0' && byte <= '9') return static_cast<Key>(byte);
    if (byte == ' ') return Key::SPACE;
    return Key::NONE;
}

/*
    Decodes one UTF-8 codepoint starting at index. Returns the number of bytes
    consumed, or 0 when the sequence is incomplete (the caller keeps the bytes
    pending). A malformed continuation is treated as a one-byte codepoint rather
    than desyncing the stream.

    The leading byte's length is decided most-significant first: 0xF0 before
    0xE0 before 0xC0. Getting this order wrong makes every 3- and 4-byte
    codepoint decode as 2 bytes.
*/
inline std::size_t decode_utf8(const std::string& buffer, std::size_t index,
                               char32_t& codepoint) {
    const unsigned char b0 = static_cast<unsigned char>(buffer[index]);

    std::size_t length;
    char32_t value;
    if (b0 < 0x80) {
        length = 1;
        value = b0;
    } else if ((b0 & 0xE0) == 0xC0) {
        length = 2;
        value = b0 & 0x1F;
    } else if ((b0 & 0xF0) == 0xE0) {
        length = 3;
        value = b0 & 0x0F;
    } else if ((b0 & 0xF8) == 0xF0) {
        length = 4;
        value = b0 & 0x07;
    } else {
        // A continuation byte with no lead, or an invalid lead. Emit it alone.
        codepoint = b0;
        return 1;
    }

    if (index + length > buffer.size()) return 0;  // incomplete, wait

    for (std::size_t i = 1; i < length; ++i) {
        const unsigned char continuation =
            static_cast<unsigned char>(buffer[index + i]);
        if ((continuation & 0xC0) != 0x80) {
            codepoint = b0;  // malformed: emit the lead alone
            return 1;
        }
        value = (value << 6) | (continuation & 0x3F);
    }

    codepoint = value;
    return length;
}

// ctrl / alt / shift are 1 + (bit0=shift, bit1=alt, bit2=ctrl) per the xterm
// spec, so the modifier parameter of a modified sequence is this minus one.
inline constexpr int MOD_SHIFT = 1;
inline constexpr int MOD_ALT = 2;
inline constexpr int MOD_CTRL = 4;

struct EscapeMapping {
    const char* sequence;
    std::size_t length;
    Key key;
};

// The '~' family: ESC [ <n> ~
inline constexpr EscapeMapping TILDE_KEYS[] = {
    {"\x1b[1~", 4, Key::HOME},     {"\x1b[2~", 4, Key::INSERT},
    {"\x1b[3~", 4, Key::DELETE},   {"\x1b[4~", 4, Key::END},
    {"\x1b[5~", 4, Key::PAGE_UP},  {"\x1b[6~", 4, Key::PAGE_DOWN},
    {"\x1b[7~", 4, Key::HOME},     {"\x1b[8~", 4, Key::END},
    {"\x1b[11~", 5, Key::F1},      {"\x1b[12~", 5, Key::F2},
    {"\x1b[13~", 5, Key::F3},      {"\x1b[14~", 5, Key::F4},
    {"\x1b[15~", 5, Key::F5},      {"\x1b[17~", 5, Key::F6},
    {"\x1b[18~", 5, Key::F7},      {"\x1b[19~", 5, Key::F8},
    {"\x1b[20~", 5, Key::F9},      {"\x1b[21~", 5, Key::F10},
    {"\x1b[23~", 5, Key::F11},     {"\x1b[24~", 5, Key::F12},
};

// SS3, sent when the terminal is in application cursor mode.
inline constexpr EscapeMapping SS3_KEYS[] = {
    {"\x1bOA", 3, Key::UP},    {"\x1bOB", 3, Key::DOWN},
    {"\x1bOC", 3, Key::RIGHT}, {"\x1bOD", 3, Key::LEFT},
    {"\x1bOH", 3, Key::HOME},  {"\x1bOF", 3, Key::END},
    {"\x1bOP", 3, Key::F1},    {"\x1bOQ", 3, Key::F2},
    {"\x1bOR", 3, Key::F3},    {"\x1bOS", 3, Key::F4},
};

// Plain CSI, final byte identifies the key.
inline constexpr EscapeMapping CSI_KEYS[] = {
    {"\x1b[A", 3, Key::UP},   {"\x1b[B", 3, Key::DOWN},
    {"\x1b[C", 3, Key::RIGHT}, {"\x1b[D", 3, Key::LEFT},
    {"\x1b[H", 3, Key::HOME}, {"\x1b[F", 3, Key::END},
};

// A CSI or SS3 sequence with modifiers: ESC [ 1;mod <final> or ESC O 1;mod <final>.
inline std::size_t match_modified_escape(const std::string& buffer, Key& key,
                                         bool& shift, bool& alt, bool& ctrl) {
    if (buffer.size() < 4 || buffer[0] != '\x1b') return 0;

    std::size_t index = 1;
    if (buffer[index] == '[') {
        ++index;
    } else if (buffer[index] == 'O') {
        ++index;
    } else {
        return 0;
    }

    // The leading parameter is always 1 for a cursor key.
    if (index >= buffer.size() || buffer[index] != '1') return 0;
    ++index;
    if (index >= buffer.size() || buffer[index] != ';') return 0;
    ++index;

    int modifier = 0;
    std::size_t digits = 0;
    while (index + digits < buffer.size() && buffer[index + digits] >= '0' &&
           buffer[index + digits] <= '9') {
        modifier = modifier * 10 + (buffer[index + digits] - '0');
        ++digits;
    }
    if (digits == 0) return 0;
    index += digits;

    if (index >= buffer.size()) return 0;
    Key mapped = Key::UNKNOWN;
    switch (buffer[index]) {
        case 'A': mapped = Key::UP; break;
        case 'B': mapped = Key::DOWN; break;
        case 'C': mapped = Key::RIGHT; break;
        case 'D': mapped = Key::LEFT; break;
        case 'H': mapped = Key::HOME; break;
        case 'F': mapped = Key::END; break;
        default: return 0;
    }

    const int bits = modifier > 0 ? modifier - 1 : 0;
    shift = (bits & MOD_SHIFT) != 0;
    alt = (bits & MOD_ALT) != 0;
    ctrl = (bits & MOD_CTRL) != 0;
    key = mapped;
    return index + 1;
}

// A modified '~' sequence: ESC [ <n> ; <mod> ~
inline std::size_t match_modified_tilde(const std::string& buffer, Key& key,
                                        bool& shift, bool& alt, bool& ctrl) {
    if (buffer.size() < 6 || buffer[0] != '\x1b' || buffer[1] != '[') return 0;

    std::size_t index = 2;
    int number = 0;
    std::size_t digits = 0;
    while (index + digits < buffer.size() && buffer[index + digits] >= '0' &&
           buffer[index + digits] <= '9') {
        number = number * 10 + (buffer[index + digits] - '0');
        ++digits;
    }
    if (digits == 0) return 0;
    index += digits;
    if (index >= buffer.size() || buffer[index] != ';') return 0;
    ++index;

    int modifier = 0;
    digits = 0;
    while (index + digits < buffer.size() && buffer[index + digits] >= '0' &&
           buffer[index + digits] <= '9') {
        modifier = modifier * 10 + (buffer[index + digits] - '0');
        ++digits;
    }
    if (digits == 0) return 0;
    index += digits;

    if (index >= buffer.size() || buffer[index] != '~') return 0;

    Key mapped = Key::UNKNOWN;
    switch (number) {
        case 1: case 7: mapped = Key::HOME; break;
        case 2: mapped = Key::INSERT; break;
        case 3: mapped = Key::DELETE; break;
        case 4: case 8: mapped = Key::END; break;
        case 5: mapped = Key::PAGE_UP; break;
        case 6: mapped = Key::PAGE_DOWN; break;
        case 15: mapped = Key::F5; break;
        case 17: mapped = Key::F6; break;
        case 18: mapped = Key::F7; break;
        case 19: mapped = Key::F8; break;
        case 20: mapped = Key::F9; break;
        case 21: mapped = Key::F10; break;
        case 23: mapped = Key::F11; break;
        case 24: mapped = Key::F12; break;
        default: return 0;
    }

    const int bits = modifier > 0 ? modifier - 1 : 0;
    shift = (bits & MOD_SHIFT) != 0;
    alt = (bits & MOD_ALT) != 0;
    ctrl = (bits & MOD_CTRL) != 0;
    key = mapped;
    return index + 1;
}

// Matches any known escape sequence at the front of buffer. Returns the number of
// bytes it spans, or 0 when what is present is not (yet) a recognised sequence.
inline std::size_t match_escape(const std::string& buffer, Key& key, bool& shift,
                                bool& alt, bool& ctrl) {
    if (buffer.empty() || buffer[0] != '\x1b') return 0;

    shift = alt = ctrl = false;

    // Modified forms first: they are longer, and the bare CSI forms would match a
    // partial modified sequence if tested in the other order.
    const std::size_t modified_tilde =
        match_modified_tilde(buffer, key, shift, alt, ctrl);
    if (modified_tilde > 0) return modified_tilde;

    const std::size_t modified = match_modified_escape(buffer, key, shift, alt, ctrl);
    if (modified > 0) return modified;

    for (const EscapeMapping& mapping : TILDE_KEYS) {
        if (buffer.size() >= mapping.length &&
            buffer.compare(0, mapping.length, mapping.sequence, mapping.length) == 0) {
            key = mapping.key;
            return mapping.length;
        }
    }
    for (const EscapeMapping& mapping : SS3_KEYS) {
        if (buffer.size() >= mapping.length &&
            buffer.compare(0, mapping.length, mapping.sequence, mapping.length) == 0) {
            key = mapping.key;
            return mapping.length;
        }
    }
    for (const EscapeMapping& mapping : CSI_KEYS) {
        if (buffer.size() >= mapping.length &&
            buffer.compare(0, mapping.length, mapping.sequence, mapping.length) == 0) {
            key = mapping.key;
            return mapping.length;
        }
    }
    return 0;
}

/*
    The length of a CSI/SS3 sequence whose key we do not recognise, so its bytes
    can be dropped as a unit instead of being re-parsed as printable keys.

    Returns 0 when it does not (yet) look like a complete sequence to discard.
    A CSI final byte is in 0x40..0x7E; parameters/intermediates are 0x20..0x3F.
*/
inline std::size_t unknown_sequence_length(const std::string& buffer) {
    if (buffer.size() < 3 || buffer[0] != '\x1b') return 0;
    if (buffer[1] != '[' && buffer[1] != 'O') return 0;

    for (std::size_t i = 2; i < buffer.size(); ++i) {
        const unsigned char byte = static_cast<unsigned char>(buffer[i]);
        if (byte >= 0x40 && byte <= 0x7E) return i + 1;  // final byte: done
        if (byte < 0x20 || byte > 0x3F) return 0;        // not a valid parameter
    }
    return 0;  // still incomplete
}

} // namespace keyseq
} // namespace guayaba

#endif // GUAYABA_TERMINAL_KEYSEQUENCE_HPP
