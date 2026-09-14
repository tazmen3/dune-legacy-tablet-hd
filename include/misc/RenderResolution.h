/*
 *  This file is part of Dune Legacy.
 *
 *  Small, platform-neutral geometry helpers for the logical-to-physical
 *  renderer boundary.  Game code remains expressed in logical coordinates.
 */

#ifndef DUNELEGACY_RENDERRESOLUTION_H
#define DUNELEGACY_RENDERRESOLUTION_H

#include <SDL.h>

#include <algorithm>

struct RenderResolution {
    int logicalWidth = 0;
    int logicalHeight = 0;
    int physicalWidth = 0;
    int physicalHeight = 0;
    int renderScale = 1;

    int framebufferWidth() const noexcept {
        return logicalWidth * renderScale;
    }

    int framebufferHeight() const noexcept {
        return logicalHeight * renderScale;
    }

    bool framebufferFitsPhysical() const noexcept {
        return logicalWidth > 0 && logicalHeight > 0
            && physicalWidth > 0 && physicalHeight > 0
            && framebufferWidth() <= physicalWidth
            && framebufferHeight() <= physicalHeight;
    }

    bool usesHdFramebuffer() const noexcept {
        return renderScale > 1 && framebufferFitsPhysical();
    }

    SDL_Rect presentationViewport() const noexcept {
        const int width = framebufferWidth();
        const int height = framebufferHeight();
        return SDL_Rect{
            (physicalWidth - width) / 2,
            (physicalHeight - height) / 2,
            width,
            height
        };
    }

    SDL_Rect logicalRectToRenderTargetRect(const SDL_Rect& logicalRect) const noexcept {
        return SDL_Rect{
            logicalRect.x * renderScale,
            logicalRect.y * renderScale,
            logicalRect.w * renderScale,
            logicalRect.h * renderScale
        };
    }

    static RenderResolution create(int logicalWidth, int logicalHeight,
                                   int physicalWidth, int physicalHeight,
                                   int requestedScale) noexcept {
        RenderResolution result;
        result.logicalWidth = logicalWidth;
        result.logicalHeight = logicalHeight;
        result.physicalWidth = physicalWidth;
        result.physicalHeight = physicalHeight;

        const int safeRequestedScale = std::max(1, requestedScale);
        const int widthScale = logicalWidth > 0 ? physicalWidth / logicalWidth : 1;
        const int heightScale = logicalHeight > 0 ? physicalHeight / logicalHeight : 1;
        const int fittingScale = std::min(widthScale, heightScale);
        result.renderScale = std::max(1, std::min(safeRequestedScale, fittingScale));
        return result;
    }
};

#endif // DUNELEGACY_RENDERRESOLUTION_H
