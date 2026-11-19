#include "prx/libc/include/Api.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

struct Case {
    const char* label;
    std::uint64_t input[6];
    std::uint64_t first;
    std::uint64_t second;
    std::uint64_t output[6];
    std::uint64_t result;
};

const Case GetCases[] = {
    {"two digits", {0x89abcdef, 0x1234567, 0x5, 0x6, 0x7, 0x8}, 0x0, 0x0, {0x89abcdef, 0x1234567, 0x5, 0x6, 0x7, 0x8}, 0x123456789abcdef},
    {"wide low word", {0xfedcba9876543210, 0x1, 0x0, 0x0, 0x0, 0x0}, 0x0, 0x0, {0xfedcba9876543210, 0x1, 0x0, 0x0, 0x0, 0x0}, 0xfedcba9976543210},
    {"wide high word", {0x10, 0xffffffff00000002, 0x0, 0x0, 0x0, 0x0}, 0x0, 0x0, {0x10, 0xffffffff00000002, 0x0, 0x0, 0x0, 0x0}, 0x200000010},
};

const Case AddCases[] = {
    {"no carry", {0x1, 0x2, 0x3, 0x4, 0x0, 0x77}, 0x500000006, 0x0, {0x7, 0x7, 0x3, 0x4, 0x0, 0x77}, 0x0},
    {"carry into the last word", {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x0, 0x77}, 0x1, 0x0, {0x0, 0x0, 0x0, 0x0, 0x1, 0x77}, 0x0},
    {"carry stops early", {0xffffffff, 0xffffffff, 0x0, 0x9, 0x0, 0x77}, 0x1, 0x0, {0x0, 0x0, 0x1, 0x9, 0x0, 0x77}, 0x0},
    {"largest addend", {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x77}, 0xffffffffffffffff, 0x0, {0xfffffffe, 0xffffffff, 0x0, 0x0, 0x0, 0x77}, 0x0},
    {"wide words", {0x1ffffffff, 0x300000000, 0x2ffffffff, 0x7, 0x0, 0x77}, 0xffffffff00000001, 0x0, {0x0, 0x1, 0x3, 0xa, 0x0, 0x77}, 0x0},
};

const Case MulCases[] = {
    {"zero", {0x9, 0x9, 0x9, 0x9, 0x9, 0x77}, 0x0, 0x0, {0x0, 0x0, 0x0, 0x0, 0x0, 0x77}, 0x0},
    {"largest", {0x9, 0x9, 0x9, 0x9, 0x9, 0x77}, 0xffffffffffffffff, 0xffffffffffffffff, {0x1, 0x0, 0xfffffffe, 0xffffffff, 0x0, 0x77}, 0x0},
    {"low multiplier zero", {0x9, 0x9, 0x9, 0x9, 0x9, 0x77}, 0x123456789abcdef0, 0xfedcba9800000000, {0x0, 0xd05ebe80, 0xcf13578a, 0x121fa00a, 0x0, 0x77}, 0x0},
    {"high multiplier zero", {0x9, 0x9, 0x9, 0x9, 0x9, 0x77}, 0x123456789abcdef0, 0xfedcba98, {0xd05ebe80, 0xcf13578a, 0x121fa00a, 0x0, 0x0, 0x77}, 0x0},
    {"mixed", {0x9, 0x9, 0x9, 0x9, 0x9, 0x77}, 0x5851f42d4c957f2d, 0x14057b7ef767814f, {0x8d52ebe3, 0x6037292, 0x7ce0e720, 0x6e84b48, 0x0, 0x77}, 0x0},
};

