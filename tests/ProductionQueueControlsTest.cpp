/*
 * ProductionQueueControlsTest.cpp - Deterministic queue-control policies.
 */

#include <catch2/catch_all.hpp>

#include <misc/ProductionControls.h>

#include <vector>

namespace {

struct RecordingBuilder {
    std::vector<int> queue{1};
    bool waitingToPlace = false;
    bool onHold = false;
    int setOnHoldCalls = 0;
    bool lastOnHoldState = false;
    std::uint32_t currentQueueEntryID = 7001;
    int cancelCurrentCalls = 0;
    std::uint32_t cancelledQueueEntryID = 0;
    int cancelAllCalls = 0;

    const std::vector<int>& getProductionQueue() const { return queue; }
    bool isWaitingToPlace() const { return waitingToPlace; }
    bool isOnHold() const { return onHold; }
    void handleSetOnHoldClick(bool value) { ++setOnHoldCalls; lastOnHoldState = value; }
    std::uint32_t getCurrentQueueEntryId() const { return currentQueueEntryID; }
    void handleCancelQueueEntryClick(std::uint32_t value) { ++cancelCurrentCalls; cancelledQueueEntryID = value; }
    void handleCancelAllProductionClick() { ++cancelAllCalls; }
};

struct RecordingStarport {
    bool canOrder = true;
    int orderCalls = 0;

    bool okToOrder() const { return canOrder; }
    void handlePlaceOrderClick() { ++orderCalls; }
};

} // namespace

TEST_CASE("Production queue controls expose the historical producer states", "[production][queue-controls][visibility]") {
    const auto empty = TouchInput::productionControlVisibility(false, false, false, false);
    REQUIRE_FALSE(empty.pause);
    REQUIRE_FALSE(empty.cancel);
    REQUIRE_FALSE(empty.cancelAll);
    REQUIRE_FALSE(empty.order);

    const auto active = TouchInput::productionControlVisibility(true, false, false, false);
    REQUIRE(active.pause);
    REQUIRE(active.cancel);
    REQUIRE(active.cancelAll);
    REQUIRE_FALSE(active.order);

    const auto waiting = TouchInput::productionControlVisibility(true, true, false, false);
    REQUIRE_FALSE(waiting.pause);
    REQUIRE(waiting.cancel);
    REQUIRE(waiting.cancelAll);

    const auto starport = TouchInput::productionControlVisibility(true, false, true, true);
    REQUIRE_FALSE(starport.pause);
    REQUIRE(starport.order);
    REQUIRE(starport.orderEnabled);
    REQUIRE(starport.cancel);
    REQUIRE(starport.cancelAll);

    const auto lockedStarport = TouchInput::productionControlVisibility(true, false, true, false);
    REQUIRE_FALSE(lockedStarport.pause);
    REQUIRE(lockedStarport.order);
    REQUIRE_FALSE(lockedStarport.orderEnabled);
    REQUIRE_FALSE(lockedStarport.cancel);
    REQUIRE_FALSE(lockedStarport.cancelAll);
}

TEST_CASE("Production queue controls send one valid pause or resume action", "[production][queue-controls][actions]") {
    RecordingBuilder builder;
    REQUIRE(TouchInput::requestProductionPauseToggle(builder, false));
    REQUIRE(builder.setOnHoldCalls == 1);
    REQUIRE(builder.lastOnHoldState);

    builder.onHold = true;
    REQUIRE(TouchInput::requestProductionPauseToggle(builder, false));
    REQUIRE(builder.setOnHoldCalls == 2);
    REQUIRE_FALSE(builder.lastOnHoldState);

    builder.waitingToPlace = true;
    REQUIRE_FALSE(TouchInput::requestProductionPauseToggle(builder, false));
    REQUIRE(builder.setOnHoldCalls == 2);
}

TEST_CASE("Production queue controls preserve stable queue IDs and atomic cancellation", "[production][queue-controls][actions]") {
    RecordingBuilder builder;
    REQUIRE(TouchInput::requestCancelCurrentProduction(builder));
    REQUIRE(builder.cancelCurrentCalls == 1);
    REQUIRE(builder.cancelledQueueEntryID == 7001);

    builder.currentQueueEntryID = 0;
    REQUIRE_FALSE(TouchInput::requestCancelCurrentProduction(builder));
    REQUIRE(builder.cancelCurrentCalls == 1);

    TouchInput::requestCancelAllProduction(builder);
    REQUIRE(builder.cancelAllCalls == 1);
}

TEST_CASE("Production queue controls order only an editable Starport", "[production][queue-controls][actions]") {
    RecordingStarport starport;
    REQUIRE(TouchInput::requestStarportOrder(starport));
    REQUIRE(starport.orderCalls == 1);

    starport.canOrder = false;
    REQUIRE_FALSE(TouchInput::requestStarportOrder(starport));
    REQUIRE(starport.orderCalls == 1);
}
