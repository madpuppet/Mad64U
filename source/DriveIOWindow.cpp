#include "common.h"
#include "DriveIOWindow.h"
#include "WindowLayout.h"
#include "ViceBridge.h"
#include "WindowManager.h"
#include "Application.h"

#define DRIVE_ROW_HEIGHT 18
#define DRIVE_TEXT_LINES 6
#define DRIVE_RING_SEGMENTS 120     // arc segments around a full track

// VICE disk image types (diskimage.h)
#define IMAGE_TYPE_G71 101
#define IMAGE_TYPE_D71 1571
#define IMAGE_TYPE_D81 1581
#define IMAGE_TYPE_D80 8050
#define IMAGE_TYPE_D82 8250
#define IMAGE_TYPE_D1M 1000
#define IMAGE_TYPE_D2M 2000
#define IMAGE_TYPE_D4M 4000

// VICE drive types (drive.h)
#define DRIVE_TYPE_1571 1571
#define DRIVE_TYPE_1571CR 1573
#define DRIVE_TYPE_1581 1581

static const SDL_Color s_labelCol{ 160, 160, 160, 255 };
static const SDL_Color s_valueCol{ 255, 255, 255, 255 };
static const SDL_Color s_offCol{ 80, 80, 80, 255 };
static const SDL_Color s_readCol{ 60, 230, 90, 255 };
static const SDL_Color s_writeCol{ 255, 70, 50, 255 };
static const SDL_Color s_verifyCol{ 240, 210, 60, 255 };
static const SDL_Color s_headCol{ 70, 150, 200, 255 };
static const SDL_Color s_unitCol{ 110, 200, 255, 255 };

static bool IsDoubleSided(const mad_drive_info_t& info)
{
    if (info.hasImage)
        return info.imageType == IMAGE_TYPE_D71 || info.imageType == IMAGE_TYPE_G71;
    return false;
}

static bool Is3_5Inch(const mad_drive_info_t& info)
{
    if (info.hasImage)
        return info.imageType == IMAGE_TYPE_D81 || info.imageType == IMAGE_TYPE_D1M || info.imageType == IMAGE_TYPE_D2M || info.imageType == IMAGE_TYPE_D4M;
    return info.type == DRIVE_TYPE_1581;
}

static int TrackCount(const mad_drive_info_t& info)
{
    int tracks = 35;
    if (info.hasImage && info.imageTracks > 0)
        tracks = info.imageTracks;
    else if (Is3_5Inch(info))
        tracks = 80;
    return std::clamp(tracks, 1, DRIVE_MAP_TRACKS);
}

static int SectorsPerTrack(const mad_drive_info_t& info, int track)
{
    if (Is3_5Inch(info))
        return 40;
    if (info.hasImage && (info.imageType == IMAGE_TYPE_D80 || info.imageType == IMAGE_TYPE_D82))
    {
        int t = track > 77 ? track - 77 : track;
        return t <= 39 ? 29 : t <= 53 ? 27 : t <= 64 ? 25 : 23;
    }
    if (IsDoubleSided(info) && track > 35)
        track -= 35;
    return track <= 17 ? 21 : track <= 24 ? 19 : track <= 30 ? 18 : 17;
}

static int DirectoryTrack(const mad_drive_info_t& info)
{
    if (Is3_5Inch(info))
        return 40;
    if (info.hasImage && (info.imageType == IMAGE_TYPE_D80 || info.imageType == IMAGE_TYPE_D82))
        return 39;
    return 18;
}

// 1 when just accessed, fading to 0 over DRIVE_FADE_FRAMES
static float Fade(u32 now, u32 last)
{
    if (last == 0 || now < last)
        return 0.0f;
    u32 age = now - last;
    if (age >= DRIVE_FADE_FRAMES)
        return 0.0f;
    return 1.0f - (float)age / DRIVE_FADE_FRAMES;
}

static SDL_FColor Lerp(const SDL_FColor& a, const SDL_Color& b, float t)
{
    return SDL_FColor{ a.r + (b.r / 255.0f - a.r) * t, a.g + (b.g / 255.0f - a.g) * t, a.b + (b.b / 255.0f - a.b) * t, 1.0f };
}

