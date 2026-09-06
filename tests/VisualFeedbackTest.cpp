/*
 * VisualFeedbackTest.cpp - deterministic transient visual feedback tests.
 */

#include <catch2/catch_all.hpp>

#include <misc/VisualFeedback.h>

TEST_CASE("Hostile feedback requires a writable touch attack on an object", "[touch][attack-feedback]") {
    REQUIRE(shouldShowAttackTargetFeedback(true, true, true, true));
    REQUIRE_FALSE(shouldShowAttackTargetFeedback(true, true, false, false)); // terrain
    REQUIRE_FALSE(shouldShowAttackTargetFeedback(true, true, true, false)); // ally
    REQUIRE_FALSE(shouldShowAttackTargetFeedback(false, true, true, true)); // desktop
    REQUIRE_FALSE(shouldShowAttackTargetFeedback(true, false, true, true)); // replay/read-only
}

TEST_CASE("Attack target feedback is time based", "[touch][attack-feedback]") {
    AttackTargetFeedback feedback;
    feedback.activate(42, 1000);

    REQUIRE(feedback.isActive(1000));
    REQUIRE(feedback.isActive(1399));
    REQUIRE_FALSE(feedback.isActive(1400));
    REQUIRE(feedback.objectID() == 42);
}

TEST_CASE("A successive attack order replaces and renews the target", "[touch][attack-feedback]") {
    AttackTargetFeedback feedback;
    feedback.activate(12, 1000);
    feedback.activate(24, 1300);

    REQUIRE(feedback.objectID() == 24);
    REQUIRE(feedback.isActive(1699));
    REQUIRE_FALSE(feedback.isActive(1700));
}

TEST_CASE("Destroyed targets can clear attack feedback immediately", "[touch][attack-feedback]") {
    AttackTargetFeedback feedback;
    feedback.activate(42, 1000);
    feedback.clear();

    REQUIRE_FALSE(feedback.isActive(1001));
    REQUIRE(feedback.objectID() == AttackTargetFeedback::INVALID_OBJECT_ID);
}

TEST_CASE("Attack target timing remains correct across tick wraparound", "[touch][attack-feedback]") {
    AttackTargetFeedback feedback;
    feedback.activate(42, AttackTargetFeedback::INVALID_OBJECT_ID - 100);

    REQUIRE(feedback.isActive(50));
    REQUIRE_FALSE(feedback.isActive(400));
}
