#include "dock.hpp"

#include <cmath>
#include <cstddef>

#include "app.hpp"
#include "config.hpp"
#include "context.hpp"
#include "popup.hpp"

void drawGlassDock(NVGcontext* vg, float x, float y, float w, float h,
                   float r, NVGcolor bgColor) {
    // shadow
    NVGpaint shadow{nvgBoxGradient(vg, x, y + h * 0.5f, w, h * 0.5f, r, 24.f,
                                   nvgRGBAf(0.f, 0.f, 0.f, 0.22f),
                                   nvgRGBAf(0.f, 0.f, 0.f, 0.f))};
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x - 20.f, y, w + 40.f, h + 32.f, r);
    nvgFillPaint(vg, shadow);
    nvgFill(vg);

    // fill
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, r);
    nvgFillColor(vg, bgColor);
    nvgFill(vg);

    // top half glow
    NVGpaint glow{nvgLinearGradient(vg, x, y, x, y + h,
                                    nvgRGBAf(1.f, 1.f, 1.f, 0.12f),
                                    nvgRGBAf(1.f, 1.f, 1.f, 0.f))};
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, r);
    nvgFillPaint(vg, glow);
    nvgFill(vg);

    // outer border
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x + 0.5f, y + 0.5f, w - 1.f, h - 1.f, r);
    nvgStrokeWidth(vg, 1.f);
    nvgStrokeColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.30f));
    nvgStroke(vg);

    float midX{x + w * 0.5f};

    // left half tint
    NVGpaint rimL{nvgLinearGradient(vg, x + r, y, midX, y,
                                    nvgRGBAf(1.f, 1.f, 1.f, 0.f),
                                    nvgRGBAf(1.f, 1.f, 1.f, 0.65f))};
    nvgBeginPath(vg);
    nvgMoveTo(vg, x + r, y + 1.f);
    nvgLineTo(vg, midX, y + 1.f);
    nvgStrokeWidth(vg, 1.f);
    nvgStrokePaint(vg, rimL);
    nvgStroke(vg);

    // right half tint
    NVGpaint rimR{nvgLinearGradient(vg, midX, y, x + w - r, y,
                                    nvgRGBAf(1.f, 1.f, 1.f, 0.65f),
                                    nvgRGBAf(1.f, 1.f, 1.f, 0.f))};
    nvgBeginPath(vg);
    nvgMoveTo(vg, midX, y + 1.f);
    nvgLineTo(vg, x + w - r, y + 1.f);
    nvgStrokeWidth(vg, 1.f);
    nvgStrokePaint(vg, rimR);
    nvgStroke(vg);
}

static void updateDockAnimations(std::vector<DockItem>& items, float mouseX,
                                 float mouseY, float dockStartX,
                                 float baseBottomY, float itemSpacing,
                                 float itemSize, float dt) {
    DockConfig& config{DockConfig::get()};

    float influenceRadius{itemSize * 2.3f};

    for (std::size_t i{0}; i < items.size(); i++) {
        float iconSize{itemSize * items[i].scale()};
        float iconCenter{dockStartX + i * itemSpacing + itemSize * 0.5f};

        float distance{std::abs(mouseX - iconCenter)};

        float iconBottom{baseBottomY + items[i].lift()};
        float iconTop{iconBottom - iconSize};

        bool insideY{mouseY >= iconTop && mouseY <= iconBottom};
        float influence{
            insideY ? std::max(0.f, 1.f - distance / influenceRadius) : 0.f};

        influence *= influence;

        float targetScale{1.f + influence * (config.maxScale - 1.f)};
        float targetLift{
            (config.maxScale == 1.f) ? 0.f : -influence * config.maxLiftAmount};

        items[i].scaleSpring.setTarget(targetScale);
        items[i].liftSpring.setTarget(targetLift);
        items[i].scaleSpring.update(dt);
        items[i].liftSpring.update(dt);
    }
}

