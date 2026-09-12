/*
 * ProductionControlsTest.cpp - Deterministic production touch and control tests.
 */

#include <catch2/catch_all.hpp>

#include <misc/ProductionControls.h>

namespace {

struct RecordingBuilder {
    std::uint32_t currentQueueEntryID = 7001;
    int cancelCurrentCalls = 0;
    int cancelAllCalls = 0;
    std::uint32_t cancelledQueueEntryID = 0;

    std::uint32_t getCurrentQueueEntryId() const { return currentQueueEntryID; }
    void handleCancelQueueEntryClick(std::uint32_t queueEntryID) {
        ++cancelCurrentCalls;
        cancelledQueueEntryID = queueEntryID;
    }
    void handleCancelAllProductionClick() { ++cancelAllCalls; }
};

TouchInput::ProductionCatalogTarget target() {
    return { 42, 100, 200, 91, 175 };
}

TouchInput::ProductionCatalogTarget gridTarget() {
    return { 77, 20, 340, 581, 185 };
}

TouchInput::ProductionCatalogTouchAction releaseAfter(std::uint32_t elapsed) {
    TouchInput::ProductionCatalogTouchGuard touch;
    REQUIRE(touch.begin(target(), 120, 210, 1000));
    return touch.release(120, 210, 1000 + elapsed);
}

} // namespace

TEST_CASE("Production catalogue: short touch is add-only tap", "[touch][production]") {
    REQUIRE(releaseAfter(100) == TouchInput::ProductionCatalogTouchAction::Tap);
    REQUIRE(releaseAfter(599) == TouchInput::ProductionCatalogTouchAction::Tap);
}

TEST_CASE("Production catalogue: five rapid taps remain five catalogue requests", "[touch][production]") {
    int catalogueRequests = 0;
    for(int i = 0; i < 5; ++i) {
        if(TouchInput::isProductionCatalogTap(releaseAfter(100))) ++catalogueRequests;
    }
    REQUIRE(catalogueRequests == 5);
}

TEST_CASE("Production catalogue: historical 600 ms long press is consumed", "[touch][production]") {
    REQUIRE(releaseAfter(600) == TouchInput::ProductionCatalogTouchAction::HoldSuppressed);
    REQUIRE(releaseAfter(800) == TouchInput::ProductionCatalogTouchAction::HoldSuppressed);
    REQUIRE(releaseAfter(2000) == TouchInput::ProductionCatalogTouchAction::HoldSuppressed);
    REQUIRE_FALSE(TouchInput::isProductionCatalogTap(TouchInput::ProductionCatalogTouchAction::HoldSuppressed));
}

TEST_CASE("Production catalogue: a hold emits only one suppressed result", "[touch][production]") {
    TouchInput::ProductionCatalogTouchGuard touch;
    REQUIRE(touch.begin(target(), 120, 210, 500));
    REQUIRE(touch.classify(2500) == TouchInput::ProductionCatalogTouchAction::HoldSuppressed);
    REQUIRE(touch.isTracking());
    REQUIRE(touch.release(120, 210, 2500) == TouchInput::ProductionCatalogTouchAction::HoldSuppressed);
    REQUIRE(touch.release(120, 210, 2600) == TouchInput::ProductionCatalogTouchAction::None);
}

TEST_CASE("Production catalogue: micro movement stays armed", "[touch][production]") {
    TouchInput::ProductionCatalogTouchGuard touch;
    REQUIRE(touch.begin(target(), 120, 210, 10));
    touch.move(99, 199);
    REQUIRE(touch.isArmed());
    touch.move(88, 188);
    REQUIRE(touch.isArmed());
    REQUIRE(touch.release(88, 188, 110) == TouchInput::ProductionCatalogTouchAction::Tap);
}

TEST_CASE("Production catalogue: leaving the tolerated region cancels the touch", "[touch][production]") {
    TouchInput::ProductionCatalogTouchGuard touch;
    REQUIRE(touch.begin(target(), 120, 210, 10));
    touch.move(87, 210);
    REQUIRE_FALSE(touch.isArmed());
    touch.move(120, 210);
    REQUIRE_FALSE(touch.isArmed());
    REQUIRE(touch.release(120, 210, 2510) == TouchInput::ProductionCatalogTouchAction::None);
}

