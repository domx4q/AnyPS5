#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using Version = std::array<std::uint8_t, 4>;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU version: %s\n", message);
    std::abort();
}

bool ExpectedInvariant(std::uint32_t unit) {
    if (unit > 0x7f) return false;
    for (const std::uint32_t variant : {0x0a, 0x21, 0x23, 0x24, 0x40, 0x5b, 0x5c, 0x5d, 0x5e, 0x60, 0x7b, 0x7c, 0x7d, 0x7e})
        if (unit == variant) return false;
    return true;
}

template<class F>
bool Rejects(F call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

Version FromString(const char* text) {
    Version version{9, 9, 9, 9};
    u_versionFromString_nid_postfix(version.data(), text);
    return version;
}

Version FromUString(const char16_t* text) {
    Version version{9, 9, 9, 9};
    u_versionFromUString_nid_postfix(version.data(), text);
    return version;
}

std::string ToString(Version version) {
    std::array<char, 21> text;
    text.fill('X');
    u_versionToString_nid_postfix(version.data(), text.data());
    const auto length = std::strlen(text.data());
    Require(length <= 15 && text[length + 1] == 'X', "version string overrun");
    return text.data();
}

void CharsToUChars() {
    std::array<char, 256> chars;
    for (std::size_t index = 0; index < chars.size(); ++index) chars[index] = static_cast<char>(index);
    std::array<char16_t, 257> units;
    units.fill(0x5555);
    u_charsToUChars_nid_postfix(chars.data(), units.data(), 256);
    for (std::size_t index = 0; index < chars.size(); ++index) Require(units[index] == index, "bytes widen without sign extension");
    Require(units[256] == 0x5555, "widening stays within the length");
    units.fill(0x5555);
    u_charsToUChars_nid_postfix(chars.data(), units.data(), 0);
    u_charsToUChars_nid_postfix(chars.data(), units.data(), -1);
    Require(units[0] == 0x5555, "nonpositive lengths write nothing");
    u_charsToUChars_nid_postfix(nullptr, nullptr, 0);
    u_charsToUChars_nid_postfix(nullptr, nullptr, -5);
    Require(Rejects([&] { u_charsToUChars_nid_postfix(nullptr, units.data(), 1); }), "null source rejected");
    Require(Rejects([&] { u_charsToUChars_nid_postfix(chars.data(), nullptr, 1); }), "null destination rejected");
}

void UCharsToChars() {
    std::array<char16_t, 0x10000> units;
    for (std::size_t index = 0; index < units.size(); ++index) units[index] = static_cast<char16_t>(index);
    std::array<char, 0x10001> chars;
    chars.fill('X');
    u_UCharsToChars_nid_postfix(units.data(), chars.data(), 0x10000);
    for (std::uint32_t unit = 0; unit < 0x10000; ++unit)
        Require(chars[unit] == (ExpectedInvariant(unit) ? static_cast<char>(unit) : 0), "only invariant characters narrow");
    Require(chars[0x10000] == 'X', "narrowing stays within the length");
    chars.fill('X');
    u_UCharsToChars_nid_postfix(units.data(), chars.data(), 0);
    u_UCharsToChars_nid_postfix(units.data(), chars.data(), -1);
    Require(chars[0] == 'X', "nonpositive lengths write nothing");
    u_UCharsToChars_nid_postfix(nullptr, nullptr, 0);
    Require(Rejects([&] { u_UCharsToChars_nid_postfix(nullptr, chars.data(), 1); }), "null source rejected");
    Require(Rejects([&] { u_UCharsToChars_nid_postfix(units.data(), nullptr, 1); }), "null destination rejected");
}

void AuStrcpy() {
    std::array<char, 8> chars;
    chars.fill('X');
    Require(u_austrcpy_nid_postfix(chars.data(), u"") == chars.data(), "empty copy returns destination");
    Require(chars[0] == 0 && chars[1] == 'X', "empty copy writes only terminator");

    chars.fill('X');
    Require(u_austrcpy_nid_postfix(chars.data(), u"Ab_9") == chars.data(), "ASCII copy returns destination");
    Require(std::memcmp(chars.data(), "Ab_9\0XXX", chars.size()) == 0, "ASCII characters and terminator copy");

    chars.fill('X');
    u_austrcpy_nid_postfix(chars.data(), u"A!#$@B");
    Require(std::memcmp(chars.data(), "A!#$@B\0X", chars.size()) == 0, "punctuation outside the invariant set copies unchanged");

    std::array<char16_t, 128> asciiUnits;
    for (std::size_t index = 0; index < 127; ++index) asciiUnits[index] = static_cast<char16_t>(index + 1);
    asciiUnits[127] = 0;
    std::array<char, 129> asciiChars;
    asciiChars.fill('X');
    Require(u_austrcpy_nid_postfix(asciiChars.data(), asciiUnits.data()) == asciiChars.data(), "full ASCII copy returns destination");
    for (std::size_t index = 0; index < 127; ++index)
        Require(asciiChars[index] == static_cast<char>(index + 1), "every unit in 0x01-0x7f copies unchanged");
    Require(asciiChars[127] == 0 && asciiChars[128] == 'X', "full ASCII copy writes terminator without overrun");

    Require(Rejects([&] { u_austrcpy_nid_postfix(nullptr, u"a"); }), "null destination rejected");
    Require(Rejects([&] { u_austrcpy_nid_postfix(chars.data(), nullptr); }), "null source rejected");
}

void AuStrncpy() {
    std::array<char, 8> chars;
    chars.fill('X');
    Require(u_austrncpy_nid_postfix(chars.data(), u"Abcdef", 3) == chars.data(), "truncated copy returns destination");
    Require(std::memcmp(chars.data(), "AbcXXXXX", chars.size()) == 0, "truncated copy omits terminator");

    chars.fill('X');
    u_austrncpy_nid_postfix(chars.data(), u"Abc", 3);
    Require(std::memcmp(chars.data(), "AbcXXXXX", chars.size()) == 0, "exact-length copy omits terminator");

    chars.fill('X');
    u_austrncpy_nid_postfix(chars.data(), u"Abc", 6);
    Require(std::memcmp(chars.data(), "Abc\0XXXX", chars.size()) == 0, "longer bound writes one terminator without padding");

    chars.fill('X');
    u_austrncpy_nid_postfix(chars.data(), u"A!#$@", 7);
    Require(std::memcmp(chars.data(), "A!#$@\0XX", chars.size()) == 0, "non-invariant ASCII units copy unchanged within bound");

    std::array<char16_t, 128> asciiUnits;
    for (std::size_t index = 0; index < 127; ++index) asciiUnits[index] = static_cast<char16_t>(index + 1);
    asciiUnits[127] = 0;
    std::array<char, 129> asciiChars;
    asciiChars.fill('X');
    Require(u_austrncpy_nid_postfix(asciiChars.data(), asciiUnits.data(), 128) == asciiChars.data(), "bounded full ASCII copy returns destination");
    for (std::size_t index = 0; index < 127; ++index)
        Require(asciiChars[index] == static_cast<char>(index + 1), "every unit in 0x01-0x7f copies unchanged in bounded copy");
    Require(asciiChars[127] == 0 && asciiChars[128] == 'X', "bounded full ASCII copy writes terminator without overrun");

    const std::array<char16_t, 3> unterminated{u'!', u'$', u'@'};
    chars.fill('X');
    u_austrncpy_nid_postfix(chars.data(), unterminated.data(), 3);
    Require(std::memcmp(chars.data(), "!$@XXXXX", chars.size()) == 0, "bounded copy stops at count without reading past it");

    chars.fill('X');
    Require(u_austrncpy_nid_postfix(chars.data(), u"Abc", 0) == chars.data(), "zero count returns destination");
    Require(u_austrncpy_nid_postfix(chars.data(), u"Abc", -2) == chars.data(), "negative count returns destination");
    Require(chars[0] == 'X', "nonpositive counts write nothing");
    Require(u_austrncpy_nid_postfix(nullptr, nullptr, 0) == nullptr, "zero count allows null pointers");
    Require(u_austrncpy_nid_postfix(chars.data(), nullptr, -1) == chars.data(), "negative count allows null source");
    Require(Rejects([&] { u_austrncpy_nid_postfix(nullptr, u"a", 1); }), "null destination with positive count rejected");
    Require(Rejects([&] { u_austrncpy_nid_postfix(chars.data(), nullptr, 1); }), "null source with positive count rejected");
}

void VersionFromString() {
    Require(FromString("1.2.3.4") == Version{1, 2, 3, 4}, "four parts");
    Require(FromString("65.1") == Version{65, 1, 0, 0}, "missing parts are zero");
    Require(FromString("1.2.3.4.5") == Version{1, 2, 3, 4}, "parts after the fourth are ignored");
    Require(FromString("") == Version{0, 0, 0, 0}, "empty string");
    Require(FromString("x") == Version{0, 0, 0, 0}, "no digits");
    Require(FromString("3x.4") == Version{3, 0, 0, 0}, "parsing stops at a non-delimiter");
    Require(FromString("1..2") == Version{1, 0, 0, 0}, "empty part stops parsing");
    Require(FromString("1.2.") == Version{1, 2, 0, 0}, "trailing delimiter");
    Require(FromString(" 7. 8") == Version{7, 8, 0, 0}, "leading whitespace is skipped by strtoul");
    Require(FromString("+5.-1") == Version{5, 255, 0, 0}, "signs follow strtoul");
    Require(FromString("256.300.511") == Version{0, 44, 255, 0}, "parts truncate to eight bits");
    Require(FromString("99999999999999999999999.1") == Version{255, 1, 0, 0}, "overflow saturates before truncation");
    Require(FromString(nullptr) == Version{0, 0, 0, 0}, "null string clears the version");
    u_versionFromString_nid_postfix(nullptr, "1.2");
}

void VersionFromUString() {
    Require(FromUString(u"10.2.1") == Version{10, 2, 1, 0}, "UTF-16 version");
    Require(FromUString(u"") == Version{0, 0, 0, 0}, "empty UTF-16 string");
    Require(FromUString(u"1$2") == Version{1, 0, 0, 0}, "variant character becomes NUL");
    Require(FromUString(u"4.é5") == Version{4, 0, 0, 0}, "non-ASCII character becomes NUL");
    Require(FromUString(u"１.2") == Version{0, 0, 0, 0}, "fullwidth digit is not a digit");
    Require(FromUString((std::u16string(17, u'0') + u"1.2").c_str()) == Version{1, 2, 0, 0}, "twenty units are read");
    Require(FromUString((std::u16string(18, u'0') + u"1.2").c_str()) == Version{1, 0, 0, 0}, "units after the twentieth are ignored");
    Require(FromUString(nullptr) == Version{9, 9, 9, 9}, "null string leaves the version unchanged");
    u_versionFromUString_nid_postfix(nullptr, u"1.2");
}

void VersionToString() {
    Require(ToString({1, 2, 3, 4}) == "1.2.3.4", "four parts");
    Require(ToString({65, 1, 0, 0}) == "65.1", "trailing zero parts are omitted");
    Require(ToString({7, 0, 0, 0}) == "7.0", "at least two parts");
    Require(ToString({0, 0, 0, 0}) == "0.0", "zero version");
    Require(ToString({1, 0, 0, 5}) == "1.0.0.5", "inner zero parts are kept");
    Require(ToString({0, 0, 1, 0}) == "0.0.1", "leading zero parts are kept");
    Require(ToString({255, 199, 99, 10}) == "255.199.99.10", "multi-digit parts");
    Require(ToString({100, 105, 109, 110}) == "10.15.19.110", "a zero tens digit after the hundreds digit is dropped");
    std::array<char, 4> text{'X', 'X', 'X', 'X'};
    u_versionToString_nid_postfix(nullptr, text.data());
    Require(text[0] == 0 && text[1] == 'X', "null version writes an empty string");
    const Version version{1, 2, 0, 0};
    u_versionToString_nid_postfix(version.data(), nullptr);
}

void GetVersion() {
    Version version{9, 9, 9, 9};
    u_getVersion_nid_postfix(version.data());
    Require(version == Version{78, 1, 0, 0}, "library version is 78.1.0.0");
    Require(ToString(version) == "78.1", "library version string");
    u_getVersion_nid_postfix(nullptr);
}

void RoundTrip() {
    const auto written = [](std::uint32_t part) { return part < 100 || part % 100 >= 10; };
    for (std::uint32_t part = 0; part < 256; ++part) {
        if (!written(part) || !written(255 - part)) continue;
        const Version version{static_cast<std::uint8_t>(part), 3, static_cast<std::uint8_t>(255 - part), 1};
        Require(FromString(ToString(version).c_str()) == version, "string round trip");
    }
}

template<class F>
void RequiresRuntimeErrorKeepingErrno(F call, const char* message) {
    errno = 71;
    bool threw = false;
    try {
        call();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw && errno == 71, message);
}

void GuestPointers() {
#ifdef _WIN32
    const std::size_t pageSize = 4096;
    auto* pages = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 4, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Require(pages != nullptr, "allocate guest pointer pages");
#else
    const std::size_t pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE) > 0 ? sysconf(_SC_PAGESIZE) : 4096);
    void* mapping = mmap(nullptr, pageSize * 4, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapping != MAP_FAILED, "allocate guest pointer pages");
    auto* pages = static_cast<char*>(mapping);
