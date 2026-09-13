/*
 * InterfaceInputRoutingTest.cpp - Keep unconsumed interface input available to the map.
 */

#include <catch2/catch_all.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::filesystem::path sourceRoot() {
    const char* configured = std::getenv("DUNELEGACY_DATADIR");
    return configured != nullptr && configured[0] != '\0'
        ? std::filesystem::path(configured).parent_path()
        : std::filesystem::path(".");
}

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.is_open());

    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}

} // namespace

TEST_CASE("Game routes unconsumed interface input back to the map", "[touch][input-routing]") {
    const auto game = readTextFile(sourceRoot() / "src" / "Game.cpp");

    REQUIRE(game.find("interfaceInputConsumed = pInterface->handleMouseLeft") != std::string::npos);
    REQUIRE(game.find("interfaceInputConsumed = pInterface->handleMouseRight") != std::string::npos);
    REQUIRE(game.find("if(!interfaceInputConsumed) switch(mouse->button)") != std::string::npos);
}
