#pragma once

#include "WindowBase.h"
#include "ViceBridge.h"

// Shows the status of each enabled disk drive: drive type, mounted image, motor / LED / head position, pending DOS
// jobs, recent sector accesses, and a map of the disk (tracks as rings, sectors as wedges) where sectors light up
// green when read and red when written, fading out over a couple of seconds.
class DriveIOWindow : public WindowBase
{
public:
    DriveIOWindow();
    ~DriveIOWindow();
    void Paint(SDL_Renderer* renderer, const Recti& dirtyArea) override;
    void MessageChild(WindowLayout* layout, struct WindowMessageStruct& msg) override;
    bool Tick() override;

    // true if a newer drive state than the one being displayed has been captured
    bool NeedsRefresh();

    void SaveTokens(std::vector<std::string>& layoutTokens) override;
    static bool CreateFromLayoutTokens(struct WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx);

protected:
    void DrawUnit(SDL_Renderer* renderer, int unit, const SDL_FRect& area, int charWidth);
    void DrawDisk(SDL_Renderer* renderer, const DriveUnitState& u, int firstTrack, int lastTrack, float cx, float cy, float radius, bool headOnThisSide);

    DriveState m_state;
    u32 m_stateVersion = 0;
    std::vector<SDL_Vertex> m_verts;
};
