/*
 * SaveGameNameTest.cpp - Automatic save-name behavior.
 */

#include <catch2/catch_all.hpp>

#include <misc/SaveGameName.h>
#include <misc/string_util.h>

#include <set>
#include <string>

namespace {

std::tm sampleTime() {
    std::tm value = {};
    value.tm_year = 2026 - 1900;
    value.tm_mon = 8;
    value.tm_mday = 7;
    value.tm_hour = 22;
    value.tm_min = 24;
    value.tm_sec = 32;
    return value;
}

} // namespace

TEST_CASE("Campaign saves receive a readable timestamped name", "[save-name]") {
    const auto name = SaveGameName::create("Atreides - Mission 05", sampleTime(),
        [](const std::string&) { return false; });

    REQUIRE(name == "Atreides - Mission 05 - 2026-09-07 22-24-32");
}

TEST_CASE("Custom map names are sanitized for filesystems", "[save-name]") {
    const auto name = SaveGameName::create("  My? *Map:/\\\" <Final>.  ", sampleTime(),
        [](const std::string&) { return false; });

    REQUIRE(name == "My Map Final - 2026-09-07 22-24-32");
    REQUIRE(name.find_first_of("?*:|<>/\\\"") == std::string::npos);
}

TEST_CASE("Missing context uses the generic fallback", "[save-name]") {
    const auto name = SaveGameName::create("<>/", sampleTime(),
        [](const std::string&) { return false; });

    REQUIRE(name == "Save - 2026-09-07 22-24-32");
}

TEST_CASE("Existing automatic names receive an incrementing suffix", "[save-name]") {
    const std::set<std::string> existing = {
        "Arrakis - 2026-09-07 22-24-32",
        "Arrakis - 2026-09-07 22-24-32 (2)"
    };

    const auto name = SaveGameName::create("Arrakis", sampleTime(),
        [&existing](const std::string& candidate) { return existing.count(candidate) != 0; });

    REQUIRE(name == "Arrakis - 2026-09-07 22-24-32 (3)");
}

TEST_CASE("Long UTF-8 map names preserve timestamp and length limit", "[save-name]") {
    const std::string context = "Très longue carte personnalisée avec un nom qui dépasse largement la limite";
    const auto name = SaveGameName::create(context, sampleTime(),
        [](const std::string& candidate) { return candidate.find("(2)") == std::string::npos; });

    const std::string expectedEnd = "2026-09-07 22-24-32 (2)";
    REQUIRE(name.size() >= expectedEnd.size());
    REQUIRE(name.compare(name.size() - expectedEnd.size(), expectedEnd.size(), expectedEnd) == 0);
    REQUIRE(utf8Length(name) <= SaveGameName::MAX_NAME_LENGTH);
}
