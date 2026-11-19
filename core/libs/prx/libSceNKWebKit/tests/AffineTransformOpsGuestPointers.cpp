#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "AffineTransform operation guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Function>
static bool FaultsAsNotImplemented(Function function, const char* name) {
    try {
        static_cast<void>(function());
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(name) + " not implemented";
    }
    return false;
}

static WebCore::AffineTransform* MapReadOnly(const WebCore::AffineTransform& value) {
#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(page != nullptr);
#else
    void* page = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(page != MAP_FAILED);
#endif
    std::memcpy(page, &value, sizeof(value));
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, 4096, PAGE_READONLY, &previous) != 0);
#else
    Require(mprotect(page, 4096, PROT_READ) == 0);
#endif
    return static_cast<WebCore::AffineTransform*>(page);
}

static void UnmapPage(void* page) {
#ifdef _WIN32
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, 4096) == 0);
#endif
}

static bool SameBytes(const WebCore::AffineTransform& a, const WebCore::AffineTransform& b) {
    return std::memcmp(&a, &b, sizeof(a)) == 0;
}

template <typename Operation>
static void CheckOperation(Operation operation, const char* name, const std::array<double, 6>& expected) {
    const WebCore::AffineTransform start{{2.0, 0.5, -1.0, 3.0, 7.0, -4.0}};
    auto* unreadable = reinterpret_cast<WebCore::AffineTransform*>(0x10);
    WebCore::AffineTransform* readOnly = MapReadOnly(start);
    Require(FaultsAsNotImplemented([&] { operation(unreadable); return 0; }, name));
    Require(FaultsAsNotImplemented([&] { operation(readOnly); return 0; }, name));
    Require(SameBytes(*readOnly, start));
    UnmapPage(readOnly);
    WebCore::AffineTransform transform = start;
    operation(&transform);
    Require(transform.m_transform == expected);
}

int main() {
    CheckOperation([](WebCore::AffineTransform* t) { _ZN7WebCore15AffineTransform12makeIdentityEv(t); },
        "_ZN7WebCore15AffineTransform12makeIdentityEv", {1.0, 0.0, 0.0, 1.0, 0.0, 0.0});
    CheckOperation([](WebCore::AffineTransform* t) { _ZN7WebCore15AffineTransform15scaleNonUniformEdd(t, 2.0, -0.5); },
        "_ZN7WebCore15AffineTransform15scaleNonUniformEdd", {4.0, 1.0, 0.5, -1.5, 7.0, -4.0});
    CheckOperation([](WebCore::AffineTransform* t) { _ZN7WebCore15AffineTransform4skewEdd(t, 0.0, 0.0); },
        "_ZN7WebCore15AffineTransform4skewEdd", {2.0, 0.5, -1.0, 3.0, 7.0, -4.0});
    CheckOperation([](WebCore::AffineTransform* t) { _ZN7WebCore15AffineTransform5flipXEv(t); },
        "_ZN7WebCore15AffineTransform5flipXEv", {-2.0, -0.5, -1.0, 3.0, 7.0, -4.0});
    CheckOperation([](WebCore::AffineTransform* t) { _ZN7WebCore15AffineTransform5flipYEv(t); },
        "_ZN7WebCore15AffineTransform5flipYEv", {2.0, 0.5, 1.0, -3.0, 7.0, -4.0});
    CheckOperation([](WebCore::AffineTransform* t) { _ZN7WebCore15AffineTransform5shearEdd(t, 1.0, 2.0); },
        "_ZN7WebCore15AffineTransform5shearEdd", {0.0, 6.5, 1.0, 3.5, 7.0, -4.0});
    CheckOperation([](WebCore::AffineTransform* t) { _ZN7WebCore15AffineTransform9translateEdd(t, 1.0, 2.0); },
        "_ZN7WebCore15AffineTransform9translateEdd", {2.0, 0.5, -1.0, 3.0, 7.0, 2.5});
    const WebCore::AffineTransform other{{1.0, 2.0, 3.0, 4.0, 5.0, 6.0}};
    CheckOperation([&](WebCore::AffineTransform* t) { _ZN7WebCore15AffineTransform8multiplyERKS0_(t, other); },
        "_ZN7WebCore15AffineTransform8multiplyERKS0_", {0.0, 6.5, 2.0, 13.5, 11.0, 16.5});

    WebCore::AffineTransform translated{{1.0, 0.0, 0.0, 1.0, 3.0, 4.0}};
    _ZN7WebCore15AffineTransform9translateEdd(&translated, 1.0, -2.0);
    Require(translated.m_transform == (std::array<double, 6>{1.0, 0.0, 0.0, 1.0, 4.0, 2.0}));

    WebCore::AffineTransform self{{2.0, 0.5, -1.0, 3.0, 7.0, -4.0}};
    _ZN7WebCore15AffineTransform8multiplyERKS0_(&self, self);
    Require(self.m_transform == (std::array<double, 6>{3.5, 2.5, -5.0, 8.5, 25.0, -12.5}));

    WebCore::AffineTransform target{{2.0, 0.5, -1.0, 3.0, 7.0, -4.0}};
    const WebCore::AffineTransform before = target;
    Require(FaultsAsNotImplemented([&] {
        return &_ZN7WebCore15AffineTransform8multiplyERKS0_(&target, *reinterpret_cast<const WebCore::AffineTransform*>(0x10));
    }, "_ZN7WebCore15AffineTransform8multiplyERKS0_"));
    Require(SameBytes(target, before));
    Require(&_ZN7WebCore15AffineTransform5flipXEv(&target) == &target);
    return 0;
}
