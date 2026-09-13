/*
 * BuilderProductionControlsTest.cpp - contracts for the right-hand controls.
 */

#include <catch2/catch_all.hpp>

#include <misc/ProductionControls.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

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

TEST_CASE("Builder production controls use the shared visibility policy", "[production][controls]") {
    const auto empty = TouchInput::productionControlVisibility(false, false, false, false);
    REQUIRE_FALSE(empty.pause);
    REQUIRE_FALSE(empty.cancel);
    REQUIRE_FALSE(empty.cancelAll);

    const auto active = TouchInput::productionControlVisibility(true, false, false, false);
    REQUIRE(active.pause);
    REQUIRE(active.cancel);
    REQUIRE(active.cancelAll);

    const auto onHold = TouchInput::productionControlVisibility(true, false, false, false);
    REQUIRE(onHold.pause);

    const auto waiting = TouchInput::productionControlVisibility(true, true, false, false);
    REQUIRE_FALSE(waiting.pause);
    REQUIRE(waiting.cancel);
    REQUIRE(waiting.cancelAll);

    const auto starport = TouchInput::productionControlVisibility(true, false, true, true);
    REQUIRE(starport.order);
    REQUIRE(starport.orderEnabled);
    REQUIRE(starport.cancel);
    REQUIRE(starport.cancelAll);

    const auto lockedStarport = TouchInput::productionControlVisibility(true, false, true, false);
    REQUIRE(lockedStarport.order);
    REQUIRE_FALSE(lockedStarport.orderEnabled);
    REQUIRE_FALSE(lockedStarport.cancel);
    REQUIRE_FALSE(lockedStarport.cancelAll);
}

TEST_CASE("BuilderInterface uses the dedicated right-hand control panel", "[production][controls][architecture]") {
    const auto header = readTextFile(sourceRoot() / "include" / "GUI" / "ObjectInterfaces" / "BuilderInterface.h");

    REQUIRE(header.find("BuilderProductionControls.h") != std::string::npos);
    REQUIRE(header.find("BuilderProductionControls::create") != std::string::npos);
    REQUIRE(header.find("BuilderProductionControls* pBuilderProductionControls") != std::string::npos);
}

TEST_CASE("BuilderProductionControls delegate all actions to shared helpers", "[production][controls][actions]") {
    const auto source = readTextFile(sourceRoot() / "src" / "GUI" / "dune" / "BuilderProductionControls.cpp");

    REQUIRE(source.find("productionControlVisibility") != std::string::npos);
    REQUIRE(source.find("requestProductionPauseToggle") != std::string::npos);
    REQUIRE(source.find("requestCancelCurrentProduction") != std::string::npos);
    REQUIRE(source.find("requestCancelAllProduction") != std::string::npos);
    REQUIRE(source.find("requestStarportOrder") != std::string::npos);
    REQUIRE(source.find("CommandManager") == std::string::npos);
    REQUIRE(source.find("handleCancelItemClick") == std::string::npos);
    REQUIRE(source.find("getChoam") == std::string::npos);
}
