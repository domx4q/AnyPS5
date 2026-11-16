#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::size_t PageBytes = 4096;

class GuestUnits {
public:
    GuestUnits(const char16_t* text, const char* caller) : text(text), caller(caller) {}

    char16_t operator[](std::size_t index) {
        if (index - start >= count) Load(index);
        return chunk[index - start];
    }

private:
    void Load(std::size_t index) {
        const auto base = reinterpret_cast<std::uintptr_t>(text);
        if (index > (std::numeric_limits<std::uintptr_t>::max() - base) / sizeof(char16_t)) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
        const auto address = base + index * sizeof(char16_t);
        const auto units = std::max<std::size_t>(1, (PageBytes - address % PageBytes) / sizeof(char16_t));
        const int savedErrno = errno;
        const bool ok = GuestCopyIn_nid_no_patch(chunk.data(), reinterpret_cast<const void*>(address), units * sizeof(char16_t));
        errno = savedErrno;
        if (!ok) NotImplementedAt_nid_no_patch(caller, __FILE__, __LINE__);
        start = index;
        count = units;
    }

    const char16_t* text;
    const char* caller;
    std::array<char16_t, PageBytes / sizeof(char16_t)> chunk;
    std::size_t start = 0;
    std::size_t count = 0;
};

std::int32_t Length(GuestUnits& text) {
    std::int32_t length = 0;
    while (text[static_cast<std::size_t>(length)] != 0) {
        if (length == std::numeric_limits<std::int32_t>::max()) throw std::overflow_error("ICU: string length exceeds int32");
        ++length;
    }
    return length;
}

void RequireBuffers(const char16_t* first, const char16_t* second) {
    if (first == nullptr || second == nullptr) throw std::invalid_argument("ICU: null comparison buffer");
}

bool Lead(char16_t value) {
    return value >= 0xd800 && value <= 0xdbff;
}

bool Trail(char16_t value) {
    return value >= 0xdc00 && value <= 0xdfff;
}

bool Paired(GuestUnits& text, std::size_t length, std::size_t index) {
    return (Lead(text[index]) && index + 1 < length && Trail(text[index + 1])) ||
           (Trail(text[index]) && index != 0 && Lead(text[index - 1]));
}

} // namespace

extern "C" {

std::int32_t APS5_VABI u_strlen_nid_postfix(const char16_t* text) {
    if (text == nullptr) throw std::invalid_argument("ICU: null string");
    GuestUnits units(text, __func__);
    return Length(units);
}

std::int32_t APS5_VABI u_strcmp_nid_postfix(const char16_t* first, const char16_t* second) {
    RequireBuffers(first, second);
    GuestUnits left(first, __func__);
    GuestUnits right(second, __func__);
    for (std::size_t index = 0;; ++index) {
        const auto difference = static_cast<std::int32_t>(left[index]) - static_cast<std::int32_t>(right[index]);
        if (difference != 0 || left[index] == 0) return difference;
    }
}

std::int32_t APS5_VABI u_strncmp_nid_postfix(const char16_t* first, const char16_t* second, std::int32_t count) {
    if (count <= 0) return 0;
    RequireBuffers(first, second);
    GuestUnits left(first, __func__);
    GuestUnits right(second, __func__);
    for (std::int32_t index = 0; index < count; ++index) {
        const auto difference = static_cast<std::int32_t>(left[index]) - static_cast<std::int32_t>(right[index]);
        if (difference != 0 || left[index] == 0) return difference;
    }
    return 0;
}

std::int32_t APS5_VABI u_memcmp_nid_postfix(const char16_t* first, const char16_t* second, std::int32_t count) {
    if (count <= 0) return 0;
    RequireBuffers(first, second);
    GuestUnits left(first, __func__);
    GuestUnits right(second, __func__);
    for (std::int32_t index = 0; index < count; ++index) {
        const auto difference = static_cast<std::int32_t>(left[index]) - static_cast<std::int32_t>(right[index]);
        if (difference != 0) return difference;
    }
    return 0;
}

std::int32_t APS5_VABI u_memcmpCodePointOrder_nid_postfix(const char16_t* first, const char16_t* second, std::int32_t count) {
    if (count == 0 || first == second) return 0;
    RequireBuffers(first, second);
    GuestUnits left(first, __func__);
    GuestUnits right(second, __func__);
    const auto firstLength = count < 0 ? static_cast<std::size_t>(Length(left)) + 1 : static_cast<std::size_t>(count);
    const auto secondLength = count < 0 ? static_cast<std::size_t>(Length(right)) + 1 : static_cast<std::size_t>(count);
    const auto common = std::min(firstLength, secondLength);
    for (std::size_t index = 0; index < common; ++index) {
        std::int32_t leftUnit = left[index];
        std::int32_t rightUnit = right[index];
        if (leftUnit == rightUnit) continue;
        if (leftUnit >= 0xd800 && rightUnit >= 0xd800) {
            if (!Paired(left, firstLength, index)) leftUnit -= 0x2800;
            if (!Paired(right, secondLength, index)) rightUnit -= 0x2800;
        }
        return leftUnit - rightUnit;
    }
    return 0;
}

}
