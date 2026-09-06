/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef PRODUCTIONCONTROLS_H
#define PRODUCTIONCONTROLS_H

#include <cstdint>

namespace TouchInput {

constexpr std::uint32_t PRODUCTION_CATALOG_HOLD_MS = 600;
constexpr int PRODUCTION_MOVEMENT_TOLERANCE = 12;

enum class ProductionCatalogTouchAction {
    None,
    Tap,
    HoldSuppressed
};

constexpr bool isProductionCatalogTap(ProductionCatalogTouchAction action) {
    return action == ProductionCatalogTouchAction::Tap;
}

constexpr bool nextProductionOnHoldState(bool currentlyOnHold) {
    return !currentlyOnHold;
}

constexpr bool shouldUseLegacyMouseResume(bool touchGenerated, bool clickedCurrentItem, bool currentlyOnHold) {
    return !touchGenerated && clickedCurrentItem && currentlyOnHold;
}

struct ProductionControlVisibility {
    bool pause = false;
    bool cancel = false;
    bool cancelAll = false;
};

constexpr ProductionControlVisibility productionControlVisibility(
        bool hasQueue, bool waitingToPlace, bool isStarport, bool starportCanOrder) {
    const bool canCancel = hasQueue && (!isStarport || starportCanOrder);
    return {
        hasQueue && !waitingToPlace && !isStarport,
        canCancel,
        canCancel
    };
}

template<typename Builder>
bool requestCancelCurrentProduction(Builder& builder) {
    const auto queueEntryID = builder.getCurrentQueueEntryId();
    if(queueEntryID == 0) return false;
    builder.handleCancelQueueEntryClick(queueEntryID);
    return true;
}

template<typename Builder>
void requestCancelAllProduction(Builder& builder) {
    builder.handleCancelAllProductionClick();
}

struct ProductionCatalogTarget {
    std::uint32_t builderObjectID = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool contains(int pointX, int pointY, int tolerance = 0) const {
        return pointX >= x - tolerance && pointX < x + width + tolerance
            && pointY >= y - tolerance && pointY < y + height + tolerance;
    }

    bool sameRegion(const ProductionCatalogTarget& other) const {
        return builderObjectID == other.builderObjectID
            && x == other.x && y == other.y
            && width == other.width && height == other.height;
    }
};

/**
 * Pure guard for the production catalogue. A short touch remains a normal
 * catalogue tap; a hold is consumed so Android's historical synthetic
 * right-click cannot perform a destructive BuilderList action.
 */
class ProductionCatalogTouchGuard {
public:
    bool begin(const ProductionCatalogTarget& target, int pointX, int pointY, std::uint32_t timestamp) {
        if(target.builderObjectID == 0 || target.width <= 0 || target.height <= 0
           || !target.contains(pointX, pointY)) {
            return false;
        }

        target_ = target;
        pressedAt_ = timestamp;
        tracking_ = true;
        armed_ = true;
        return true;
    }

    void move(int pointX, int pointY) {
        if(tracking_ && armed_ && !target_.contains(pointX, pointY, PRODUCTION_MOVEMENT_TOLERANCE)) {
            armed_ = false;
        }
    }

    void cancel() {
        tracking_ = false;
        armed_ = false;
    }

    ProductionCatalogTouchAction release(int pointX, int pointY, std::uint32_t timestamp) {
        if(!tracking_) return ProductionCatalogTouchAction::None;
        move(pointX, pointY);
        const auto action = classify(timestamp);
        tracking_ = false;
        armed_ = false;
        return action;
    }

    ProductionCatalogTouchAction classify(std::uint32_t timestamp) const {
        if(!tracking_ || !armed_) return ProductionCatalogTouchAction::None;
        const auto elapsed = timestamp - pressedAt_;
        return elapsed >= PRODUCTION_CATALOG_HOLD_MS
            ? ProductionCatalogTouchAction::HoldSuppressed
            : ProductionCatalogTouchAction::Tap;
    }

    bool isTracking() const { return tracking_; }
    bool isArmed() const { return tracking_ && armed_; }
    const ProductionCatalogTarget& target() const { return target_; }

private:
    ProductionCatalogTarget target_{};
    std::uint32_t pressedAt_ = 0;
    bool tracking_ = false;
    bool armed_ = false;
};

} // namespace TouchInput

#endif // PRODUCTIONCONTROLS_H