static const char* JobName(u8 code)
{
    switch (code & 0xf0)
    {
        case 0x80: return "READ";
        case 0x90: return "WRITE";
        case 0xa0: return "VERIFY";
        case 0xb0: return "SEEK";
        case 0xc0: return "BUMP";
        case 0xd0: return "JUMP";
        case 0xe0: return "EXEC";
        default: return "?";
    }
}

static std::string FileName(const char* path)
{
    std::string p = path;
    size_t slash = p.find_last_of("/\\");
    return slash == std::string::npos ? p : p.substr(slash + 1);
}

DriveIOWindow::DriveIOWindow()
{
    m_name = "DriveIO";
}

DriveIOWindow::~DriveIOWindow()
{
}

void DriveIOWindow::MessageChild(WindowLayout* layout, struct WindowMessageStruct& msg)
{
    switch (msg.m_type)
    {
        case WindowMessage::Query_Highlight:
        {
            auto query = (WindowHighlightQuery*)msg.m_query;
            if (m_clientArea.Contains(msg.m_x, msg.m_y))
            {
                msg.m_response++;
                query->m_area = m_clientArea;
                query->m_highlight = WindowHighlightType::ClientArea;
                query->m_tree = msg.m_tree;
                query->m_layout = layout;
                query->m_window = this;
                return;
            }
        }
        break;
    }
}

bool DriveIOWindow::Tick()
{
    return NeedsRefresh();
}

bool DriveIOWindow::NeedsRefresh()
{
    return ViceBridge::Exists() && ViceBridge::Instance().GetDriveStateVersion() != m_stateVersion;
}

//==================================================================================================================================
// Disk map: track 1 is the outer ring, sectors run clockwise from the top. Each sector is shaded by how recently it passed
// under the head (blue), was read (green) or written (red).
//==================================================================================================================================
void DriveIOWindow::DrawDisk(SDL_Renderer* renderer, const DriveUnitState& u, int firstTrack, int lastTrack, float cx, float cy, float radius, bool headOnThisSide)
{
    const auto& info = u.m_info;
    const u32 now = m_state.m_frame;
    const float pi2 = 6.28318530718f;
    int tracks = lastTrack - firstTrack + 1;
    if (radius < 8.0f || tracks <= 0)
        return;

    float hub = radius * 0.28f;
    float ringW = (radius - hub) / tracks;
    int dirTrack = DirectoryTrack(info);

    // disk surface
    {
        SDL_FRect bg{ cx - radius - 2, cy - radius - 2, radius * 2 + 4, radius * 2 + 4 };
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(renderer, &bg);
    }

    m_verts.clear();
    auto addQuad = [&](float r0, float r1, float a0, float a1, const SDL_FColor& col)
        {
            float s0 = sinf(a0), c0 = -cosf(a0), s1 = sinf(a1), c1 = -cosf(a1);
            SDL_Vertex v[4] = {
                { { cx + s0 * r1, cy + c0 * r1 }, col, { 0, 0 } },
                { { cx + s1 * r1, cy + c1 * r1 }, col, { 0, 0 } },
                { { cx + s1 * r0, cy + c1 * r0 }, col, { 0, 0 } },
                { { cx + s0 * r0, cy + c0 * r0 }, col, { 0, 0 } },
            };
            m_verts.push_back(v[0]); m_verts.push_back(v[1]); m_verts.push_back(v[2]);
            m_verts.push_back(v[0]); m_verts.push_back(v[2]); m_verts.push_back(v[3]);
        };

    for (int t = firstTrack; t <= lastTrack; t++)
    {
        int ring = t - firstTrack;
        float r1 = radius - ring * ringW;
        float r0 = r1 - ringW + (ringW >= 4.0f ? 1.0f : 0.0f);
        int nsec = (std::min)(SectorsPerTrack(info, t), DRIVE_MAP_SECTORS);
        int steps = (std::max)(1, DRIVE_RING_SEGMENTS / nsec);
        float secAngle = pi2 / nsec;
        float gap = (r1 * secAngle >= 8.0f) ? 0.6f / r1 : 0.0f;    // ~1 pixel between sectors when there is room

        for (int s = 0; s < nsec; s++)
        {
            float base = ((t + s) & 1) ? 0.16f : 0.12f;
            SDL_FColor col{ base, base, base, 1.0f };
            if (t == dirTrack)
                col = SDL_FColor{ base, base, base + 0.12f, 1.0f };

            col = Lerp(col, s_headCol, Fade(now, u.m_lastHead[t][s]) * 0.6f);
            col = Lerp(col, s_readCol, Fade(now, u.m_lastRead[t][s]));
            col = Lerp(col, s_writeCol, Fade(now, u.m_lastWrite[t][s]));

            float a0 = s * secAngle + gap;
            float a1 = (s + 1) * secAngle - gap;
            for (int k = 0; k < steps; k++)
                addQuad(r0, r1, a0 + (a1 - a0) * k / steps, a0 + (a1 - a0) * (k + 1) / steps, col);
        }
    }
    SDL_RenderGeometry(renderer, nullptr, m_verts.data(), (int)m_verts.size(), nullptr, 0);

    // hub
    {
        SDL_FRect hubRect{ cx - hub * 0.4f, cy - hub * 0.4f, hub * 0.8f, hub * 0.8f };
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
        SDL_RenderFillRect(renderer, &hubRect);
    }

    // head: an arm from outside the disk to the current track, at the rotational position of the head
    if (headOnThisSide && info.trueEmulation && info.halfTrack > 0)
    {
        float trackPos = (info.halfTrack / 2.0f) + ((IsDoubleSided(info) && info.side) ? 35.0f : 0.0f) - firstTrack;
        float rHead = radius - (trackPos + 0.5f) * ringW;
        rHead = std::clamp(rHead, hub, radius);

        float angle = info.trackSize > 0 ? pi2 * info.headOffset / info.trackSize : 0.0f;
        float sx = sinf(angle), cy2 = -cosf(angle);
        SDL_Color armCol = info.motorOn ? (info.writeMode ? s_writeCol : s_valueCol) : s_labelCol;
        SDL_SetRenderDrawColor(renderer, armCol.r, armCol.g, armCol.b, 255);
        SDL_RenderLine(renderer, cx + sx * (radius + 6), cy + cy2 * (radius + 6), cx + sx * rHead, cy + cy2 * rHead);
        SDL_FRect headBox{ cx + sx * rHead - 2.5f, cy + cy2 * rHead - 2.5f, 5.0f, 5.0f };
        SDL_RenderFillRect(renderer, &headBox);
    }
}

