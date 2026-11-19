#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

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

using Rows = double[4][4];

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "TransformationMatrix operation guest pointer check failed at line %d\n", line);
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

static WebCore::TransformationMatrix* MapReadOnly(const WebCore::TransformationMatrix& value) {
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
    return static_cast<WebCore::TransformationMatrix*>(page);
}

static void UnmapPage(void* page) {
#ifdef _WIN32
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, 4096) == 0);
#endif
}

static bool Equal(const WebCore::TransformationMatrix& matrix, const Rows& expected) {
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (matrix.m_matrix[i][j] != expected[i][j]) return false;
        }
    }
    return true;
}

static WebCore::TransformationMatrix Start() {
    WebCore::TransformationMatrix matrix;
    const Rows rows = {{2.0, 0.5, 5.0, 0.0}, {-1.0, 3.0, 0.0, 0.0}, {6.0, 0.0, 1.0, 0.0}, {7.0, -4.0, 9.0, 1.0}};
    std::memcpy(matrix.m_matrix, rows, sizeof(rows));
    return matrix;
}

template <typename Operation>
static void CheckOperation(Operation operation, const char* name, const Rows& expected) {
    const WebCore::TransformationMatrix start = Start();
    auto* unreadable = reinterpret_cast<WebCore::TransformationMatrix*>(0x10);
    WebCore::TransformationMatrix* readOnly = MapReadOnly(start);
    Require(FaultsAsNotImplemented([&] { operation(unreadable); return 0; }, name));
    Require(FaultsAsNotImplemented([&] { operation(readOnly); return 0; }, name));
    Require(std::memcmp(readOnly, &start, sizeof(start)) == 0);
    UnmapPage(readOnly);
    WebCore::TransformationMatrix matrix = start;
    operation(&matrix);
    Require(Equal(matrix, expected));
}

int main() {
    CheckOperation([](WebCore::TransformationMatrix* m) { _ZN7WebCore20TransformationMatrix10makeAffineEv(m); },
        "_ZN7WebCore20TransformationMatrix10makeAffineEv",
        {{2.0, 0.5, 0.0, 0.0}, {-1.0, 3.0, 0.0, 0.0}, {0.0, 0.0, 1.0, 0.0}, {7.0, -4.0, 0.0, 1.0}});
    CheckOperation([](WebCore::TransformationMatrix* m) { _ZN7WebCore20TransformationMatrix15scaleNonUniformEdd(m, 2.0, -0.5); },
        "_ZN7WebCore20TransformationMatrix15scaleNonUniformEdd",
        {{4.0, 1.0, 10.0, 0.0}, {0.5, -1.5, 0.0, 0.0}, {6.0, 0.0, 1.0, 0.0}, {7.0, -4.0, 9.0, 1.0}});
    CheckOperation([](WebCore::TransformationMatrix* m) { _ZN7WebCore20TransformationMatrix11translate3dEddd(m, 1.0, 2.0, 3.0); },
        "_ZN7WebCore20TransformationMatrix11translate3dEddd",
        {{2.0, 0.5, 5.0, 0.0}, {-1.0, 3.0, 0.0, 0.0}, {6.0, 0.0, 1.0, 0.0}, {25.0, 2.5, 17.0, 1.0}});
    WebCore::TransformationMatrix translation;
    translation.m_matrix[3][0] = 1.0;
    translation.m_matrix[3][1] = 2.0;
    CheckOperation([&](WebCore::TransformationMatrix* m) { _ZN7WebCore20TransformationMatrix8multiplyERKS0_(m, translation); },
        "_ZN7WebCore20TransformationMatrix8multiplyERKS0_",
        {{2.0, 0.5, 5.0, 0.0}, {-1.0, 3.0, 0.0, 0.0}, {6.0, 0.0, 1.0, 0.0}, {7.0, 2.5, 14.0, 1.0}});

    WebCore::TransformationMatrix target = Start();
    const WebCore::TransformationMatrix before = target;
    Require(FaultsAsNotImplemented([&] {
        return &_ZN7WebCore20TransformationMatrix8multiplyERKS0_(&target, *reinterpret_cast<const WebCore::TransformationMatrix*>(0x10));
    }, "_ZN7WebCore20TransformationMatrix8multiplyERKS0_"));
    Require(std::memcmp(&target, &before, sizeof(target)) == 0);
    Require(&_ZN7WebCore20TransformationMatrix11translate3dEddd(&target, 0.0, 0.0, 0.0) == &target);

    const WebCore::AffineTransform affine{{2.0, 0.5, -1.0, 3.0, 7.0, -4.0}};
    const Rows affineRows = {{2.0, 0.5, 0.0, 0.0}, {-1.0, 3.0, 0.0, 0.0}, {0.0, 0.0, 1.0, 0.0}, {7.0, -4.0, 0.0, 1.0}};
    CheckOperation([&](WebCore::TransformationMatrix* m) { _ZN7WebCore20TransformationMatrixC1ERKNS_15AffineTransformE(m, affine); },
        "_ZN7WebCore20TransformationMatrixC1ERKNS_15AffineTransformE", affineRows);
    WebCore::TransformationMatrix constructed = Start();
    _ZN7WebCore20TransformationMatrixC2ERKNS_15AffineTransformE(&constructed, affine);
    Require(Equal(constructed, affineRows));
    Require(FaultsAsNotImplemented([&] {
        _ZN7WebCore20TransformationMatrixC1ERKNS_15AffineTransformE(&target, *reinterpret_cast<const WebCore::AffineTransform*>(0x10));
        return 0;
    }, "_ZN7WebCore20TransformationMatrixC1ERKNS_15AffineTransformE"));
    Require(std::memcmp(&target, &before, sizeof(target)) == 0);
    return 0;
}