const Case RemCases[] = {
    {"zero dividend", {0x0, 0x0, 0x0, 0x0, 0x0, 0x0}, 0x123456789, 0x0, {0x0, 0x0, 0x0, 0x0, 0x0, 0x0}, 0x0},
    {"dividend shorter than divisor", {0x1234, 0x0, 0x0, 0x0, 0x0, 0x0}, 0x500000000, 0x0, {0x1234, 0x0, 0x0, 0x0, 0x0, 0x0}, 0x0},
    {"dividend below divisor", {0x89abcdef, 0x4, 0x0, 0x0, 0x0, 0x0}, 0x500000000, 0x0, {0x89abcdef, 0x4, 0x0, 0x0, 0x0, 0x0}, 0x0},
    {"top divisor digit all ones", {0x11111111, 0x22222222, 0x33333333, 0x44444444, 0x0, 0x0}, 0xffffffff00000007, 0x0, {0xccccccde, 0xbbbbbbb7, 0x0, 0x0, 0x0, 0x0}, 0x0},
    {"normalized divisor", {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x0, 0x0}, 0x8000000000000001, 0x0, {0x3, 0x0, 0x0, 0x0, 0x0, 0x0}, 0x0},
    {"small top divisor digit", {0xffffffff, 0x0, 0xffffffff, 0x7fffffff, 0x0, 0x0}, 0x1ffffffff, 0x0, {0x8fffffff, 0x0, 0x0, 0x0, 0x0, 0x0}, 0x0},
    {"engine step 1", {0xcddb7546, 0x1a45dbe2, 0x7ce0e720, 0x6e84b48, 0x0, 0x0}, 0xfffffffffffffc5b, 0x0, {0xed862803, 0x46e03b11, 0x0, 0x0, 0x0, 0x0}, 0x0},
    {"engine step 2", {0xf85b520c, 0xdd3dd2ab, 0xde82ef64, 0xd1342543, 0x0, 0x0}, 0xffffffffffffec5b, 0x0, {0x1bbf6c86, 0x929de0ee, 0x0, 0x0, 0x0, 0x0}, 0x0},
    {"engine step 3", {0xccf2b8c4, 0xf4751d2, 0x4cec3906, 0x170940b2, 0x0, 0x0}, 0xffffffffffffdc5b, 0x0, {0xad719bcb, 0x2c166542, 0x0, 0x0, 0x0, 0x0}, 0x0},
};

void Require(bool condition, const char* label, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "sce v2 multiprecision: %s: %s\n", label, message);
    std::abort();
}

void Check(const Case& entry, const std::uint64_t* words) { Require(std::memcmp(words, entry.output, sizeof(entry.output)) == 0, entry.label, "words"); }

std::uint64_t Remainder(const std::uint64_t* words, std::uint64_t divisor) {
    unsigned __int128 rest = 0;
    for (int index = 4; index >= 0; --index) rest = ((rest << 32) + words[index]) % divisor;
    return static_cast<std::uint64_t>(rest);
}

void CheckSingleWordDivisors() {
    const std::uint64_t divisors[] = {1, 3, 0x80, 0xb7727df1, 0x7fffffff, 0xffffffff};
    const std::uint64_t dividends[][5] = {
        {0x337cae9f, 0x97440f26, 0, 0, 0},
        {0, 1, 0, 0, 0},
        {5, 3, 0, 0, 0},
        {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0},
        {0x12345678, 0x9abcdef0, 0x0fedcba9, 0x87654321, 0},
    };
    for (const auto divisor : divisors) {
        for (const auto& dividend : dividends) {
            std::uint64_t words[6] {dividend[0], dividend[1], dividend[2], dividend[3], dividend[4], 0x77};
            const std::uint64_t expected = Remainder(dividend, divisor);
            _ZNSt8__sce_v27_MP_RemEPyy_nid_postfix(words, divisor);
            Require(words[0] == (expected & 0xffffffff) && words[1] == expected >> 32, "one-word divisor", "remainder");
            Require(words[2] == 0 && words[3] == 0 && words[4] == 0 && words[5] == 0x77, "one-word divisor", "upper words");
        }
    }
}

class GuardedPage {
public:
    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        allocation = static_cast<unsigned char*>(VirtualAlloc(nullptr, size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(allocation != nullptr, "guarded page", "allocation");
        DWORD previous = 0;
        Require(VirtualProtect(allocation + size, size, PAGE_NOACCESS, &previous) != 0, "guarded page", "guard");
#else
        size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        void* mapping = mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapping != MAP_FAILED, "guarded page", "allocation");
        allocation = static_cast<unsigned char*>(mapping);
        Require(mprotect(allocation + size, size, PROT_NONE) == 0, "guarded page", "guard");
#endif
    }
    GuardedPage(const GuardedPage&) = delete;
    GuardedPage& operator=(const GuardedPage&) = delete;
    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(allocation, 0, MEM_RELEASE);
#else
        munmap(allocation, size * 2);
#endif
    }
    std::uint64_t* WordsBeforeGuard(std::size_t count) const { return reinterpret_cast<std::uint64_t*>(allocation + size) - count; }
    void ReadOnly() {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(allocation, size, PAGE_READONLY, &previous) != 0, "guarded page", "read-only");
