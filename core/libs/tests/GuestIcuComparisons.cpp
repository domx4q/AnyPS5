#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
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

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU comparisons: %s\n", message);
    std::abort();
}

int Sign(std::int32_t value) {
    return (value > 0) - (value < 0);
}

struct Points {
    std::array<std::uint32_t, 3> values{};
    std::size_t size = 0;
};

Points Decode(const char16_t* units, std::size_t count) {
    Points points;
    for (std::size_t index = 0; index < count; ++index) {
        const auto current = static_cast<std::uint32_t>(units[index]);
        if (current >= 0xd800 && current <= 0xdbff && index + 1 < count &&
            units[index + 1] >= 0xdc00 && units[index + 1] <= 0xdfff) {
            points.values[points.size++] = 0x10000 + (current - 0xd800) * 0x400 + units[++index] - 0xdc00;
        } else points.values[points.size++] = current;
    }
    return points;
}

int ComparePoints(const char16_t* first, std::size_t firstCount, const char16_t* second, std::size_t secondCount) {
    const auto left = Decode(first, firstCount);
    const auto right = Decode(second, secondCount);
    const auto mismatch = std::mismatch(left.values.begin(), left.values.begin() + left.size,
                                        right.values.begin(), right.values.begin() + right.size);
    if (mismatch.first == left.values.begin() + left.size) return mismatch.second == right.values.begin() + right.size ? 0 : -1;
    if (mismatch.second == right.values.begin() + right.size) return 1;
    return *mismatch.first < *mismatch.second ? -1 : 1;
}

std::int32_t CompareUnits(const char16_t* first, const char16_t* second, std::size_t count) {
    const auto different = std::mismatch(first, first + count, second);
    return different.first == first + count ? 0 : static_cast<std::int32_t>(*different.first) - *different.second;
}

void Exhaustive() {
    constexpr std::array<char16_t, 9> alphabet{0, u'a', 0xd7ff, 0xd800, 0xdbff, 0xdc00, 0xdfff, 0xe000, 0xffff};
    std::vector<std::array<char16_t, 4>> strings;
    for (auto first : alphabet) for (auto second : alphabet) for (auto third : alphabet)
        strings.push_back({first, second, third, 0});
    const auto saved = strings;
    for (const auto& first : strings) {
        const auto length = std::char_traits<char16_t>::length(first.data());
        Require(u_strlen_nid_postfix(first.data()) == static_cast<std::int32_t>(length), "length counts code units through first NUL");
        for (const auto& second : strings) {
            const auto secondLength = std::char_traits<char16_t>::length(second.data());
            const auto terminatedCount = std::min(length, secondLength) + 1;
            const auto terminatedDifference = CompareUnits(first.data(), second.data(), terminatedCount);
            Require(u_strcmp_nid_postfix(first.data(), second.data()) == terminatedDifference, "exact terminated code-unit difference");
            for (std::int32_t count = 0; count <= 3; ++count) {
                const auto expected = CompareUnits(first.data(), second.data(), count);
                Require(u_memcmp_nid_postfix(first.data(), second.data(), count) == expected, "exact binary difference including NUL");
                Require(u_strncmp_nid_postfix(first.data(), second.data(), count) ==
                        CompareUnits(first.data(), second.data(), std::min<std::size_t>(count, terminatedCount)), "bounded string stops at NUL or bound");
                Require(Sign(u_memcmpCodePointOrder_nid_postfix(first.data(), second.data(), count)) ==
                        ComparePoints(first.data(), count, second.data(), count), "scalar-value ordering with paired and unpaired surrogates");
            }
            Require(Sign(u_memcmpCodePointOrder_nid_postfix(first.data(), second.data(), -1)) ==
                    ComparePoints(first.data(), length, second.data(), secondLength), "negative code-point count uses string lengths");
        }
    }
    Require(strings == saved, "all inputs remain unchanged");
}