#endif
    std::memset(pages, 0x5a, pageSize);
    std::memcpy(pages, "\x01\x02\x03\x04", 4);
    std::memcpy(pages + pageSize - 8, "1.2.3", 6);
    std::memcpy(pages + pageSize - 2, "4.", 2);
    auto* unitPage = pages + pageSize * 2;
    auto* units = reinterpret_cast<char16_t*>(unitPage + pageSize) - 32;
    for (std::size_t index = 0; index < 32; ++index) units[index] = index == 1 ? u'.' : u'7';
    auto* terminatedUnits = reinterpret_cast<char16_t*>(unitPage);
    std::memcpy(terminatedUnits, u"5.6", sizeof(u"5.6"));
    for (const std::size_t page : {std::size_t{0}, std::size_t{2}}) {
#ifdef _WIN32
        DWORD oldProtect = 0;
        Require(VirtualProtect(pages + pageSize * page, pageSize, PAGE_READONLY, &oldProtect) != 0, "protect data page");
        Require(VirtualProtect(pages + pageSize * (page + 1), pageSize, PAGE_NOACCESS, &oldProtect) != 0, "protect guard page");
#else
        Require(mprotect(pages + pageSize * page, pageSize, PROT_READ) == 0, "protect data page");
        Require(mprotect(pages + pageSize * (page + 1), pageSize, PROT_NONE) == 0, "protect guard page");
#endif
    }
    auto* readOnly = reinterpret_cast<std::uint8_t*>(pages);
    auto* unmappedVersion = reinterpret_cast<std::uint8_t*>(std::uintptr_t{0x10});
    const auto* unmappedText = reinterpret_cast<const char*>(std::uintptr_t{0x10});
    const auto* unmappedUnits = reinterpret_cast<const char16_t*>(std::uintptr_t{0x10});
    auto* unwritableText = pages + 64;

    Require(FromString(pages + pageSize - 8) == Version{1, 2, 3, 0}, "terminated string ending at the guard page parses");
    Require(FromUString(terminatedUnits) == Version{5, 6, 0, 0}, "terminated units parse");
    Require(FromUString(units + 12) == FromString("77777777777777777777"), "twenty unterminated units before the guard page parse");
    Version unicode{9, 9, 9, 9};
    u_getUnicodeVersion_nid_postfix(unicode.data());
    Require(unicode == Version{17, 0, 0, 0}, "Unicode version is 17.0.0.0");

    Version version{9, 9, 9, 9};
    RequiresRuntimeErrorKeepingErrno([&] { u_versionFromString_nid_postfix(version.data(), unmappedText); },
                                     "unreadable version string throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { u_versionFromString_nid_postfix(version.data(), pages + pageSize - 2); },
                                     "version string running into the guard page throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { u_versionFromUString_nid_postfix(version.data(), unmappedUnits); },
                                     "unreadable version units throw and keep errno");
    RequiresRuntimeErrorKeepingErrno([&] { u_versionFromUString_nid_postfix(version.data(), reinterpret_cast<const char16_t*>(unitPage + pageSize) - 3); },
                                     "version units running into the guard page throw and keep errno");
    Require(version == Version{9, 9, 9, 9}, "unreadable input leaves the version unchanged");
    RequiresRuntimeErrorKeepingErrno([&] { u_versionFromString_nid_postfix(readOnly, "5.6"); },
                                     "unwritable version from a string throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { u_versionFromString_nid_postfix(unmappedVersion, nullptr); },
                                     "unwritable version from a null string throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { u_versionFromUString_nid_postfix(readOnly, u"5.6"); },
                                     "unwritable version from units throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { u_getUnicodeVersion_nid_postfix(readOnly); },
                                     "unwritable Unicode version throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { u_getVersion_nid_postfix(unmappedVersion); },
                                     "unwritable library version throws and keeps errno");
    Require(std::memcmp(readOnly, "\x01\x02\x03\x04", 4) == 0, "read-only version stays unchanged");

    std::array<char, 21> text;
    text.fill('X');
    RequiresRuntimeErrorKeepingErrno([&] { u_versionToString_nid_postfix(unmappedVersion, text.data()); },
                                     "unreadable version throws and keeps errno");
    Require(text[0] == 'X', "unreadable version leaves the string unchanged");
    RequiresRuntimeErrorKeepingErrno([&] { u_versionToString_nid_postfix(readOnly, unwritableText); },
                                     "unwritable version string throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { u_versionToString_nid_postfix(nullptr, unwritableText); },
                                     "unwritable empty version string throws and keeps errno");
    u_versionToString_nid_postfix(readOnly, text.data());
    Require(std::string(text.data()) == "1.2.3.4", "read-only version converts");
#ifdef _WIN32
    Require(VirtualFree(pages, 0, MEM_RELEASE) != 0, "release guest pointer pages");
#else
    Require(munmap(pages, pageSize * 4) == 0, "release guest pointer pages");
#endif
}

}

int main() {
    CharsToUChars();
    UCharsToChars();
    AuStrcpy();
    AuStrncpy();
    VersionFromString();
    VersionFromUString();
    VersionToString();
    GetVersion();
    RoundTrip();
    GuestPointers();
    return 0;
}