void DriveIOWindow::DrawUnit(SDL_Renderer* renderer, int unit, const SDL_FRect& area, int charWidth)
{
    auto& fr = FontRenderer::Instance();
    const auto& u = m_state.m_units[unit];
    const auto& info = u.m_info;
    const u32 now = m_state.m_frame;

    auto text = [&](int x, int y, const std::string& str, const SDL_Color& col) -> int
        {
            fr.RenderText(renderer, str, col, x, y, FontType::Text);
            return x + ((int)str.size() + 1) * charWidth;
        };

    // disk map(s) on the left, as large as the section allows
    bool twoSides = IsDoubleSided(info);
    float diskSize = (std::max)(0.0f, (std::min)(area.h - 12.0f, (area.w - 40.0f * charWidth) / (twoSides ? 2.0f : 1.0f)));
    diskSize = (std::max)(diskSize, (std::min)(area.h - 12.0f, 96.0f));
    float radius = diskSize * 0.5f;
    float diskY = area.y + 6.0f + radius;
    float x = area.x + 10.0f;
    int tracks = TrackCount(info);
    if (twoSides)
    {
        int half = tracks / 2;
        bool side1 = info.side != 0;
        DrawDisk(renderer, u, 1, half, x + radius, diskY, radius, !side1);
        x += diskSize + 10.0f;
        DrawDisk(renderer, u, half + 1, tracks, x + radius, diskY, radius, side1);
    }
    else
    {
        DrawDisk(renderer, u, 1, tracks, x + radius, diskY, radius, true);
    }
    x += diskSize + 16.0f;

    // text panel
    int tx = (int)x;
    int y = (int)area.y + 6;
    int labelW = 8 * charWidth;

    // line 1: unit, drive type, LED / motor and what it is doing
    int lx = text(tx, y, std::format("DRIVE {}", unit + 8), s_unitCol);
    lx = text(lx, y, info.typeName, s_valueCol);
    lx = text(lx, y, info.trueEmulation ? "TRUE DRIVE" : "VIRTUAL", s_labelCol);
    {
        SDL_FRect led{ (float)lx, (float)y + 4, 10.0f, (float)DRIVE_ROW_HEIGHT - 9 };
        if (info.ledOn)
            SDL_SetRenderDrawColor(renderer, 255, 40, 40, 255);
        else
            SDL_SetRenderDrawColor(renderer, 60, 20, 20, 255);
        SDL_RenderFillRect(renderer, &led);
        lx += 2 * charWidth;
    }
    lx = text(lx, y, "MOTOR", info.motorOn ? s_valueCol : s_offCol);

    bool recentRead = false, recentWrite = false;
    if (u.m_eventCount > 0)
    {
        auto& ev = u.m_events[(u.m_eventCount - 1) % DRIVE_EVENT_COUNT];
        if (now - ev.m_frame < 10)
        {
            recentWrite = ev.m_kind == DriveAccess::Write;
            recentRead = !recentWrite;
        }
    }
    if ((info.motorOn && info.writeMode) || recentWrite)
        text(lx + charWidth, y, "WRITING", s_writeCol);
    else if (info.motorOn || recentRead)
        text(lx + charWidth, y, "READING", s_readCol);
    else
        text(lx + charWidth, y, "IDLE", s_offCol);
    y += DRIVE_ROW_HEIGHT;

    // line 2: mounted image
    text(tx, y, "Disk", s_labelCol);
    if (info.hasImage)
    {
        std::string name = info.imageName[0] ? FileName(info.imageName) : std::string("(unnamed)");
        lx = text(tx + labelW, y, name, s_valueCol);
        if (info.readOnly)
            text(lx, y, "[READ ONLY]", s_verifyCol);
    }
    else
    {
        text(tx + labelW, y, "No disk", s_offCol);
    }
    y += DRIVE_ROW_HEIGHT;

    // line 3: head position
    text(tx, y, "Head", s_labelCol);
    if (info.trueEmulation && info.halfTrack > 0)
    {
        int track = info.halfTrack / 2 + ((twoSides && info.side) ? 35 : 0);
        std::string trackStr = (info.halfTrack & 1) ? std::format("Track {}.5", track) : std::format("Track {}", track);
        lx = text(tx + labelW, y, trackStr, s_valueCol);
        if (info.headerSector >= 0)
            lx = text(lx, y, std::format("Sector {:02}", info.headerSector), info.motorOn ? s_valueCol : s_labelCol);
        else
            lx = text(lx, y, "Sector --", s_offCol);
        if (twoSides)
            text(lx, y, std::format("Side {}", info.side), s_labelCol);
    }
    else
    {
        text(tx + labelW, y, info.trueEmulation ? "--" : "(no mechanics emulated)", s_offCol);
    }
    y += DRIVE_ROW_HEIGHT;

    // line 4: pending DOS jobs
    text(tx, y, "Jobs", s_labelCol);
    lx = tx + labelW;
    int pending = 0;
    for (int j = 0; j < info.jobCount; j++)
    {
        u8 code = info.jobCode[j];
        if (!(code & 0x80))
            continue;
        SDL_Color col = (code & 0xf0) == 0x90 ? s_writeCol : (code & 0xf0) == 0x80 ? s_readCol : s_verifyCol;
        if ((code & 0xf0) >= 0xb0)
            lx = text(lx, y, std::format("{}:{} {}", j, JobName(code), info.jobTrack[j]), col);
        else
            lx = text(lx, y, std::format("{}:{} {}/{:02}", j, JobName(code), info.jobTrack[j], info.jobSector[j]), col);
        pending++;
    }
    if (pending == 0)
        text(lx, y, info.jobCount ? "none" : "--", s_offCol);
    y += DRIVE_ROW_HEIGHT;

    // lines 5+: recent accesses, newest first, fading with age
    text(tx, y, "Recent", s_labelCol);
    lx = tx + labelW;
    int lineEnd = (int)(area.x + area.w) - 4;
    u32 count = (std::min)(u.m_eventCount, (u32)DRIVE_EVENT_COUNT);
    if (count == 0)
        text(lx, y, "none", s_offCol);
    for (u32 i = 0; i < count; i++)
    {
        auto& ev = u.m_events[(u.m_eventCount - 1 - i) % DRIVE_EVENT_COUNT];
        const char* k = ev.m_kind == DriveAccess::Write ? "W" : ev.m_kind == DriveAccess::Verify ? "V" : "R";
        std::string str = std::format("{}{}/{:02}", k, ev.m_track, ev.m_sector);
        if (lx + (int)str.size() * charWidth > lineEnd)
        {
            y += DRIVE_ROW_HEIGHT;
            if (y + DRIVE_ROW_HEIGHT > area.y + area.h)
                break;
            lx = tx + labelW;
        }
        SDL_Color base = ev.m_kind == DriveAccess::Write ? s_writeCol : ev.m_kind == DriveAccess::Verify ? s_verifyCol : s_readCol;
        float f = 0.35f + 0.65f * Fade(now, ev.m_frame);
        SDL_Color col{ (u8)(base.r * f), (u8)(base.g * f), (u8)(base.b * f), 255 };
        lx = text(lx, y, str, col);
    }
}

