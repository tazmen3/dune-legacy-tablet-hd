/*
 * ProductionQueueControlsTest.cpp - Deterministic queue-control policies.
 */

#include <catch2/catch_all.hpp>

#include <GUI/dune/ProductionQueueControls.h>
#include <misc/ProductionControls.h>

#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <sstream>
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

class TestProductionQueueButton : public ProductionQueueButton {
public:
    Point getMinimumSize() const override { return Point(0, 0); }
};

class QueueButtonHarness : public StaticContainer {
public:
    QueueButtonHarness() {
        primary.setOnClick([this] { ++primaryActions; });
        cancel.setOnClick([this] { ++cancelActions; });
        cancelAll.setOnClick([this] { ++cancelAllActions; });
        addWidget(&primary, Point(0, 0), Point(30, 30));
        addWidget(&cancel, Point(35, 0), Point(30, 30));
        addWidget(&cancelAll, Point(70, 0), Point(30, 30));
        resize(100, 30);
    }

    void press(int x, int y = 10) { StaticContainer::handleMouseLeft(x, y, true); }
    void release(int x, int y = 10) { StaticContainer::handleMouseLeft(x, y, false); }
    void move(int x, int y = 10) { StaticContainer::handleMouseMovement(x, y, false); }
    void cancelActivePress() {
        primary.cancelPress();
        cancel.cancelPress();
        cancelAll.cancelPress();
    }

    TestProductionQueueButton primary;
    TestProductionQueueButton cancel;
    TestProductionQueueButton cancelAll;
    int primaryActions = 0;
    int cancelActions = 0;
    int cancelAllActions = 0;
};

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

TEST_CASE("Production queue buttons invoke only their own completed press", "[production][queue-controls][widget]") {
    QueueButtonHarness controls;

    controls.press(10);
    controls.release(10);
    REQUIRE(controls.primaryActions == 1);
    REQUIRE(controls.cancelActions == 0);
    REQUIRE(controls.cancelAllActions == 0);

    controls.press(45);
    controls.release(45);
    REQUIRE(controls.primaryActions == 1);
    REQUIRE(controls.cancelActions == 1);
    REQUIRE(controls.cancelAllActions == 0);

    controls.press(80);
    controls.release(80);
    REQUIRE(controls.primaryActions == 1);
    REQUIRE(controls.cancelActions == 1);
    REQUIRE(controls.cancelAllActions == 1);
}

TEST_CASE("Production queue button movement cancels the original press without residue", "[production][queue-controls][widget]") {
    QueueButtonHarness controls;

    auto requireCancelledThenCancel = [&](int releaseX) {
        controls.press(10);
        controls.move(releaseX);
        controls.release(releaseX);
        REQUIRE(controls.primaryActions == 0);
        REQUIRE(controls.cancelActions == 0);
        REQUIRE(controls.cancelAllActions == 0);

        controls.press(45);
        controls.release(45);
        REQUIRE(controls.primaryActions == 0);
        REQUIRE(controls.cancelActions == 1);
        REQUIRE(controls.cancelAllActions == 0);
        controls.cancelActions = 0;
    };

    requireCancelledThenCancel(105); // outside the panel
    requireCancelledThenCancel(45);  // another button
    requireCancelledThenCancel(32);  // spacing between buttons
    requireCancelledThenCancel(-5);  // outside the primary and panel

    controls.press(10);
    controls.release(45); // release can arrive after a pointer jump without a motion event
    REQUIRE(controls.primaryActions == 0);
    REQUIRE(controls.cancelActions == 0);

    controls.press(10);
    controls.release(32);
    REQUIRE(controls.primaryActions == 0);
    REQUIRE(controls.cancelActions == 0);
}

TEST_CASE("Production queue button state changes cancel an active press", "[production][queue-controls][widget]") {
    QueueButtonHarness controls;

    controls.press(10);
    controls.primary.setEnabled(false);
    controls.release(10);
    REQUIRE(controls.primaryActions == 0);
    controls.primary.setEnabled(true);
    controls.release(10);
    REQUIRE(controls.primaryActions == 0);

    controls.press(10);
    controls.primary.setVisible(false);
    controls.release(10);
    REQUIRE(controls.primaryActions == 0);
    controls.primary.setVisible(true);
    controls.release(10);
    REQUIRE(controls.primaryActions == 0);

    controls.press(10);
    controls.cancelActivePress(); // builder A -> builder B
    controls.release(10);
    REQUIRE(controls.primaryActions == 0);
    REQUIRE(controls.cancelActions == 0);
    REQUIRE(controls.cancelAllActions == 0);

    controls.press(10);
    controls.cancelActivePress(); // clear() or queue becoming empty
    controls.release(10);
    REQUIRE(controls.primaryActions == 0);
    controls.press(80);
    controls.release(80);
    REQUIRE(controls.cancelAllActions == 1);
}

TEST_CASE("Production queue controls reset their buttons on lifecycle cancellation", "[production][queue-controls][widget]") {
    const auto header = readTextFile(sourceRoot() / "include" / "GUI" / "dune" / "ProductionQueueControls.h");
    const auto source = readTextFile(sourceRoot() / "src" / "GUI" / "dune" / "ProductionQueueControls.cpp");

    REQUIRE(header.find("void cancelPress()") != std::string::npos);
    REQUIRE(header.find("void setVisible(bool visible) override") != std::string::npos);
    REQUIRE(source.find("void ProductionQueueControls::cancelButtonPresses()") != std::string::npos);
    REQUIRE(source.find("setBuilderObjectID") != std::string::npos);
    REQUIRE(source.find("clear()") != std::string::npos);
    REQUIRE(source.find("cancelButtonPresses();") != std::string::npos);
}
