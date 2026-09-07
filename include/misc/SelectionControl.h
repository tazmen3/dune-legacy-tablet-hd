/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 */

#ifndef SELECTIONCONTROL_H
#define SELECTIONCONTROL_H

#include <cstddef>
#include <functional>
#include <set>

namespace SelectionControl {

inline bool isGlobalDeselectVisible(std::size_t selectionSize) noexcept {
    return selectionSize != 0;
}

/// Clears a selection as one atomic UI action and emits one selection-change notification.
template<typename ObjectID, typename ClearObject, typename ResetState, typename SelectionChanged>
bool clearSelection(std::set<ObjectID>& selectedObjects, ClearObject&& clearObject,
                    ResetState&& resetState, SelectionChanged&& selectionChanged) {
    if(selectedObjects.empty()) {
        return false;
    }

    for(const auto objectID : selectedObjects) {
        std::invoke(clearObject, objectID);
    }
    selectedObjects.clear();
    std::invoke(resetState);
    std::invoke(selectionChanged);
    return true;
}

} // namespace SelectionControl

#endif // SELECTIONCONTROL_H
