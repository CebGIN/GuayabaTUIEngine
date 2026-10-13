// Headless checks for the terminal byte-stream parser. No terminal: the pure
// functions in src/Terminal/KeySequence.hpp are fed byte strings directly.

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

#include "../src/Terminal/KeySequence.hpp"

using guayaba::Key;
namespace keyseq = guayaba::keyseq;

namespace {

void test_ascii_mapping() {
    assert(keyseq::key_from_ascii('a') == Key::A);
    assert(keyseq::key_from_ascii('z') == Key::Z);
    assert(keyseq::key_from_ascii('Q') == Key::Q);
    assert(keyseq::key_from_ascii('7') == Key::DIGIT_7);
    assert(keyseq::key_from_ascii(' ') == Key::SPACE);
    assert(keyseq::key_from_ascii('!') == Key::NONE);

    // ASCII-valued: the key compares against the character literal.
    assert(static_cast<uint16_t>(Key::A) == 'A');
    assert(static_cast<uint16_t>(Key::DIGIT_3) == '3');
}

void test_utf8_decode() {
    char32_t cp = 0;

    // 2-byte: U+00E9 e-acute.
    std::string two = "\xC3\xA9";
    assert(keyseq::decode_utf8(two, 0, cp) == 2);
    assert(cp == 0xE9);

    // 3-byte: U+20AC euro. This is the case the ascending length check broke.
    std::string three = "\xE2\x82\xAC";
    assert(keyseq::decode_utf8(three, 0, cp) == 3);
    assert(cp == 0x20AC);

    // 4-byte: U+1F600.
    std::string four = "\xF0\x9F\x98\x80";
    assert(keyseq::decode_utf8(four, 0, cp) == 4);
    assert(cp == 0x1F600);

    // Incomplete: a 3-byte lead with one continuation present.
    std::string partial = "\xE2\x82";
    assert(keyseq::decode_utf8(partial, 0, cp) == 0);

    // A lone continuation byte is emitted alone, not treated as a desync.
    std::string stray = "\x80";
    assert(keyseq::decode_utf8(stray, 0, cp) == 1);
    assert(cp == 0x80);
}

void test_simple_escapes() {
    Key key = Key::NONE;
    bool shift = false, alt = false, ctrl = false;

    assert(keyseq::match_escape("\x1b[A", key, shift, alt, ctrl) == 3);
    assert(key == Key::UP && !shift && !alt && !ctrl);

    assert(keyseq::match_escape("\x1bOD", key, shift, alt, ctrl) == 3);
    assert(key == Key::LEFT);

    assert(keyseq::match_escape("\x1b[3~", key, shift, alt, ctrl) == 4);
    assert(key == Key::DELETE);

    assert(keyseq::match_escape("\x1b[15~", key, shift, alt, ctrl) == 5);
    assert(key == Key::F5);
}

void test_modified_escapes() {
    Key key = Key::NONE;
    bool shift = false, alt = false, ctrl = false;

    // ctrl+up is modifier 5 (1 + ctrl bit 4).
    assert(keyseq::match_escape("\x1b[1;5A", key, shift, alt, ctrl) == 6);
    assert(key == Key::UP && ctrl && !shift && !alt);

    // ctrl+shift+delete is modifier 6 (1 + shift 1 + ctrl 4).
    assert(keyseq::match_escape("\x1b[3;6~", key, shift, alt, ctrl) == 6);
    assert(key == Key::DELETE && shift && ctrl && !alt);
}

void test_unknown_sequence() {
    // A bracketed-paste start ESC[200~ is a complete CSI we do not map: it must be
    // consumed whole, not re-parsed as ESC, [, 2, 0, 0, ~.
    assert(keyseq::unknown_sequence_length("\x1b[200~") == 6);

    // Incomplete CSI: nothing to consume yet.
    assert(keyseq::unknown_sequence_length("\x1b[20") == 0);

    // Not a sequence at all.
    assert(keyseq::unknown_sequence_length("a") == 0);
}

} // namespace

int main() {
    test_ascii_mapping();
    test_utf8_decode();
    test_simple_escapes();
    test_modified_escapes();
    test_unknown_sequence();

    std::cout << "key_parsing: all checks passed." << std::endl;
    return 0;
}