void handleDock(NVGcontext* vg, IconRenderer& iconRenderer, LayerSurface& ls,
                float dt) {
    int w{ls.width};
    int h{ls.height};

    auto& config{DockConfig::get()};
    auto& mouseCtx{MouseContext::get()};
    auto& iconIndex{IconIndex::get()};
    auto& items{config.items};
    auto& popup{Popup::get()};
    int itemCount{static_cast<int>(items.size())};

    if (itemCount == 0) return;

    float baseSize{config.itemSize};
    float spacing{config.itemSpacing};

    float estimatedWidth{config.padding.horizontal() +
                         config.itemMargin.horizontal() + itemCount * baseSize +
                         (itemCount - 1) * spacing};
    float visualDockHeight{config.padding.vertical() +
                           config.itemMargin.vertical() + baseSize};

    float dockX{(w - estimatedWidth) * 0.5f};
    float dockY{h - visualDockHeight};
    float startX{dockX + config.padding.left + config.itemMargin.left};

    float hoverX{-9999.f};
    float hoverY{-9999.f};

    if (popup.isOpen()) {
        hoverX = mouseCtx.rightClickX;
        hoverY = mouseCtx.rightClickY;
    } else if (mouseCtx.inside && mouseCtx.currentSurface == ls.surface) {
        hoverX = mouseCtx.x;
        hoverY = mouseCtx.y;
    }

    float baseBottomY{dockY + config.padding.top + config.itemMargin.top +
                      baseSize};

    updateDockAnimations(items, hoverX, hoverY, startX, baseBottomY,
                         baseSize + spacing, baseSize, dt);

    float animatedWidth{config.padding.horizontal() +
                        config.itemMargin.horizontal()};

    for (int i{0}; i < itemCount; i++) {
        animatedWidth += baseSize * items[i].scale();

        if (i != itemCount - 1) {
            animatedWidth += spacing;
        }
    }

    dockX = (w - animatedWidth) * 0.5f;

    static bool prevPressed{false};
    if (mouseCtx.pressed && !prevPressed) {
        if (popup.isOpen() && mouseCtx.currentSurface != popup.wlSurface()) {
            popup.destroy();
        }

        if (mouseCtx.currentSurface == ls.surface) {
            float cx{dockX + config.padding.left + config.itemMargin.left};

            mouseCtx.lastClickIndex = -1;

            for (int i{0}; i < itemCount; i++) {
                float iconSize{baseSize * items[i].scale()};
                float iconBottom{baseBottomY + items[i].lift()};
                float iconTop{iconBottom - iconSize};
                bool insideY{mouseCtx.clickY >= iconTop &&
                             mouseCtx.clickY <= iconBottom};

                if (insideY && mouseCtx.clickX >= cx &&
                    mouseCtx.clickX <= cx + iconSize) {
                    mouseCtx.lastClickIndex = i;
                    break;
                }

                cx += iconSize + spacing;
            }

            if (mouseCtx.lastClickIndex != -1) {
                auto& clickedItem{items.at(mouseCtx.lastClickIndex)};
                clickedItem.app.launch();
            }
        }
    }
    prevPressed = mouseCtx.pressed;

    static bool prevRightPressed{false};
    if (mouseCtx.rightPressed && !prevRightPressed) {
        if (mouseCtx.currentSurface == ls.surface) {
            float cx{dockX + config.padding.left + config.itemMargin.left};
            int clickedIndex{-1};
            float clickedIconX{0.f};
            float clickedIconY{0.f};
            float clickedIconSize{0.f};

            for (int i{0}; i < itemCount; i++) {
                float iconSize{baseSize * items[i].scale()};
                float iconBottom{baseBottomY + items[i].lift()};
                float iconTop{iconBottom - iconSize};
                bool insideY{mouseCtx.rightClickY >= iconTop &&
                             mouseCtx.rightClickY <= iconBottom};

                if (insideY && mouseCtx.rightClickX >= cx &&
                    mouseCtx.rightClickX <= cx + iconSize) {
                    clickedIndex = i;
                    clickedIconX = cx;
                    clickedIconY = iconTop;
                    clickedIconSize = iconSize;
                    break;
                }

                cx += iconSize + spacing;
            }

            if (clickedIndex != -1) {
                auto& clickedItem{items.at(clickedIndex)};
                const auto& actions{clickedItem.app.actions};
                if (!actions.empty()) {
                    float menuItemHeight{std::round(config.font.size * 2.f + 10.f)};
                    float padding{std::round(config.font.size * 0.6f + 8.f)};
                    int menuHeight{static_cast<int>(
                        (actions.size() * menuItemHeight) + padding)};

                    float maxTextWidth{0.f};
                    nvgSave(vg);
                    nvgFontSize(vg, config.font.size);
                    nvgFontFace(vg, config.font.name.c_str());
                    for (const auto& action : actions) {
                        float bounds[4];
                        nvgTextBounds(vg, 0.f, 0.f, action.displayName.c_str(), nullptr, bounds);
                        float w = bounds[2] - bounds[0];
                        if (w > maxTextWidth) maxTextWidth = w;
                    }
                    nvgRestore(vg);

                    float paddingX{std::round(config.font.size * 0.8f + 20.f)};
                    int menuWidth{static_cast<int>(std::max(180.f, maxTextWidth + paddingX))};

                    popup.create(ls, clickedIndex,
                                 static_cast<int>(clickedIconX),
                                 static_cast<int>(clickedIconY),
                                 static_cast<int>(clickedIconSize),
                                 static_cast<int>(clickedIconSize), menuWidth,
                                 menuHeight, mouseCtx.rightClickSerial);
                } else {
                    if (popup.isOpen()) popup.destroy();
                }
            } else {
                if (popup.isOpen()) popup.destroy();
            }
        }
    }
    prevRightPressed = mouseCtx.rightPressed;

    drawGlassDock(vg, dockX, dockY, animatedWidth, visualDockHeight,
                  config.cornerRadius, config.backgroundColor.toNVG());

    float currentX{dockX + config.padding.left + config.itemMargin.left};
    for (int i{0}; i < itemCount; i++) {
        auto& item{items[i]};
        auto iconPath{iconIndex.find(item.app)};

        float iconSize{baseSize * item.scale()};
        float x{currentX + iconSize * 0.5f};
        float y{baseBottomY - (iconSize * 0.5f) + item.lift()};

        if (popup.isOpen() && popup.sourceAppIndex() == i) {
            float iconBottom{baseBottomY + item.lift()};
            float iconTop{iconBottom - iconSize};
            popup.reposition(
                static_cast<int>(currentX), static_cast<int>(iconTop),
                static_cast<int>(iconSize), static_cast<int>(iconSize));
        }

        item.dotSpring.update(dt);

        if (iconPath) {
            iconRenderer.draw(*iconPath, x, y, iconSize, iconSize * 0.2f);
        }

        if (item.active) {
            float dotRadius{config.activeDotSize * 0.5f};
            float dotY{baseBottomY + dotRadius + 2.f +
                       items[i].dotSpring.get()};

            nvgBeginPath(vg);
            nvgCircle(vg, x, dotY, dotRadius);
            nvgFillColor(vg, nvgRGBAf(1.f, 1.f, 1.f, 0.9f));
            nvgFill(vg);
        }

        currentX += iconSize + spacing;
    }
}