void Boundaries() {
    const std::array<char16_t, 3> supplementary{0xd800, 0xdc00, 0};
    const std::array<char16_t, 3> bmp{0xe000, 0, 0};
    Require(u_strlen_nid_postfix(supplementary.data()) == 2, "supplementary character occupies two code units");
    Require(u_memcmpCodePointOrder_nid_postfix(supplementary.data(), bmp.data(), 1) == -0x800, "boundary excludes trailing surrogate");
    Require(u_memcmpCodePointOrder_nid_postfix(supplementary.data(), bmp.data(), 2) == 0x2000, "paired lead sorts after upper BMP with exact difference");
    const std::array<char16_t, 3> longer{u'a', u'b', 0};
    const std::array<char16_t, 3> shorter{u'a', 0, 0};
    Require(u_memcmpCodePointOrder_nid_postfix(shorter.data(), longer.data(), -1) == -98, "negative length compares the first terminator exactly");
    Require(u_memcmpCodePointOrder_nid_postfix(longer.data(), shorter.data(), std::numeric_limits<std::int32_t>::min()) == 98, "all negative counts select terminated lengths");
    Require(u_strncmp_nid_postfix(u"a", u"b", std::numeric_limits<std::int32_t>::max()) == -1, "large bound stops at mismatch");
    Require(u_strncmp_nid_postfix(u"", u"", std::numeric_limits<std::int32_t>::max()) == 0, "large bound stops at terminator");
    for (const auto count : {0, -1, std::numeric_limits<std::int32_t>::min()}) {
        Require(u_memcmp_nid_postfix(nullptr, nullptr, count) == 0, "nonpositive binary count reads nothing");
        Require(u_strncmp_nid_postfix(nullptr, nullptr, count) == 0, "nonpositive string bound reads nothing");
    }
    for (const auto count : {0, -1, 1, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()})
        Require(u_memcmpCodePointOrder_nid_postfix(nullptr, nullptr, count) == 0, "identical code-point buffers read nothing for every count");
    int rejected = 0;
    try { u_strlen_nid_postfix(nullptr); } catch (const std::invalid_argument&) { ++rejected; }
    try { u_strcmp_nid_postfix(nullptr, u""); } catch (const std::invalid_argument&) { ++rejected; }
    try { u_strncmp_nid_postfix(u"", nullptr, 1); } catch (const std::invalid_argument&) { ++rejected; }
    try { u_memcmp_nid_postfix(nullptr, u"", 1); } catch (const std::invalid_argument&) { ++rejected; }
    try { u_memcmpCodePointOrder_nid_postfix(u"", nullptr, -1); } catch (const std::invalid_argument&) { ++rejected; }
    Require(rejected == 5, "invalid required buffers throw");
}

template<class TCall>
void RequiresRuntimeErrorKeepingErrno(TCall call, const char* message) {
    errno = 71;
    bool threw = false;
    try {
        call();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw && errno == 71, message);
}

void Place(char* destination, const char16_t* units, std::size_t count) {
    std::memcpy(destination, units, count * sizeof(char16_t));
}

void GuestPointers() {
#ifdef _WIN32
    const std::size_t pageSize = 4096;
    auto* base = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 3, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Require(base != nullptr, "allocate guest pointer pages");
#else
    const std::size_t pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE) > 0 ? sysconf(_SC_PAGESIZE) : 4096);
    void* mapping = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapping != MAP_FAILED, "allocate guest pointer pages");
    auto* base = static_cast<char*>(mapping);
#endif
    char* pages = base + pageSize;
    char* guard = pages + pageSize;
    Place(guard - 8, u"ab", 3);
    Place(guard - 12, u"b", 1);
    Place(guard - 2, u"a", 1);
    Place(pages - 1, u"\u2603bc", 4);
    Place(pages + 1024, u"\xd800\xdc00", 3);
#ifdef _WIN32
    DWORD oldProtect = 0;
    Require(VirtualProtect(guard, pageSize, PAGE_NOACCESS, &oldProtect) != 0, "protect guard page");
#else
    Require(mprotect(guard, pageSize, PROT_NONE) == 0, "protect guard page");
