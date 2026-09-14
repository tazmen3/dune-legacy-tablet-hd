/*
 *  This file is part of Dune Legacy.
 *
 *  Dune Legacy is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Dune Legacy is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Dune Legacy.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <GUI/ScrollBar.h>

#include <algorithm>

ScrollBar::ScrollBar() : Widget() {
    color = COLOR_DEFAULT;
    minValue = 1;
    maxValue = 1;
    currentValue = 1;
    bigStepSize = 10;
    bDragSlider = false;
    touchPressPart = -1;

    enableResizing(false,true);

    updateArrowButtonSurface();

    arrow1.setOnClick(std::bind(&ScrollBar::onArrow1, this));
    arrow2.setOnClick(std::bind(&ScrollBar::onArrow2, this));

    pBackground = nullptr;

    resize(ScrollBar::getMinimumSize());
}

ScrollBar::~ScrollBar() = default;

bool ScrollBar::findTouchTarget(Sint32 x, Sint32 y, TouchTargetCandidate& candidate) {
    if((isEnabled() == false) || (isVisible() == false)) {
        return false;
    }

    const TouchTarget::Rect arrow1Visual{0, 0, arrow1.getSize().x, arrow1.getSize().y};
    const TouchTarget::Rect arrow2Visual{
        0, getSize().y - arrow2.getSize().y, arrow2.getSize().x, arrow2.getSize().y};
    const TouchTarget::Rect sliderVisual{
        sliderPosition.x, sliderPosition.y, sliderButton.getSize().x, sliderButton.getSize().y};

    bool found = false;
    TouchTargetCandidate best;
    const auto consider = [&](const TouchTarget::Rect& visual, std::size_t part) {
        const auto touch = TouchTarget::centeredRect(
            visual, TouchInput::getTouchTargetSize(visual.width, visual.height));
        if(!touch.contains(x, y)) {
            return;
        }

        TouchTargetCandidate current;
        current.widget = this;
        current.visual = visual;
        current.touch = touch;
        current.stablePath = { part };
        if(!found || TouchTarget::isBetterCandidate(current, best, x, y)) {
            best = std::move(current);
            found = true;
        }
    };

    consider(arrow1Visual, 0);
    consider(sliderVisual, 1);
    consider(arrow2Visual, 2);

    if(found) {
        candidate = std::move(best);
    }
    return found;
}

void ScrollBar::handleMouseMovement(Sint32 x, Sint32 y, bool insideOverlay) {
    arrow1.handleMouseMovement(x,y,insideOverlay);
    arrow2.handleMouseMovement(x,y - getSize().y + arrow2.getSize().y,insideOverlay);

    if(bDragSlider) {
        int SliderAreaHeight = getSize().y - arrow1.getSize().y - arrow2.getSize().y;
        int Range = (maxValue - minValue + 1);

        double OneTickHeight = static_cast<double>(SliderAreaHeight - sliderButton.getSize().y) / static_cast<double>(Range - 1);

        setCurrentValue(static_cast<int>((y - dragPositionFromSliderTop - arrow1.getSize().y) / OneTickHeight));
    }
}

bool ScrollBar::handleMouseLeft(Sint32 x, Sint32 y, bool pressed) {
    if(TouchInput::isTapDispatch()) {
        const auto capture = TouchInput::getTouchTarget();
        if(pressed) {
            if(capture.widget != this) {
                return false;
            }

            const TouchTarget::Rect arrow1Visual{0, 0, arrow1.getSize().x, arrow1.getSize().y};
            const TouchTarget::Rect arrow2Visual{
                0, getSize().y - arrow2.getSize().y, arrow2.getSize().x, arrow2.getSize().y};
            const TouchTarget::Rect sliderVisual{
                sliderPosition.x, sliderPosition.y, sliderButton.getSize().x, sliderButton.getSize().y};
            const auto arrow1Touch = TouchTarget::centeredRect(
                arrow1Visual, TouchInput::getTouchTargetSize(arrow1Visual.width, arrow1Visual.height));
            const auto arrow2Touch = TouchTarget::centeredRect(
                arrow2Visual, TouchInput::getTouchTargetSize(arrow2Visual.width, arrow2Visual.height));
            const auto sliderTouch = TouchTarget::centeredRect(
                sliderVisual, TouchInput::getTouchTargetSize(sliderVisual.width, sliderVisual.height));

            TouchTargetCandidate best;
            bool found = false;
            const auto consider = [&](const TouchTarget::Rect& visual,
                                      const TouchTarget::Rect& touch,
                                      int part) {
                if(!touch.contains(x, y)) {
                    return;
                }

                TouchTargetCandidate current;
                current.visual = visual;
                current.touch = touch;
                current.stablePath = { static_cast<std::size_t>(part) };
                if(!found || TouchTarget::isBetterCandidate(current, best, x, y)) {
                    best = std::move(current);
                    found = true;
                }
            };
            consider(arrow1Visual, arrow1Touch, 0);
            consider(sliderVisual, sliderTouch, 1);
            consider(arrow2Visual, arrow2Touch, 2);

            if(!found) {
                touchPressPart = -1;
                return false;
            }
            touchPressPart = static_cast<int>(best.stablePath.front());

            if(touchPressPart == 0 || touchPressPart == 2) {
                auto* arrow = touchPressPart == 0 ? &arrow1 : &arrow2;
                const auto& visual = touchPressPart == 0 ? arrow1Visual : arrow2Visual;
                TouchInput::setTouchTarget(arrow, 0, 0);
                arrow->handleMouseLeft(
                    std::max(0, std::min(x, visual.width - 1)),
                    std::max(0, std::min(y - visual.y, visual.height - 1)), true);
                TouchInput::setTouchTarget(this, 0, 0);
            } else {
                bDragSlider = true;
                dragPositionFromSliderTop = std::max(0, std::min(
                    y - sliderPosition.y, sliderButton.getSize().y - 1));
            }
            return true;
        }

        if(touchPressPart < 0) {
            return false;
        }
        if(touchPressPart == 0 || touchPressPart == 2) {
            auto* arrow = touchPressPart == 0 ? &arrow1 : &arrow2;
            const TouchTarget::Rect visual = touchPressPart == 0
                ? TouchTarget::Rect{0, 0, arrow1.getSize().x, arrow1.getSize().y}
                : TouchTarget::Rect{0, getSize().y - arrow2.getSize().y,
                                    arrow2.getSize().x, arrow2.getSize().y};
            TouchInput::setTouchTarget(arrow, 0, 0);
            arrow->handleMouseLeft(
                std::max(0, std::min(x, visual.width - 1)),
                std::max(0, std::min(y - visual.y, visual.height - 1)), false);
            TouchInput::setTouchTarget(this, 0, 0);
        }
        bDragSlider = false;
        touchPressPart = -1;
        return true;
    }

    if(pressed == false) {
        bDragSlider = false;
    }

    if(x >= 0 && x < getSize().x && y >= 0 && y < getSize().y) {
        if(arrow1.handleMouseLeft(x, y, pressed) || arrow2.handleMouseLeft(x, y - getSize().y + arrow2.getSize().y, pressed)) {
            // one of the arrow buttons clicked
            return true;
        } else {
            if(pressed) {
                if(y < sliderPosition.y) {
                    // between up arrow and slider
                    setCurrentValue(currentValue-bigStepSize);
                } else if(y > sliderPosition.y + sliderButton.getSize().y) {
                    // between slider and down button
                    setCurrentValue(currentValue+bigStepSize);
                } else {
                    // slider button
                    bDragSlider = true;
                    dragPositionFromSliderTop = y - sliderPosition.y;
                }
            }
            return true;
        }
    } else {
        return false;
    }
}

bool ScrollBar::handleMouseWheel(Sint32 x, Sint32 y, bool up)  {
    if((x >= 0) && (x < getSize().x) && (y >= 0) && (y < getSize().y)) {
        if(up == true) {
            setCurrentValue(currentValue-1);
        } else {
            setCurrentValue(currentValue+1);
        }
        return true;
    } else {
        return false;
    }
}

bool ScrollBar::handleKeyPress(SDL_KeyboardEvent& key) {
    return true;
}

void ScrollBar::draw(Point position) {
    if(isVisible() == false) {
        return;
    }

    updateTextures();

    if(pBackground != nullptr) {
        SDL_Rect dest = calcDrawingRect(pBackground.get(), position.x, position.y);
        SDL_RenderCopy(renderer, pBackground.get(), nullptr, &dest);
    }

    arrow1.draw(position);
    Point p = position;
    p.y = p.y + getSize().y - arrow2.getSize().y;
    arrow2.draw(p);
    sliderButton.draw(position+sliderPosition);
}

void ScrollBar::resize(Uint32 width, Uint32 height) {
    Widget::resize(width,height);

    invalidateTextures();

    updateSliderButton();
}

void ScrollBar::updateSliderButton() {
    double Range = static_cast<double>(maxValue - minValue + 1);
    int ArrowHeight = GUIStyle::getInstance().getMinimumScrollBarArrowButtonSize().y;
    double SliderAreaHeight = static_cast<double>(getSize().y - 2 * ArrowHeight);

    if(SliderAreaHeight < 0.0) {
        SliderAreaHeight = GUIStyle::getInstance().getMinimumScrollBarArrowButtonSize().y;
    }

    double SliderButtonHeight;
    double OneTickHeight;

    if(Range <= 1) {
        SliderButtonHeight = SliderAreaHeight;
        OneTickHeight = 0;
    } else {
        SliderButtonHeight = (SliderAreaHeight*bigStepSize) / (Range+bigStepSize);
        if(SliderButtonHeight <= 7) {
            SliderButtonHeight = 7;
        }
        OneTickHeight = (SliderAreaHeight - SliderButtonHeight)/(Range-1);
    }

    sliderButton.resize(getSize().x, lround(SliderButtonHeight));
    sliderPosition.x = 0;
    sliderPosition.y = ArrowHeight +  lround((currentValue - minValue)*OneTickHeight);
}

void ScrollBar::updateArrowButtonSurface() {
    arrow1.setSurfaces( GUIStyle::getInstance().createScrollBarArrowButton(false,false,false,color),
                        GUIStyle::getInstance().createScrollBarArrowButton(false,true,true,color),
                        GUIStyle::getInstance().createScrollBarArrowButton(false,false,true,color));

    arrow2.setSurfaces( GUIStyle::getInstance().createScrollBarArrowButton(true,false,false,color),
                        GUIStyle::getInstance().createScrollBarArrowButton(true,true,true,color),
                        GUIStyle::getInstance().createScrollBarArrowButton(true,false,true,color));
}


