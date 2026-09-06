/*
 * LocalizationTest.cpp - Runtime PO catalogue and source-string checks.
 */

#include <catch2/catch_all.hpp>

#include <FileClasses/POFile.h>

#include <SDL.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>

namespace {

const std::array<std::string, 4> productionMsgIds = {
    "Pause", "Resume", "Cancel", "Cancel All"
};

std::filesystem::path dataDirectory() {
    const char* configured = std::getenv("DUNELEGACY_DATADIR");
    return configured != nullptr && configured[0] != '\0'
        ? std::filesystem::path(configured)
        : std::filesystem::path("data");
}

std::map<std::string, std::string> loadCatalogue(const std::string& filename) {
    const auto path = dataDirectory() / "locale" / filename;
    SDL_RWops* input = SDL_RWFromFile(path.string().c_str(), "rb");
    REQUIRE(input != nullptr);
    auto catalogue = loadPOFile(input, filename);
    SDL_RWclose(input);
    return catalogue;
}

std::string localizeOrFallback(
        const std::map<std::string, std::string>& catalogue,
        const std::string& msgid) {
    const auto found = catalogue.find(msgid);
    return found != catalogue.end() && !found->second.empty() ? found->second : msgid;
}

std::string readTextFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    REQUIRE(input.is_open());
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}

} // namespace

TEST_CASE("Production localization template contains the four canonical msgids", "[localization][production]") {
    const auto catalogue = loadCatalogue("dunelegacy.pot");
    for(const auto& msgid : productionMsgIds) {
        REQUIRE(catalogue.find(msgid) != catalogue.end());
    }
}

TEST_CASE("Production controls have exact French translations", "[localization][production]") {
    const auto catalogue = loadCatalogue("French.fr.po");
    REQUIRE(localizeOrFallback(catalogue, "Pause") == "Pause");
    REQUIRE(localizeOrFallback(catalogue, "Resume") == "Reprendre");
    REQUIRE(localizeOrFallback(catalogue, "Cancel") == "Annuler");
    REQUIRE(localizeOrFallback(catalogue, "Cancel All") == "Tout annuler");
}

TEST_CASE("Production controls have exact Spanish translations", "[localization][production]") {
    const auto catalogue = loadCatalogue("Spanish.es.po");
    REQUIRE(localizeOrFallback(catalogue, "Pause") == "Pausa");
    REQUIRE(localizeOrFallback(catalogue, "Resume") == "Reanudar");
    REQUIRE(localizeOrFallback(catalogue, "Cancel") == "Cancelar");
    REQUIRE(localizeOrFallback(catalogue, "Cancel All") == "Cancelar todo");
}

TEST_CASE("Production controls have exact German translations", "[localization][production]") {
    const auto catalogue = loadCatalogue("German.de.po");
    REQUIRE(localizeOrFallback(catalogue, "Pause") == "Pause");
    REQUIRE(localizeOrFallback(catalogue, "Resume") == "Fortsetzen");
    REQUIRE(localizeOrFallback(catalogue, "Cancel") == "Abbrechen");
    REQUIRE(localizeOrFallback(catalogue, "Cancel All") == "Alles abbrechen");
}

TEST_CASE("Missing English translations fall back to canonical msgids", "[localization][production]") {
    const auto catalogue = loadCatalogue("English.en.po");
    for(const auto& msgid : productionMsgIds) {
        REQUIRE(localizeOrFallback(catalogue, msgid) == msgid);
    }
}

TEST_CASE("BuilderList uses only canonical gettext strings for production controls", "[localization][production]") {
    const auto sourceRoot = dataDirectory().parent_path();
    const auto source = readTextFile(sourceRoot / "src" / "GUI" / "dune" / "BuilderList.cpp");

    for(const auto& msgid : productionMsgIds) {
        REQUIRE(source.find("_(\"" + msgid + "\")") != std::string::npos);
    }

    for(const std::string translated : {
            "Reprendre", "Annuler", "Tout annuler", "Pausa", "Reanudar",
            "Cancelar", "Cancelar todo", "Fortsetzen", "Abbrechen", "Alles abbrechen"}) {
        REQUIRE(source.find("\"" + translated + "\"") == std::string::npos);
    }
}
