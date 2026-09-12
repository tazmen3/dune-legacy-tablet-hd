/*
 * Minimal non-rendering GUI runtime for ProductionQueueButton widget tests.
 *
 * dunelegacy_tests intentionally does not link the game GUI.  These definitions
 * supply Button's non-rendering base mechanics only; the exercised click
 * implementation remains ProductionQueueButton from the production header.
 */

#include <GUI/Button.h>

std::unique_ptr<GUIStyle> GUIStyle::currentGUIStyle;

void Widget::setActive() {
    active = true;
}

void Widget::setInactive() {
    active = false;
}

Button::Button()
    : bPressed(false), bHover(false), bToggleButton(false), bToggleState(false) {
    pUnpressedTexture = nullptr;
    pPressedTexture = nullptr;
    pActiveTexture = nullptr;
}

Button::~Button() = default;

void Button::handleMouseMovement(Sint32 x, Sint32 y, bool) {
    if(x < 0 || x >= getSize().x || y < 0 || y >= getSize().y) {
        bPressed = false;
        bHover = false;
    }
}

bool Button::handleMouseLeft(Sint32, Sint32, bool) {
    return false;
}

bool Button::handleKeyPress(SDL_KeyboardEvent&) {
    return false;
}

void Button::draw(Point) {}
void Button::drawOverlay(Point) {}
void Button::invalidateTextures() {}

void Button::setSurfaces(sdl2::surface_unique_or_nonowning_ptr,
                         sdl2::surface_unique_or_nonowning_ptr,
                         sdl2::surface_unique_or_nonowning_ptr) {}

void Button::setTextures(sdl2::texture_unique_or_nonowning_ptr,
                         sdl2::texture_unique_or_nonowning_ptr,
                         sdl2::texture_unique_or_nonowning_ptr) {}
