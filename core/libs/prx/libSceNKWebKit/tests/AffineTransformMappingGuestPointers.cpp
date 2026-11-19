#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "AffineTransform mapping guest pointer check failed at line %d\n", line);
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
    const auto* unreadable = &Unreadable<WebCore::AffineTransform>();
    const WebCore::AffineTransform transform{{2.0, 0.5, -1.0, 3.0, 7.0, -4.0}};
    const WebCore::AffineTransform translation{{1.0, 0.0, 0.0, 1.0, 3.0, 4.0}};
    const WebCore::AffineTransform identity{};

    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore15AffineTransform10isIdentityEv(unreadable); },
        "_ZNK7WebCore15AffineTransform10isIdentityEv"));
    Require(_ZNK7WebCore15AffineTransform10isIdentityEv(&identity));
    Require(!_ZNK7WebCore15AffineTransform10isIdentityEv(&translation));

    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore15AffineTransform22toTransformationMatrixEv(unreadable); },
        "_ZNK7WebCore15AffineTransform22toTransformationMatrixEv"));
    const WebCore::TransformationMatrix matrix = _ZNK7WebCore15AffineTransform22toTransformationMatrixEv(&transform);
    const double expected[4][4] = {{2.0, 0.5, 0.0, 0.0}, {-1.0, 3.0, 0.0, 0.0}, {0.0, 0.0, 1.0, 0.0}, {7.0, -4.0, 0.0, 1.0}};
    Require(std::memcmp(matrix.m_matrix, expected, sizeof(expected)) == 0);

    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore15AffineTransform6xScaleEv(unreadable); },
        "_ZNK7WebCore15AffineTransform6xScaleEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore15AffineTransform6yScaleEv(unreadable); },
        "_ZNK7WebCore15AffineTransform6yScaleEv"));
    Require(_ZNK7WebCore15AffineTransform6xScaleEv(&transform) == std::hypot(2.0, 0.5));
    Require(_ZNK7WebCore15AffineTransform6yScaleEv(&transform) == std::hypot(-1.0, 3.0));

    const WebCore::FloatPoint point{1.0f, 2.0f};
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore15AffineTransform8mapPointERKNS_10FloatPointE(unreadable, point); },
        "_ZNK7WebCore15AffineTransform8mapPointERKNS_10FloatPointE"));
    Require(FaultsAsNotImplemented([&] {
        return _ZNK7WebCore15AffineTransform8mapPointERKNS_10FloatPointE(&transform, Unreadable<WebCore::FloatPoint>());
    }, "_ZNK7WebCore15AffineTransform8mapPointERKNS_10FloatPointE"));
    Require(SamePoint(_ZNK7WebCore15AffineTransform8mapPointERKNS_10FloatPointE(&transform, point), 7.0f, 2.5f));

    const WebCore::IntPoint intPoint{1, 2};
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore15AffineTransform8mapPointERKNS_8IntPointE(unreadable, intPoint); },
        "_ZNK7WebCore15AffineTransform8mapPointERKNS_8IntPointE"));
    Require(FaultsAsNotImplemented([&] {
        return _ZNK7WebCore15AffineTransform8mapPointERKNS_8IntPointE(&transform, Unreadable<WebCore::IntPoint>());
    }, "_ZNK7WebCore15AffineTransform8mapPointERKNS_8IntPointE"));
    const WebCore::IntPoint mappedInt = _ZNK7WebCore15AffineTransform8mapPointERKNS_8IntPointE(&transform, intPoint);
    Require(mappedInt.m_x == 7 && mappedInt.m_y == 3);

    const WebCore::FloatRect rect{{0.0f, 0.0f}, {1.0f, 1.0f}};
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore15AffineTransform7mapRectERKNS_9FloatRectE(unreadable, rect); },
        "_ZNK7WebCore15AffineTransform7mapRectERKNS_9FloatRectE"));
    Require(FaultsAsNotImplemented([&] {
        return _ZNK7WebCore15AffineTransform7mapRectERKNS_9FloatRectE(&transform, Unreadable<WebCore::FloatRect>());
    }, "_ZNK7WebCore15AffineTransform7mapRectERKNS_9FloatRectE"));
    Require(SameRect(_ZNK7WebCore15AffineTransform7mapRectERKNS_9FloatRectE(&transform, rect), 6.0f, -4.0f, 3.0f, 3.5f));
    Require(SameRect(_ZNK7WebCore15AffineTransform7mapRectERKNS_9FloatRectE(&translation, rect), 3.0f, 4.0f, 1.0f, 1.0f));

    const WebCore::FloatQuad quad{{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
    Require(FaultsAsNotImplemented([&] { return _ZNK7WebCore15AffineTransform7mapQuadERKNS_9FloatQuadE(unreadable, quad); },
        "_ZNK7WebCore15AffineTransform7mapQuadERKNS_9FloatQuadE"));
    Require(FaultsAsNotImplemented([&] {
        return _ZNK7WebCore15AffineTransform7mapQuadERKNS_9FloatQuadE(&transform, Unreadable<WebCore::FloatQuad>());
    }, "_ZNK7WebCore15AffineTransform7mapQuadERKNS_9FloatQuadE"));
    const WebCore::FloatQuad mapped = _ZNK7WebCore15AffineTransform7mapQuadERKNS_9FloatQuadE(&transform, quad);
    Require(SamePoint(mapped.m_p1, 7.0f, -4.0f) && SamePoint(mapped.m_p2, 9.0f, -3.5f)
        && SamePoint(mapped.m_p3, 8.0f, -0.5f) && SamePoint(mapped.m_p4, 6.0f, -1.0f));
    const WebCore::FloatQuad moved = _ZNK7WebCore15AffineTransform7mapQuadERKNS_9FloatQuadE(&translation, quad);
    Require(SamePoint(moved.m_p1, 3.0f, 4.0f) && SamePoint(moved.m_p3, 4.0f, 5.0f));
    return 0;
}
