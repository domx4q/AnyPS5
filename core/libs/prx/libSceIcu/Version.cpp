#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/Api.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <string>

namespace {

constexpr std::uint16_t VersionLength = 4;
constexpr std::int32_t VersionStringLength = 20;
constexpr char VersionDelimiter = '.';
constexpr std::uint8_t UnicodeVersion[VersionLength] = {17, 0, 0, 0};
constexpr const char* IcuVersion = "78.1";
constexpr int StringTooLong = 63;

bool CopyInPreservingErrno(void* destination, const void* source, std::size_t bytes) {
    const int saved = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = saved;
    return copied;
}

bool CopyOutPreservingErrno(void* destination, const void* source, std::size_t bytes) {
    const int saved = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = saved;
    return copied;
}

std::string CopyInText(const char* text, const char* function) {
    std::string host;
    for (std::size_t maximum = VersionStringLength + 1;; maximum *= 2) {
        const int saved = errno;
        const int result = CopyGuestString_nid_no_patch(text, maximum, host);
        errno = saved;
        if (result == 0) return host;
        if (result != StringTooLong) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    }
}

void ParseVersion(std::uint8_t* version, const char* text) {
    std::uint16_t part = 0;
    if (text) {
        for (;;) {
            char* end = nullptr;
            version[part] = static_cast<std::uint8_t>(strtoul_nid_postfix(text, &end, 10));
            if (end == text || ++part == VersionLength || *end != VersionDelimiter) break;
            text = end + 1;
        }
    }
    while (part < VersionLength) version[part++] = 0;
}

void StoreVersion(std::uint8_t* version, const std::uint8_t* host, const char* function) {
    if (!CopyOutPreservingErrno(version, host, VersionLength)) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

char* WriteField(char* text, std::uint8_t field) {
    if (field >= 100) {
        *text++ = static_cast<char>('0' + field / 100);
        field %= 100;
    }
    if (field >= 10) {
        *text++ = static_cast<char>('0' + field / 10);
        field %= 10;
    }
    *text++ = static_cast<char>('0' + field);
    return text;
}

}

extern "C" {

void APS5_VABI u_versionFromString_nid_postfix(std::uint8_t* version, const char* text) {
    if (!version) return;
    std::uint8_t parsed[VersionLength];
    if (text) {
        const std::string host = CopyInText(text, __func__);
        ParseVersion(parsed, host.c_str());
    } else {
        ParseVersion(parsed, nullptr);
    }
    StoreVersion(version, parsed, __func__);
}

void APS5_VABI u_versionFromUString_nid_postfix(std::uint8_t* version, const char16_t* text) {
    if (!version || !text) return;
    char16_t units[VersionStringLength];
    std::int32_t length = 0;
    while (length < VersionStringLength) {
        if (!CopyInPreservingErrno(&units[length], text + length, sizeof(units[length]))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        if (!units[length]) break;
        ++length;
    }
    char chars[VersionStringLength + 1];
    u_UCharsToChars_nid_postfix(units, chars, length);
    chars[length] = 0;
    std::uint8_t parsed[VersionLength];
    ParseVersion(parsed, chars);
    StoreVersion(version, parsed, __func__);
}

void APS5_VABI u_versionToString_nid_postfix(const std::uint8_t* version, char* text) {
    if (!text) return;
    char host[VersionStringLength] = {};
    if (version) {
        std::uint8_t parts[VersionLength];
        if (!CopyInPreservingErrno(parts, version, sizeof(parts))) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        std::uint16_t count = VersionLength;
        while (count > 0 && parts[count - 1] == 0) --count;
        if (count <= 1) count = 2;
        char* end = WriteField(host, parts[0]);
        for (std::uint16_t part = 1; part < count; ++part) {
            *end++ = VersionDelimiter;
            end = WriteField(end, parts[part]);
        }
        *end = 0;
    }
    if (!CopyOutPreservingErrno(text, host, std::char_traits<char>::length(host) + 1)) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
}

void APS5_VABI u_getUnicodeVersion_nid_postfix(std::uint8_t* version) {
    if (!version) return;
    StoreVersion(version, UnicodeVersion, __func__);
}

void APS5_VABI u_getVersion_nid_postfix(std::uint8_t* version) {
    if (!version) return;
    std::uint8_t parsed[VersionLength];
    ParseVersion(parsed, IcuVersion);
    StoreVersion(version, parsed, __func__);
}

}
