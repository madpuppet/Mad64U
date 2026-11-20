#pragma once

#include "WindowBase.h"
#include "ViceBridge.h"

// Renders what the VIC-II would display for a whole frame if its registers and all the memory it can see
// stayed exactly as they are right now (own renderer - no raster effects), over the full raster including blanking.
//  - the beam position (current line + cycle) is marked
//  - enabled sprites are outlined, and a panel lists all the VIC-II registers
class VicScreenWindow : public WindowBase
{
public:
    VicScreenWindow();
    ~VicScreenWindow();
    void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) override;
    bool HandleEvent(SDL_Event* e) override;
    void MessageChild(WindowLayout* layout, struct WindowMessageStruct& msg) override;
    bool Tick() override;

    // true if a newer VIC snapshot than the one being displayed is waiting, i.e. the window should be repainted
    bool NeedsRefresh();

    void SaveTokens(std::vector<std::string>& layoutTokens) override;
    static bool CreateFromLayoutTokens(struct WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx);

protected:
    void UpdateTexture(SDL_Renderer* renderer);
    void UpdateSpriteTexture(SDL_Renderer* renderer);

    SDL_Renderer* m_renderer = nullptr;
    SDL_Texture* m_texture = nullptr;

    // the 8 sprites side by side at 24x21 each (no X/Y expansion), for the sprite strip above the info panel
    SDL_Renderer* m_spriteRenderer = nullptr;
    SDL_Texture* m_spriteTexture = nullptr;
    int m_textureWidth = 0;
    int m_textureHeight = 0;
    int m_zoom = 0;
    bool m_charGrid = false;    // overlay: false = shade bad lines, true = draw the 8x8 character cell grid

    VicRasterState m_raster;

    // last rendered image area, used to map the mouse to raster line/cycle
    SDL_FRect m_imageArea{ 0,0,0,0 };
    int m_mouseX = -1;
    int m_mouseY = -1;

    // raster position under the mouse (-1 if not over the image)
    int m_hoverLine = -1;
    int m_hoverPixel = -1;

    // The single raster point, placed by clicking on the image (clicking it again removes it):
    //  - Break (left button, red): the emulator stops when the beam reaches it
    //  - Refresh (right button, green): the display refreshes when the beam reaches it, emulator keeps running
    // With no point placed the display refreshes at the top of every frame.
    // Holding the button and moving the mouse drags the point.
    bool PlaceRasterPoint(VicRasterPointMode mode, int line, int block, bool allowToggle);
    bool MouseToRaster(float mouseX, float mouseY, int& line, int& pixel);
    VicRasterPointMode m_dragMode = VicRasterPointMode::Off;    // mode of the point being dragged, Off if not dragging
    float m_viewRight = 0.0f;       // right / bottom edge of the visible image area (start of the register / info panels)
    float m_viewBottom = 0.0f;
    VicRasterPointMode m_pointMode = VicRasterPointMode::TopOfFrame;
    int m_pointLine = 0;
    int m_pointBlock = 0;       // 8 pixel block (cycle) within the line
    u32 m_snapshotVersion = 0;
    bool m_captureStarted = false;  // snapshot capture has been requested from the ViceBridge
};