TEST_CASE("Production catalogue: multi-touch or focus cancellation has no action", "[touch][production]") {
    TouchInput::ProductionCatalogTouchGuard touch;
    REQUIRE(touch.begin(target(), 120, 210, 10));
    touch.cancel();
    REQUIRE(touch.classify(2510) == TouchInput::ProductionCatalogTouchAction::None);
    REQUIRE(touch.release(120, 210, 2510) == TouchInput::ProductionCatalogTouchAction::None);
}

TEST_CASE("Production catalogue: invalid and outside starts are ignored", "[touch][production]") {
    TouchInput::ProductionCatalogTouchGuard touch;
    auto invalid = target();
    invalid.builderObjectID = 0;
    REQUIRE_FALSE(touch.begin(invalid, 120, 210, 10));
    REQUIRE_FALSE(touch.begin(target(), 99, 210, 10));
    REQUIRE(touch.classify(2510) == TouchInput::ProductionCatalogTouchAction::None);
}

TEST_CASE("Production catalogue: timestamp wrap preserves hold suppression", "[touch][production]") {
    TouchInput::ProductionCatalogTouchGuard touch;
    REQUIRE(touch.begin(target(), 120, 210, 0xFFFFFF00u));
    REQUIRE(touch.release(120, 210, 0x000006D0u) == TouchInput::ProductionCatalogTouchAction::HoldSuppressed);
}

TEST_CASE("Production catalogue target slots preserve the historical BuilderList region", "[touch][production][targets]") {
    TouchInput::ProductionCatalogTargetRegistry targets;
    targets.set(TouchInput::ProductionCatalogTargetSource::LegacyBuilderList, target());

    const auto selection = targets.find(120, 210);
    REQUIRE(selection.found);
    REQUIRE(selection.source == TouchInput::ProductionCatalogTargetSource::LegacyBuilderList);
    REQUIRE(selection.target.sameRegion(target()));
    REQUIRE_FALSE(targets.find(20, 340).found);
}

TEST_CASE("Production catalogue target slots recognize the Grid independently", "[touch][production][targets]") {
    TouchInput::ProductionCatalogTargetRegistry targets;
    targets.set(TouchInput::ProductionCatalogTargetSource::Grid, gridTarget());

    const auto selection = targets.find(100, 400);
    REQUIRE(selection.found);
    REQUIRE(selection.source == TouchInput::ProductionCatalogTargetSource::Grid);
    REQUIRE(selection.target.sameRegion(gridTarget()));
    REQUIRE_FALSE(targets.find(700, 300).found);
}

TEST_CASE("Production catalogue target slots select either visible catalogue without bridging the map", "[touch][production][targets]") {
    TouchInput::ProductionCatalogTargetRegistry targets;
    targets.set(TouchInput::ProductionCatalogTargetSource::LegacyBuilderList, target());
    targets.set(TouchInput::ProductionCatalogTargetSource::Grid, gridTarget());

    REQUIRE(targets.find(120, 210).source == TouchInput::ProductionCatalogTargetSource::LegacyBuilderList);
    REQUIRE(targets.find(100, 400).source == TouchInput::ProductionCatalogTargetSource::Grid);
    REQUIRE_FALSE(targets.find(400, 250).found);
}

TEST_CASE("Production catalogue target slots clear only their own surface", "[touch][production][targets]") {
    TouchInput::ProductionCatalogTargetRegistry targets;
    targets.set(TouchInput::ProductionCatalogTargetSource::LegacyBuilderList, target());
    targets.set(TouchInput::ProductionCatalogTargetSource::Grid, gridTarget());

    REQUIRE(targets.clear(TouchInput::ProductionCatalogTargetSource::Grid));
    REQUIRE(targets.has(TouchInput::ProductionCatalogTargetSource::LegacyBuilderList));
    REQUIRE_FALSE(targets.has(TouchInput::ProductionCatalogTargetSource::Grid));
    REQUIRE(targets.clear(TouchInput::ProductionCatalogTargetSource::LegacyBuilderList, target().builderObjectID));
    REQUIRE_FALSE(targets.has(TouchInput::ProductionCatalogTargetSource::LegacyBuilderList));
}

