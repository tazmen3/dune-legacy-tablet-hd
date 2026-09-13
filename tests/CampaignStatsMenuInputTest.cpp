/* CampaignStatsMenu input routing tests. */

#include <catch2/catch_all.hpp>

#include <Menu/CampaignStatsMenu.h>

TEST_CASE("Campaign stats save button consumes only a left release inside its touch area", "[campaign-stats][touch]") {
    const SDL_Rect buttonRect {100, 200, 180, 40};
    const Point windowPosition(20, 30);

    SDL_MouseButtonEvent release {};
    release.type = SDL_MOUSEBUTTONUP;
    release.button = SDL_BUTTON_LEFT;
    release.x = 120;
    release.y = 250;

    REQUIRE(CampaignStatsMenuInput::isSaveButtonReleaseInside(buttonRect, release, windowPosition));

    release.x = 99;
    REQUIRE_FALSE(CampaignStatsMenuInput::isSaveButtonReleaseInside(buttonRect, release, windowPosition));

    release.x = 120;
    release.button = SDL_BUTTON_RIGHT;
    REQUIRE_FALSE(CampaignStatsMenuInput::isSaveButtonReleaseInside(buttonRect, release, windowPosition));
}
