/* Pure geometry tests for the logical-to-HD framebuffer boundary. */

#include <catch2/catch_all.hpp>

#include <misc/RenderResolution.h>

TEST_CASE("2560x1600 uses a fitting three-times framebuffer", "[render-resolution]") {
    const auto resolution = RenderResolution::create(853, 533, 2560, 1600, 3);

    REQUIRE(resolution.renderScale == 3);
    REQUIRE(resolution.framebufferWidth() == 2559);
    REQUIRE(resolution.framebufferHeight() == 1599);
    const SDL_Rect viewport = resolution.presentationViewport();
    REQUIRE(viewport.x == 0);
    REQUIRE(viewport.y == 0);
    REQUIRE(viewport.w == 2559);
    REQUIRE(viewport.h == 1599);
}

TEST_CASE("1920x1080 clamps to the largest fitting integer factor", "[render-resolution]") {
    const auto resolution = RenderResolution::create(960, 540, 1920, 1080, 3);

    REQUIRE(resolution.renderScale == 2);
    REQUIRE(resolution.framebufferWidth() <= resolution.physicalWidth);
    REQUIRE(resolution.framebufferHeight() <= resolution.physicalHeight);
}

TEST_CASE("factor one preserves logical framebuffer dimensions", "[render-resolution]") {
    const auto resolution = RenderResolution::create(640, 480, 640, 480, 1);

    REQUIRE(resolution.renderScale == 1);
    REQUIRE(resolution.framebufferWidth() == 640);
    REQUIRE(resolution.framebufferHeight() == 480);
}

TEST_CASE("logical rectangles scale every coordinate and extent", "[render-resolution]") {
    const auto resolution = RenderResolution::create(853, 533, 2560, 1600, 3);
    const SDL_Rect result = resolution.logicalRectToRenderTargetRect(SDL_Rect{10, 20, 32, 16});

    REQUIRE(result.x == 30);
    REQUIRE(result.y == 60);
    REQUIRE(result.w == 96);
    REQUIRE(result.h == 48);
}