#endif
    const auto* terminated = reinterpret_cast<const char16_t*>(guard - 8);
    const auto* beforeLast = reinterpret_cast<const char16_t*>(guard - 12);
    const auto* last = reinterpret_cast<const char16_t*>(guard - 2);
    const auto* straddling = reinterpret_cast<const char16_t*>(pages - 1);
    const auto* pair = reinterpret_cast<const char16_t*>(pages + 1024);
    const auto* unmapped = reinterpret_cast<const char16_t*>(std::uintptr_t{0x10});

    Require(u_strlen_nid_postfix(terminated) == 2, "terminated length stops before the guard page");
    Require(u_strlen_nid_postfix(straddling) == 3, "odd-aligned length reads units straddling a page");
    Require(u_strcmp_nid_postfix(straddling, u"\u2603bd") == -1, "comparison reads units straddling a page");
    Require(u_strcmp_nid_postfix(terminated, u"ab") == 0 && u_strcmp_nid_postfix(u"\u2603bc", straddling) == 0, "terminated comparison stops before the guard page");
    Require(u_strncmp_nid_postfix(terminated, u"ab", std::numeric_limits<std::int32_t>::max()) == 0, "bounded comparison stops at the terminator");
    Require(u_strncmp_nid_postfix(last, u"a", 1) == 0, "bounded comparison reads only the bound");
    Require(u_memcmp_nid_postfix(terminated, u"ab", 3) == 0, "binary comparison reads exactly the count");
    Require(u_memcmp_nid_postfix(last, u"b", 2) == -1, "binary comparison stops at the first difference");
    Require(u_memcmp_nid_postfix(beforeLast, last, 1) == 1, "binary comparison reads guest buffers on both sides");
    Require(u_memcmpCodePointOrder_nid_postfix(terminated, u"ab", -1) == 0, "terminated code-point comparison stops before the guard page");
    Require(u_memcmpCodePointOrder_nid_postfix(pair, u"\xe000", 2) == 0x2000, "code-point comparison reads guest surrogate pairs");
    Require(u_memcmpCodePointOrder_nid_postfix(last, u"b", 1) == -1, "code-point comparison reads only the count");

    RequiresRuntimeErrorKeepingErrno([&] { u_strlen_nid_postfix(unmapped); }, "unreadable length source throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { u_strlen_nid_postfix(last); }, "unterminated length source throws at the guard page");
    RequiresRuntimeErrorKeepingErrno([&] { u_strcmp_nid_postfix(unmapped, u"a"); }, "unreadable first string throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { u_strcmp_nid_postfix(u"a", unmapped); }, "unreadable second string throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { u_strcmp_nid_postfix(last, u"a"); }, "unterminated string throws at the guard page");
    RequiresRuntimeErrorKeepingErrno([&] { u_strncmp_nid_postfix(unmapped, u"a", 1); }, "unreadable bounded first string throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { u_strncmp_nid_postfix(last, u"a", 2); }, "bounded comparison past the guard page throws");
    RequiresRuntimeErrorKeepingErrno([&] { u_memcmp_nid_postfix(u"a", unmapped, 1); }, "unreadable binary second buffer throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { u_memcmp_nid_postfix(last, u"a", 2); }, "binary comparison past the guard page throws");
    RequiresRuntimeErrorKeepingErrno([&] { u_memcmpCodePointOrder_nid_postfix(unmapped, u"a", 1); }, "unreadable code-point buffer throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { u_memcmpCodePointOrder_nid_postfix(u"a", unmapped, -1); }, "unreadable terminated code-point buffer throws and keeps errno");
    RequiresRuntimeErrorKeepingErrno([&] { u_memcmpCodePointOrder_nid_postfix(last, u"a", -1); }, "unterminated code-point buffer throws at the guard page");

#ifdef _WIN32
    VirtualFree(base, 0, MEM_RELEASE);
#else
    munmap(base, pageSize * 3);
#endif
}

} // namespace

int main() {
    Exhaustive();
    Boundaries();
    GuestPointers();
}
