#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "TransformationMatrix mapping guest pointer check failed at line %d\n", line);
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

template <typename T>
static const T& Unreadable() {
    return *reinterpret_cast<const T*>(0x10);
}

static bool SamePoint(const WebCore::FloatPoint& point, float x, float y) {
    return point.m_x == x && point.m_y == y;
}

static bool SameRect(const WebCore::FloatRect& rect, float x, float y, float width, float height) {
    return SamePoint(rect.m_location, x, y) && rect.m_size.m_width == width && rect.m_size.m_height == height;
}

int main() {
    const auto* unreadable = &Unreadable<WebCore::TransformationMatrix>();
    WebCore::TransformationMatrix affine;
    affine.m_matrix[0][0] = 2.0;
    affine.m_matrix[0][1] = 0.5;
    affine.m_matrix[1][0] = -1.0;
    affine.m_matrix[1][1] = 3.0;
    affine.m_matrix[3][0] = 7.0;
    affine.m_matrix[3][1] = -4.0;
    WebCore::TransformationMatrix translation;
    translation.m_matrix[3][0] = 3.0;
    translation.m_matrix[3][1] = 4.0;
    WebCore::TransformationMatrix singular;
    singular.m_matrix[0][0] = 1.0;
    singular.m_matrix[0][1] = 2.0;
    singular.m_matrix[1][0] = 2.0;
    singular.m_matrix[1][1] = 4.0;

    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore20TransformationMatrix12isInvertibleEv(unreadable); },
        "_ZNK7WebCore20TransformationMatrix12isInvertibleEv"));
    Require(_ZNK7WebCore20TransformationMatrix12isInvertibleEv(&affine));
    Require(_ZNK7WebCore20TransformationMatrix12isInvertibleEv(&translation));
    Require(!_ZNK7WebCore20TransformationMatrix12isInvertibleEv(&singular));

    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore20TransformationMatrix17toAffineTransformEv(unreadable); },
        "_ZNK7WebCore20TransformationMatrix17toAffineTransformEv"));
    Require(_ZNK7WebCore20TransformationMatrix17toAffineTransformEv(&affine).m_transform
        == (std::array<double, 6>{2.0, 0.5, -1.0, 3.0, 7.0, -4.0}));

    const WebCore::FloatPoint point{1.0f, 2.0f};
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore20TransformationMatrix8mapPointERKNS_10FloatPointE(unreadable, point); },
        "_ZNK7WebCore20TransformationMatrix8mapPointERKNS_10FloatPointE"));
    Require(FaultsAsNotImplemented([&] {
        return _ZNK7WebCore20TransformationMatrix8mapPointERKNS_10FloatPointE(&affine, Unreadable<WebCore::FloatPoint>());
    }, "_ZNK7WebCore20TransformationMatrix8mapPointERKNS_10FloatPointE"));
    Require(SamePoint(_ZNK7WebCore20TransformationMatrix8mapPointERKNS_10FloatPointE(&affine, point), 7.0f, 2.5f));
    Require(SamePoint(_ZNK7WebCore20TransformationMatrix8mapPointERKNS_10FloatPointE(&translation, point), 4.0f, 6.0f));

    const WebCore::FloatRect rect{{0.0f, 0.0f}, {1.0f, 1.0f}};
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore20TransformationMatrix7mapRectERKNS_9FloatRectE(unreadable, rect); },
        "_ZNK7WebCore20TransformationMatrix7mapRectERKNS_9FloatRectE"));
    Require(FaultsAsNotImplemented([&] {
        return _ZNK7WebCore20TransformationMatrix7mapRectERKNS_9FloatRectE(&affine, Unreadable<WebCore::FloatRect>());
    }, "_ZNK7WebCore20TransformationMatrix7mapRectERKNS_9FloatRectE"));
    Require(SameRect(_ZNK7WebCore20TransformationMatrix7mapRectERKNS_9FloatRectE(&affine, rect), 6.0f, -4.0f, 3.0f, 3.5f));
    Require(SameRect(_ZNK7WebCore20TransformationMatrix7mapRectERKNS_9FloatRectE(&translation, rect), 3.0f, 4.0f, 1.0f, 1.0f));

    const WebCore::FloatQuad quad{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore20TransformationMatrix7mapQuadERKNS_9FloatQuadE(unreadable, quad); },
        "_ZNK7WebCore20TransformationMatrix7mapQuadERKNS_9FloatQuadE"));
    Require(FaultsAsNotImplemented([&] {
        return _ZNK7WebCore20TransformationMatrix7mapQuadERKNS_9FloatQuadE(&affine, Unreadable<WebCore::FloatQuad>());
    }, "_ZNK7WebCore20TransformationMatrix7mapQuadERKNS_9FloatQuadE"));
    const WebCore::FloatQuad mapped = _ZNK7WebCore20TransformationMatrix7mapQuadERKNS_9FloatQuadE(&affine, quad);
    Require(SamePoint(mapped.m_p1, 7.0f, -4.0f) && SamePoint(mapped.m_p2, 9.0f, -3.5f)
        && SamePoint(mapped.m_p3, 8.0f, -0.5f) && SamePoint(mapped.m_p4, 6.0f, -1.0f));
    const WebCore::FloatQuad moved = _ZNK7WebCore20TransformationMatrix7mapQuadERKNS_9FloatQuadE(&translation, quad);
    Require(SamePoint(moved.m_p1, 3.0f, 4.0f) && SamePoint(moved.m_p3, 4.0f, 5.0f));

    Require(FaultsAsNotImplemented([&] {
        return _ZNK7WebCore24MatrixTransformOperation6matrixEv(&Unreadable<WebCore::MatrixTransformOperation>());
    }, "_ZNK7WebCore24MatrixTransformOperation6matrixEv"));
    WebCore::MatrixTransformOperation operation{};
    operation.m_a = 2.0;
    operation.m_b = 0.5;
    operation.m_c = -1.0;
    operation.m_d = 3.0;
    operation.m_e = 7.0;
    operation.m_f = -4.0;
    const WebCore::TransformationMatrix fromOperation = _ZNK7WebCore24MatrixTransformOperation6matrixEv(&operation);
    Require(std::memcmp(fromOperation.m_matrix, affine.m_matrix, sizeof(affine.m_matrix)) == 0);
    return 0;
}
