/*
 *  ProductionEconomyTest.cpp - Exact production admission payment tests.
 */

#include <catch2/catch_all.hpp>

#include <misc/CreditTransaction.h>

namespace {

int admitItems(FixPoint& storedCredits, FixPoint& startingCredits, int requested, FixPoint price) {
    int admitted = 0;
    while(admitted < requested && CreditTransaction::tryTakeExact(storedCredits, startingCredits, price)) {
        ++admitted;
    }
    return admitted;
}

} // namespace

TEST_CASE("Production payment: exact balance admits one item without remainder", "[production][economy]") {
    FixPoint storedCredits = 0;
    FixPoint startingCredits = 700;

    REQUIRE(admitItems(storedCredits, startingCredits, 1, FixPoint(700)) == 1);
    REQUIRE(storedCredits + startingCredits == 0);
}

TEST_CASE("Production payment: insufficient balance is unchanged", "[production][economy]") {
    FixPoint storedCredits = 200;
    FixPoint startingCredits = 499;

    REQUIRE(admitItems(storedCredits, startingCredits, 1, FixPoint(700)) == 0);
    REQUIRE(storedCredits == 200);
    REQUIRE(startingCredits == 499);
}

TEST_CASE("Production payment: fully funded multiple request admits every item", "[production][economy]") {
    FixPoint storedCredits = 2500;
    FixPoint startingCredits = 1000;

    REQUIRE(admitItems(storedCredits, startingCredits, 5, FixPoint(700)) == 5);
    REQUIRE(storedCredits + startingCredits == 0);
}

TEST_CASE("Production payment: partially funded multiple request stops per entry", "[production][economy]") {
    FixPoint storedCredits = 1900;
    FixPoint startingCredits = 1000;

    REQUIRE(admitItems(storedCredits, startingCredits, 5, FixPoint(700)) == 4);
    REQUIRE(storedCredits + startingCredits == 100);
}

TEST_CASE("Production payment: rounded display credits cannot authorize an exact overdraft", "[production][economy]") {
    FixPoint storedCredits = FixPoint::FromRawValue(FixPoint(700).getRawValue() - 1);
    FixPoint startingCredits = 0;
    const FixPoint before = storedCredits;

    REQUIRE_FALSE(CreditTransaction::tryTakeExact(storedCredits, startingCredits, FixPoint(700)));
    REQUIRE(storedCredits == before);
    REQUIRE(startingCredits == 0);
}