void DriveIOWindow::Paint(SDL_Renderer* renderer, const Recti& dirtyArea)
{
    auto& vb = ViceBridge::Instance();
    vb.GetDriveState(m_state, m_stateVersion);

    auto& wm = WindowManager::Instance();
    auto& tp = Application::Instance().GetThemeProperties();
    auto& fr = FontRenderer::Instance();
    int charWidth = fr.GetCharactorWidth(FontType::Text, '0');

    // background
    if (wm.GetActiveWindowBase() == this)
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackgroundSelected);
    else
        tp.SetRenderDrawColor(renderer, ThemeColor::SourceBackground);
    SDL_FRect body = m_clientArea.AsSDLFRect();
    SDL_RenderFillRect(renderer, &body);

    SDL_Rect oldClip{ 0,0,0,0 };
    bool hadClip = SDL_RenderClipEnabled(renderer);
    SDL_GetRenderClipRect(renderer, &oldClip);
    auto setClip = [&](const SDL_FRect& area)
        {
            SDL_Rect clip{ (int)area.x, (int)area.y, (int)area.w, (int)area.h };
            if (hadClip)
            {
                SDL_Rect wanted = clip;
                if (!SDL_GetRectIntersection(&oldClip, &wanted, &clip))
                    clip = SDL_Rect{ 0, 0, 0, 0 };
            }
            SDL_SetRenderClipRect(renderer, &clip);
        };

    // units with a drive (or at least an image) attached
    int units[MAD_DRIVE_UNITS];
    int unitCount = 0;
    for (int i = 0; i < MAD_DRIVE_UNITS; i++)
    {
        const auto& info = m_state.m_units[i].m_info;
        if (info.type != 0 || info.hasImage)
            units[unitCount++] = i;
    }

    if (unitCount == 0)
    {
        setClip(body);
        fr.RenderText(renderer, "No disk drives enabled", s_labelCol, (int)body.x + 8, (int)body.y + 6, FontType::Text);
    }
    else
    {
        float minHeight = (float)(DRIVE_TEXT_LINES * DRIVE_ROW_HEIGHT + 12);
        float unitHeight = (std::max)(body.h / unitCount, minHeight);
        for (int i = 0; i < unitCount; i++)
        {
            SDL_FRect area{ body.x, body.y + i * unitHeight, body.w, unitHeight };
            SDL_FRect visible = area;
            if (visible.y + visible.h > body.y + body.h)
                visible.h = body.y + body.h - visible.y;
            if (visible.h <= 0.0f)
                break;

            setClip(visible);
            DrawUnit(renderer, units[i], area, charWidth);
            if (i > 0)
            {
                SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
                SDL_RenderLine(renderer, area.x, area.y, area.x + area.w, area.y);
            }
        }
    }

    SDL_SetRenderClipRect(renderer, hadClip ? &oldClip : nullptr);
}

void DriveIOWindow::SaveTokens(std::vector<std::string>& layoutTokens)
{
    layoutTokens.push_back("DRIVEIOWINDOW");
}

bool DriveIOWindow::CreateFromLayoutTokens(WindowLayout* layout, const std::vector<std::string>& layoutTokens, size_t& idx)
{
    if (layoutTokens[idx] != "DRIVEIOWINDOW")
        return false;

    idx++;
    layout->m_tabs.push_back(new DriveIOWindow());
    return true;
}
