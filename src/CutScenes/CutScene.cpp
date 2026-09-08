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

#include <CutScenes/CutScene.h>

#include <FileClasses/Palfile.h>
#include <FileClasses/FileManager.h>
#include <FileClasses/FontManager.h>
#include <FileClasses/music/MusicPlayer.h>
#include <CutScenes/TouchSkipButton.h>
#include <misc/DrawingRectHelper.h>
#include <misc/SDL2pp.h>
#include <misc/TouchInput.h>
#include <misc/draw_util.h>

#include <globals.h>
#include <sand.h>

#include <algorithm>

CutScene::CutScene()
{
    quiting = false;
}

CutScene::~CutScene()
{
    // Fixes some flickering
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
}

void CutScene::run()
{
    SDL_Event event;

    while (!quiting)
    {
        const int frameStart = SDL_GetTicks();

        const int nextFrameTime = draw();

        while(TouchInput::pollEvent(&event)) {

            //check the events
            switch (event.type)
            {
                case (SDL_KEYDOWN): // Look for a keypress
                {
                    if((event.key.keysym.sym == SDLK_SPACE) || (event.key.keysym.sym == SDLK_ESCAPE)) {
                        abortCutScene();
                    }
                } break;

                case SDL_MOUSEBUTTONDOWN:
                    if(skipButtonEnabled) {
                        CutScenes::armTouchSkipButton(event.button.button == SDL_BUTTON_LEFT,
                                                      isInsideSkipButton(event.button.x, event.button.y),
                                                      skipButtonPressed);
                    }
                    break;

                case SDL_MOUSEBUTTONUP:
                    if(skipButtonEnabled
                       && CutScenes::releaseTouchSkipButton(event.button.button == SDL_BUTTON_LEFT,
                                                            isInsideSkipButton(event.button.x, event.button.y),
                                                            skipButtonPressed)) {
                        abortCutScene();
                    }
                    break;

                case SDL_WINDOWEVENT:
                    if(event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                        skipButtonPressed = false;
                    }
                    break;
            }

            if(quiting) {
                break;
            }
        }

        const int frameTime = SDL_GetTicks() - frameStart;
        if(frameTime < nextFrameTime) {
            SDL_Delay(nextFrameTime - frameTime);
        }
    }
}

void CutScene::startNewScene() {
    scenes.push(std::make_unique<Scene>());
}

void CutScene::addVideoEvent(std::unique_ptr<VideoEvent> newVideoEvent)
{
    if(scenes.empty()) {
        scenes.push(std::make_unique<Scene>());
    }

    scenes.back()->addVideoEvent(std::move(newVideoEvent));
}

void CutScene::addTextEvent(std::unique_ptr<TextEvent> newTextEvent)
{
    if(scenes.empty()) {
        scenes.push(std::make_unique<Scene>());
    }

    scenes.back()->addTextEvent(std::move(newTextEvent));
}

void CutScene::addTrigger(std::unique_ptr<CutSceneTrigger> newTrigger)
{
    if(scenes.empty()) {
        scenes.push(std::make_unique<Scene>());
    }

    scenes.back()->addTrigger(std::move(newTrigger));
}

int CutScene::draw()
{
    int nextFrameTime = 0;

    while(scenes.empty() == false) {
        if(scenes.front()->isFinished() == true) {
            scenes.pop();
            continue;
        } else {
            if(skipButtonEnabled) {
                nextFrameTime = scenes.front()->draw([this] { drawSkipButton(); });
            } else {
                nextFrameTime = scenes.front()->draw();
            }
            break;
        }
    }

    if(scenes.empty() == true && !musicPlayer->isMusicPlaying()) {
        quit();
    }

    return nextFrameTime;
}

void CutScene::enableSkipButton(const std::string& label)
{
    pSkipButtonText = pFontManager->createTextureWithText(label, COLOR_WHITE, 14);
    SDL_SetTextureBlendMode(pSkipButtonText.get(), SDL_BLENDMODE_BLEND);
    skipButtonEnabled = true;
}

void CutScene::abortCutScene()
{
    // Fixes some flickering while leaving through the same path as ESC/SPACE.
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);
    quit();
}

SDL_Rect CutScene::getSkipButtonVisualRect() const
{
    constexpr int Margin = 18;
    constexpr int HorizontalPadding = 16;
    constexpr int VerticalPadding = 10;
    constexpr int MinimumWidth = 104;
    constexpr int MinimumHeight = 36;

    int textWidth = 0;
    int textHeight = 0;
    SDL_QueryTexture(pSkipButtonText.get(), nullptr, nullptr, &textWidth, &textHeight);

    const int rendererWidth = getRendererWidth();
    const int rendererHeight = getRendererHeight();
    const int margin = std::min(Margin, std::min(rendererWidth, rendererHeight) / 8);
    const int width = std::min(rendererWidth - 2 * margin, std::max(MinimumWidth, textWidth + 2 * HorizontalPadding));
    const int height = std::min(rendererHeight - 2 * margin, std::max(MinimumHeight, textHeight + 2 * VerticalPadding));

    return { rendererWidth - margin - width, margin, width, height };
}

SDL_Rect CutScene::getSkipButtonHitRect() const
{
    constexpr int MinimumTouchWidth = 144;
    constexpr int MinimumTouchHeight = 64;

    const SDL_Rect visualRect = getSkipButtonVisualRect();
    const int width = std::min(visualRect.x + visualRect.w, std::max(MinimumTouchWidth, visualRect.w));
    const int height = std::min(getRendererHeight() - visualRect.y, std::max(MinimumTouchHeight, visualRect.h));

    return { visualRect.x + visualRect.w - width, visualRect.y, width, height };
}

bool CutScene::isInsideSkipButton(int x, int y) const
{
    const SDL_Rect hitRect = getSkipButtonHitRect();
    return x >= hitRect.x && x < hitRect.x + hitRect.w && y >= hitRect.y && y < hitRect.y + hitRect.h;
}

void CutScene::drawSkipButton() const
{
    SDL_Rect visualRect = getSkipButtonVisualRect();
    renderFillRect(renderer, &visualRect, COLOR_RGBA(0, 0, 0, 176));
    renderDrawRect(renderer, &visualRect, COLOR_WHITE);

    int textWidth = 0;
    int textHeight = 0;
    SDL_QueryTexture(pSkipButtonText.get(), nullptr, nullptr, &textWidth, &textHeight);
    SDL_Rect textRect = {
        visualRect.x + (visualRect.w - textWidth) / 2,
        visualRect.y + (visualRect.h - textHeight) / 2,
        textWidth,
        textHeight
    };
    SDL_RenderCopy(renderer, pSkipButtonText.get(), nullptr, &textRect);
}

std::unique_ptr<Wsafile> CutScene::create_wsafile(const char* name1)
{
    return std::make_unique<Wsafile>(pFileManager->openFile(name1).get());
}

std::unique_ptr<Wsafile> CutScene::create_wsafile(const char* name1, const char* name2)
{
    return std::make_unique<Wsafile>(pFileManager->openFile(name1).get(), pFileManager->openFile(name2).get());
}

std::unique_ptr<Wsafile> CutScene::create_wsafile(const char* name1, const char* name2, const char* name3)
{
    return std::make_unique<Wsafile>(pFileManager->openFile(name1).get(), pFileManager->openFile(name2).get(), pFileManager->openFile(name3).get());
}
