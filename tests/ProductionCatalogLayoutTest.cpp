/*
 * ProductionCatalogLayoutTest.cpp - Deterministic read-only catalogue grid layout.
 */

#include <catch2/catch_all.hpp>

#include <GUI/dune/ProductionCatalogLayout.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

ProductionCatalogGridLayout layoutFor(int entryCount, int width = 709, int height = 533) {
    return calculateProductionCatalogGridLayout({width, height, entryCount});
}

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

TEST_CASE("Production catalogue grid lays out one through eighteen entries deterministically", "[production][catalogue][layout]") {
    REQUIRE(layoutFor(1).columns == 1);
    REQUIRE(layoutFor(1).visibleRows == 1);
    REQUIRE(layoutFor(6).columns == 6);
    REQUIRE(layoutFor(6).visibleRows == 1);
    REQUIRE(layoutFor(7).columns == 6);
    REQUIRE(layoutFor(7).visibleRows == 2);
    REQUIRE(layoutFor(12).columns == 6);
    REQUIRE(layoutFor(12).visibleRows == 2);

    const auto eighteen = layoutFor(18);
    REQUIRE(eighteen.columns == 6);
    REQUIRE(eighteen.visibleRows == 3);
    REQUIRE(eighteen.maxVisibleEntries == 18);
    REQUIRE(eighteen.panelWidth == 581);
    REQUIRE(eighteen.panelHeight == 185);
}

TEST_CASE("Production catalogue grid reports overflow without adding scrolling", "[production][catalogue][layout]") {
    const auto layout = layoutFor(19);
    REQUIRE(layout.columns == 6);
    REQUIRE(layout.visibleRows == 3);
    REQUIRE(layout.maxVisibleEntries == 18);
    REQUIRE(layout.hasOverflow);
}

TEST_CASE("Production catalogue grid reduces columns on narrower game areas", "[production][catalogue][layout]") {
    const auto layout = layoutFor(18, 496, 480);
    REQUIRE(layout.columns == 5);
    REQUIRE(layout.visibleRows == 3);
    REQUIRE(layout.panelWidth <= 496);
}

TEST_CASE("Production catalogue grid panel never exceeds available width", "[production][catalogue][layout]") {
    for(int width = 1; width <= 709; ++width) {
        const auto layout = layoutFor(18, width, 533);
        REQUIRE(layout.panelWidth <= width);
    }
}

TEST_CASE("Production catalogue grid keeps one bounded column for tiny widths", "[production][catalogue][layout]") {
    const auto layout = layoutFor(1, 1, 1);
    REQUIRE(layout.columns == 1);
    REQUIRE(layout.panelWidth <= 1);
    REQUIRE(layout.panelHeight <= 1);
}

TEST_CASE("Production catalogue grid cell coordinates preserve catalogue order", "[production][catalogue][layout]") {
    const auto layout = layoutFor(18);
    const auto first = getProductionCatalogGridCell(layout, 0);
    const auto lastFirstRow = getProductionCatalogGridCell(layout, 5);
    const auto firstSecondRow = getProductionCatalogGridCell(layout, 6);
    const auto finalCell = getProductionCatalogGridCell(layout, 17);

    REQUIRE(first.index == 0);
    REQUIRE(lastFirstRow.column == 5);
    REQUIRE(lastFirstRow.row == 0);
    REQUIRE(firstSecondRow.column == 0);
    REQUIRE(firstSecondRow.row == 1);
    REQUIRE(finalCell.column == 5);
    REQUIRE(finalCell.row == 2);
    REQUIRE(getProductionCatalogGridCell(layout, 18).index == -1);
}

TEST_CASE("Production catalogue grid hit-test distinguishes cells from panel gaps", "[production][catalogue][layout][input-routing]") {
    const auto layout = layoutFor(18);
    const ProductionCatalogPanelBounds panel{5, 343, layout.panelWidth, layout.panelHeight};
    const auto first = getProductionCatalogGridCell(layout, 0);
    const auto finalCell = getProductionCatalogGridCell(layout, 17);
    const auto firstSecondRow = getProductionCatalogGridCell(layout, 6);

    REQUIRE(getProductionCatalogGridIndexAtPoint(layout, panel, 18, panel.x + first.x + 1, panel.y + first.y + 1) == 0);
    REQUIRE(getProductionCatalogGridIndexAtPoint(layout, panel, 18,
            panel.x + finalCell.x + 1, panel.y + finalCell.y + 1) == 17);
    REQUIRE(getProductionCatalogGridIndexAtPoint(layout, panel, 18,
            panel.x + firstSecondRow.x + 1, panel.y + firstSecondRow.y + 1) == 6);
    REQUIRE(getProductionCatalogGridIndexAtPoint(layout, panel, 18,
            panel.x + layout.padding + layout.cellWidth, panel.y + first.y + 1) == -1);
    REQUIRE(getProductionCatalogGridIndexAtPoint(layout, panel, 18,
            panel.x + first.x + 1, panel.y + layout.padding + layout.cellHeight) == -1);
    REQUIRE(getProductionCatalogGridIndexAtPoint(layout, panel, 18, panel.x, panel.y) == -1);
    REQUIRE(getProductionCatalogGridIndexAtPoint(layout, panel, 18, panel.x - 1, panel.y) == -1);
}

TEST_CASE("Production catalogue grid hit-test honors actual entry count and reduced columns", "[production][catalogue][layout][input-routing]") {
    const auto sevenEntries = layoutFor(7);
    const ProductionCatalogPanelBounds sevenPanel{5, 343, sevenEntries.panelWidth, sevenEntries.panelHeight};
    const auto missingCell = getProductionCatalogGridCell(sevenEntries, 7);
    REQUIRE(getProductionCatalogGridIndexAtPoint(sevenEntries, sevenPanel, 7,
            sevenPanel.x + missingCell.x + 1, sevenPanel.y + missingCell.y + 1) == -1);

    const auto reduced = layoutFor(18, 496, 480);
    const ProductionCatalogPanelBounds reducedPanel{5, 290, reduced.panelWidth, reduced.panelHeight};
    const auto firstSecondRow = getProductionCatalogGridCell(reduced, 5);
    REQUIRE(reduced.columns == 5);
    REQUIRE(getProductionCatalogGridIndexAtPoint(reduced, reducedPanel, 18,
            reducedPanel.x + firstSecondRow.x + 1, reducedPanel.y + firstSecondRow.y + 1) == 5);
}

TEST_CASE("Production catalogue grid consumes only events inside its visual panel", "[production][catalogue][layout][input-routing]") {
    const ProductionCatalogPanelBounds panel{5, 343, 581, 185};
    REQUIRE(isProductionCatalogPanelPointInside(panel, 5, 343));
    REQUIRE(isProductionCatalogPanelPointInside(panel, 585, 527));
    REQUIRE_FALSE(isProductionCatalogPanelPointInside(panel, 586, 527));
    REQUIRE_FALSE(isProductionCatalogPanelPointInside(panel, 4, 343));
    REQUIRE_FALSE(isProductionCatalogPanelPointInside(panel, 5, 528));
}

TEST_CASE("Production catalogue grid reuses the existing production handler only", "[production][catalogue][input-routing]") {
    const auto grid = readTextFile(sourceRoot() / "src" / "GUI" / "dune" / "ProductionCatalogGrid.cpp");

    REQUIRE(grid.find("handleProduceItemClick") != std::string::npos);
    REQUIRE(grid.find("CommandManager") == std::string::npos);
    REQUIRE(grid.find("handleCancelItemClick") == std::string::npos);
}