#else
        Require(mprotect(allocation, size, PROT_READ) == 0, "guarded page", "read-only");
#endif
    }

private:
    unsigned char* allocation = nullptr;
    std::size_t size = 0;
};

template<typename Call>
bool Rejects(Call call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void CheckGuestPointers() {
    auto* const unmapped = reinterpret_cast<std::uint64_t*>(std::uintptr_t{0x10});
    Require(Rejects([&] { _ZNSt8__sce_v27_MP_GetEPy_nid_postfix(unmapped); }), "unmapped", "get");
    Require(Rejects([&] { _ZNSt8__sce_v27_MP_AddEPyy_nid_postfix(unmapped, 1); }), "unmapped", "add");
    Require(Rejects([&] { _ZNSt8__sce_v27_MP_MulEPyyy_nid_postfix(unmapped, 2, 3); }), "unmapped", "mul");
    Require(Rejects([&] { _ZNSt8__sce_v27_MP_RemEPyy_nid_postfix(unmapped, 3); }), "unmapped", "rem");
    _ZNSt8__sce_v27_MP_RemEPyy_nid_postfix(unmapped, 0);

    GuardedPage page;
    std::uint64_t* const low = page.WordsBeforeGuard(2);
    low[0] = 0x89abcdef;
    low[1] = 0x1234567;
    Require(_ZNSt8__sce_v27_MP_GetEPy_nid_postfix(low) == 0x123456789abcdef, "two words before a guard page", "value");
    Require(Rejects([&] { _ZNSt8__sce_v27_MP_AddEPyy_nid_postfix(low, 1); }), "two words before a guard page", "add");
    Require(Rejects([&] { _ZNSt8__sce_v27_MP_RemEPyy_nid_postfix(low, 3); }), "two words before a guard page", "rem");
    Require(low[0] == 0x89abcdef && low[1] == 0x1234567, "two words before a guard page", "unchanged");
    Require(Rejects([&] { _ZNSt8__sce_v27_MP_MulEPyyy_nid_postfix(low, 2, 3); }), "two words before a guard page", "mul");

    std::uint64_t* const value = page.WordsBeforeGuard(5);
    const std::uint64_t original[5] {0xffffffff, 0xffffffff, 0x5, 0x6, 0x0};
    std::memcpy(value, original, sizeof(original));
    page.ReadOnly();
    Require(_ZNSt8__sce_v27_MP_GetEPy_nid_postfix(value) == 0xffffffffffffffff, "read-only", "get");
    Require(Rejects([&] { _ZNSt8__sce_v27_MP_AddEPyy_nid_postfix(value, 1); }), "read-only", "add");
    Require(Rejects([&] { _ZNSt8__sce_v27_MP_MulEPyyy_nid_postfix(value, 2, 3); }), "read-only", "mul");
    Require(Rejects([&] { _ZNSt8__sce_v27_MP_RemEPyy_nid_postfix(value, 3); }), "read-only", "rem");
    Require(std::memcmp(value, original, sizeof(original)) == 0, "read-only", "unchanged");
}

}

int main() {
    for (const auto& entry : GetCases) {
        std::uint64_t words[6];
        std::memcpy(words, entry.input, sizeof(words));
        Require(_ZNSt8__sce_v27_MP_GetEPy_nid_postfix(words) == entry.result, entry.label, "value");
        Check(entry, words);
    }
    for (const auto& entry : AddCases) {
        std::uint64_t words[6];
        std::memcpy(words, entry.input, sizeof(words));
        _ZNSt8__sce_v27_MP_AddEPyy_nid_postfix(words, entry.first);
        Check(entry, words);
    }
    for (const auto& entry : MulCases) {
        std::uint64_t words[6];
        std::memcpy(words, entry.input, sizeof(words));
        _ZNSt8__sce_v27_MP_MulEPyyy_nid_postfix(words, entry.first, entry.second);
        Check(entry, words);
    }
    for (const auto& entry : RemCases) {
        std::uint64_t words[6];
        std::memcpy(words, entry.input, sizeof(words));
        _ZNSt8__sce_v27_MP_RemEPyy_nid_postfix(words, entry.first);
        Check(entry, words);
    }
    CheckSingleWordDivisors();
    CheckGuestPointers();
    return 0;
}