TEST_CASE("Production catalogue session tracks the slot where the gesture began", "[touch][production][targets]") {
    TouchInput::ProductionCatalogTouchSession session;
    session.setTarget(TouchInput::ProductionCatalogTargetSource::LegacyBuilderList, target());
    session.setTarget(TouchInput::ProductionCatalogTargetSource::Grid, gridTarget());

    REQUIRE(session.begin(100, 400, 10));
    REQUIRE(session.activeSource() == TouchInput::ProductionCatalogTargetSource::Grid);
    REQUIRE(session.isTracking());

    auto movedLegacy = target();
    movedLegacy.x += 4;
    session.setTarget(TouchInput::ProductionCatalogTargetSource::LegacyBuilderList, movedLegacy);
    REQUIRE(session.isTracking());

    auto movedGrid = gridTarget();
    movedGrid.x += 4;
    session.setTarget(TouchInput::ProductionCatalogTargetSource::Grid, movedGrid);
    REQUIRE_FALSE(session.isTracking());
    REQUIRE(session.release(100, 400, 1000) == TouchInput::ProductionCatalogTouchAction::None);
}

TEST_CASE("Production catalogue Grid holds and second-finger cancellation stay inert", "[touch][production][targets]") {
    TouchInput::ProductionCatalogTouchSession session;
    session.setTarget(TouchInput::ProductionCatalogTargetSource::Grid, gridTarget());
    REQUIRE(session.begin(100, 400, 10));
    REQUIRE(session.release(100, 400, 610) == TouchInput::ProductionCatalogTouchAction::HoldSuppressed);

    REQUIRE(session.begin(100, 400, 700));
    session.cancel();
    REQUIRE(session.release(100, 400, 1300) == TouchInput::ProductionCatalogTouchAction::None);
}

TEST_CASE("Production pause control toggles independently from catalogue taps", "[touch][production]") {
    bool onHold = false;
    onHold = TouchInput::nextProductionOnHoldState(onHold);
    REQUIRE(onHold);
    onHold = TouchInput::nextProductionOnHoldState(onHold);
    REQUIRE_FALSE(onHold);

    REQUIRE_FALSE(TouchInput::shouldUseLegacyMouseResume(true, true, true));
    REQUIRE(TouchInput::shouldUseLegacyMouseResume(false, true, true));
    REQUIRE_FALSE(TouchInput::shouldUseLegacyMouseResume(false, false, true));
}

TEST_CASE("Production controls: ordinary active queue exposes all three actions", "[production][ui]") {
    const auto controls = TouchInput::productionControlVisibility(true, false, false, false);
    REQUIRE(controls.pause);
    REQUIRE(controls.cancel);
    REQUIRE(controls.cancelAll);
}

TEST_CASE("Production controls: empty queue hides production actions", "[production][ui]") {
    const auto controls = TouchInput::productionControlVisibility(false, false, false, false);
    REQUIRE_FALSE(controls.pause);
    REQUIRE_FALSE(controls.cancel);
    REQUIRE_FALSE(controls.cancelAll);
}

TEST_CASE("Production cancel button sends the active stable ID exactly once", "[production][ui]") {
    RecordingBuilder builder;
    REQUIRE(TouchInput::requestCancelCurrentProduction(builder));
    REQUIRE(builder.cancelCurrentCalls == 1);
    REQUIRE(builder.cancelledQueueEntryID == builder.getCurrentQueueEntryId());
    REQUIRE(builder.cancelAllCalls == 0);

    builder.currentQueueEntryID = 0;
    REQUIRE_FALSE(TouchInput::requestCancelCurrentProduction(builder));
    REQUIRE(builder.cancelCurrentCalls == 1);
}

TEST_CASE("Production cancel-all button emits one atomic request", "[production][ui]") {
    RecordingBuilder builder;
    TouchInput::requestCancelAllProduction(builder);
    REQUIRE(builder.cancelAllCalls == 1);
    REQUIRE(builder.cancelCurrentCalls == 0);
}

TEST_CASE("Production controls: completed building preserves cancellation but hides pause", "[production][ui]") {
    const auto controls = TouchInput::productionControlVisibility(true, true, false, false);
    REQUIRE_FALSE(controls.pause);
    REQUIRE(controls.cancel);
    REQUIRE(controls.cancelAll);
}

TEST_CASE("Production controls: Starport cancellation follows departure state", "[production][ui]") {
    const auto beforeDeparture = TouchInput::productionControlVisibility(true, false, true, true);
    REQUIRE_FALSE(beforeDeparture.pause);
    REQUIRE(beforeDeparture.cancel);
    REQUIRE(beforeDeparture.cancelAll);

    const auto afterDeparture = TouchInput::productionControlVisibility(true, false, true, false);
    REQUIRE_FALSE(afterDeparture.pause);
    REQUIRE_FALSE(afterDeparture.cancel);
    REQUIRE_FALSE(afterDeparture.cancelAll);
}
