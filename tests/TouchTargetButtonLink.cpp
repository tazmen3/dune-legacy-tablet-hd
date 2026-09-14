// Provide the Button target-discovery override for the lightweight test
// executable, which intentionally does not link the complete GUI library.

#include <GUI/Button.h>

bool Button::findTouchTarget(Sint32 x, Sint32 y, TouchTargetCandidate& candidate) {
    if((isEnabled() == false) || (isVisible() == false)) {
        return false;
    }

    const TouchTarget::Rect visual{ 0, 0, getSize().x, getSize().y };
    const TouchTarget::Rect touch = TouchTarget::centeredRect(
        visual,
        TouchInput::getTouchTargetSize(getSize().x, getSize().y));
    if(!touch.contains(x, y)) {
        return false;
    }

    candidate.widget = this;
    candidate.originX = 0;
    candidate.originY = 0;
    candidate.visual = visual;
    candidate.touch = touch;
    candidate.stablePath.clear();
    return true;
}
