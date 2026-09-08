/*
 * BuilderListInputRoutingTest.cpp - Prevent sidebar input from swallowing map input.
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

std::string functionBody(const std::string& source, const std::string& signature) {
    const auto begin = source.find(signature);
    REQUIRE(begin != std::string::npos);

    const auto nextFunction = source.find("\nbool BuilderList::", begin + signature.size());
    return source.substr(begin, nextFunction - begin);
}

void requireBoundsGuardBeforeDispatch(const std::string& body, const std::string& dispatch) {
    const auto boundsGuard = body.find("if((x < 0) || (x >= getSize().x) || (y < 0) || (y >= getSize().y))");
    const auto rejected = body.find("return false;", boundsGuard);
    const auto childDispatch = body.find(dispatch);

    REQUIRE(boundsGuard != std::string::npos);
    REQUIRE(rejected != std::string::npos);
    REQUIRE(childDispatch != std::string::npos);
    REQUIRE(boundsGuard < rejected);
    REQUIRE(rejected < childDispatch);
}

} // namespace

TEST_CASE("BuilderList rejects left and right input outside its interactive bounds", "[touch][input-routing]") {
    const auto builderList = readTextFile(sourceRoot() / "src" / "GUI" / "dune" / "BuilderList.cpp");

    requireBoundsGuardBeforeDispatch(
        functionBody(builderList, "bool BuilderList::handleMouseLeft"),
        "StaticContainer::handleMouseLeft");
    requireBoundsGuardBeforeDispatch(
        functionBody(builderList, "bool BuilderList::handleMouseRight"),
        "StaticContainer::handleMouseRight");
}

TEST_CASE("Game routes unconsumed interface input back to the map", "[touch][input-routing]") {
    const auto game = readTextFile(sourceRoot() / "src" / "Game.cpp");

    REQUIRE(game.find("interfaceInputConsumed = pInterface->handleMouseLeft") != std::string::npos);
    REQUIRE(game.find("interfaceInputConsumed = pInterface->handleMouseRight") != std::string::npos);
    REQUIRE(game.find("if(!interfaceInputConsumed) switch(mouse->button)") != std::string::npos);
}
