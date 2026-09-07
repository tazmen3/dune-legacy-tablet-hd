/*
 * SelectionControlTest.cpp - deterministic global selection reset tests.
 */

#include <catch2/catch_all.hpp>

#include <misc/SelectionControl.h>

#include <cstdint>
#include <map>

namespace {

struct TestObject {
    bool selected = true;

    void setSelected(bool value) {
        selected = value;
    }
};

} // namespace

TEST_CASE("Global deselect visibility follows selection presence", "[selection][ui]") {
    REQUIRE_FALSE(SelectionControl::isGlobalDeselectVisible(0));
    REQUIRE(SelectionControl::isGlobalDeselectVisible(1));
    REQUIRE(SelectionControl::isGlobalDeselectVisible(3));
}

TEST_CASE("Global deselect clears one selected object and notifies once", "[selection]") {
    std::set<std::uint32_t> selected{7};
    std::map<std::uint32_t, TestObject> objects{{7, {true}}};
    int stateResets = 0;
    int selectionChanges = 0;

    const bool cleared = SelectionControl::clearSelection(
        selected,
        [&](std::uint32_t objectID) { objects.at(objectID).setSelected(false); },
        [&] { ++stateResets; },
        [&] { ++selectionChanges; });

    REQUIRE(cleared);
    REQUIRE(selected.empty());
    REQUIRE_FALSE(objects.at(7).selected);
    REQUIRE(stateResets == 1);
    REQUIRE(selectionChanges == 1);
}

TEST_CASE("Global deselect clears every object and ignores an empty selection", "[selection]") {
    std::set<std::uint32_t> selected{3, 9, 12};
    std::map<std::uint32_t, TestObject> objects{{3, {true}}, {9, {true}}, {12, {true}}};
    int stateResets = 0;
    int selectionChanges = 0;
    auto clear = [&] {
        return SelectionControl::clearSelection(
            selected,
            [&](std::uint32_t objectID) { objects.at(objectID).setSelected(false); },
            [&] { ++stateResets; },
            [&] { ++selectionChanges; });
    };

    REQUIRE(clear());
    REQUIRE(selected.empty());
    REQUIRE_FALSE(objects.at(3).selected);
    REQUIRE_FALSE(objects.at(9).selected);
    REQUIRE_FALSE(objects.at(12).selected);
    REQUIRE(stateResets == 1);
    REQUIRE(selectionChanges == 1);
    REQUIRE_FALSE(clear());
    REQUIRE(stateResets == 1);
    REQUIRE(selectionChanges == 1);
}
