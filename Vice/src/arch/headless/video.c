/**
 * \file video.c
 * \brief Headless UI video stuff
 *
 * \author Marco van den Heuvel <blackystardust68@yahoo.com>
 * \author Michael C. Martin <mcmartin@gmail.com>
 */

/* This file is part of VICE, the Versatile Commodore Emulator.
 * See README for copyright notice.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 *  02111-1307  USA.
 *
 */

#include "vice.h"

#include <stdio.h>

#include "cmdline.h"
#include "machine.h"
#include "resources.h"
#include "videoarch.h"
#include "video.h"
#include "interrupt.h"

#include <stddef.h>
#include <string.h>
#include "viewport.h"
#include "viciitypes.h"
#include "mem.h"


/** \brief  Command line options related to generic video output
 */
static const cmdline_option_t cmdline_options[] =
{
    CMDLINE_LIST_END
};


/** \brief  Integer/boolean resources related to video output
 */
static const resource_int_t resources_int[] =
{
    RESOURCE_INT_LIST_END
};

/** \brief  Arch-sepcific function to check which chip is
 *          currently "active", or has the focus of the user.
 *
 * Note: this version always returns VIDEO_CHIP_VICII since the headless build
 * has no concept of a canvas or even an active window, and this function only
 * makes sense in a "headful" build.
 */
int video_arch_get_active_chip(void)
{
    return VIDEO_CHIP_VICII;
}

/** \brief  Arch-specific initialization for a video canvas
 *  \param[inout] canvas The canvas being initialized
 *  \sa video_canvas_create
 */
void video_arch_canvas_init(struct video_canvas_s *canvas)
{
    /* printf("%s\n", __func__); */
}


/** \brief  Initialize command line options for generic video resouces
 *
 * \return  0 on success, < 0 on failure
 */
int video_arch_cmdline_options_init(void)
{
    /* printf("%s\n", __func__); */

    if (machine_class != VICE_MACHINE_VSID) {
        return cmdline_register_options(cmdline_options);
    }
    return 0;
}


/** \brief  Initialize video-related resources
 *
 * \return  0 on success, < on failure
 */
int video_arch_resources_init(void)
{
    /* printf("%s\n", __func__); */

    if (machine_class != VICE_MACHINE_VSID) {
        return resources_register_int(resources_int);
    }
    return 0;
}

/** \brief Clean up any memory held by arch-specific video resources. */
void video_arch_resources_shutdown(void)
{
    /* printf("%s\n", __func__); */
}

/** \brief Query whether a canvas is resizable.
 *  \param canvas The canvas to query
 *  \return TRUE if the canvas can be resized.
 */
char video_canvas_can_resize(video_canvas_t *canvas)
{
    return 1;
}

/** \brief Create a new video_canvas_s.
 *  \param[inout] canvas A freshly allocated canvas object.
 *  \param[in]    width  Pointer to a width value. May be NULL if canvas
 *                       size is not yet known.
 *  \param[in]    height Pointer to a height value. May be NULL if canvas
 *                       size is not yet known.
 *  \param        mapped Unused.
 *  \return The completely initialized canvas. The window that holds
 *          it will be visible in the UI at time of return.
 */
video_canvas_t *video_canvas_create(video_canvas_t *canvas,
                                    unsigned int *width, unsigned int *height,
                                    int mapped)
{
    /* printf("%s\n", __func__); */

    canvas->created = 1;

    return canvas;
}

/** \brief Free a previously created video canvas and all its
 *         components.
 *  \param[in] canvas The canvas to destroy.
 */
void video_canvas_destroy(struct video_canvas_s *canvas)
{
    /* printf("%s\n", __func__); */
}


int mad64_update_current_raster_line(video_canvas_t* canvas)
{
    draw_buffer_t* db;
    const geometry_t* geo;
    unsigned int row;
    unsigned int column;
    int source_offset;
    int count;
    int width;

    if (!canvas || canvas != vicii.raster.canvas)
        return 0;

    db = canvas->draw_buffer;
    geo = canvas->geometry;

    if (!db || !db->draw_buffer || !geo)
        return 0;

    source_offset = 17 * 8 - vicii.screen_leftborderwidth;

    if (source_offset < 0 || source_offset >= VICII_DRAW_BUFFER_SIZE)
        return 0;

    count = vicii.dbuf_offset - source_offset;
    if (count <= 0)
        return 0;

    width = vicii.screen_leftborderwidth + 320
        + vicii.screen_rightborderwidth;

    if (count > width)
        count = width;

    if (count > VICII_DRAW_BUFFER_SIZE - source_offset)
        count = VICII_DRAW_BUFFER_SIZE - source_offset;

    row = vicii.raster.current_line;
    column = geo->extra_offscreen_border_left;

    if (row < geo->first_displayed_line || row < geo->last_displayed_line)
        return 0;

    if (row >= db->draw_buffer_height ||
        column >= db->draw_buffer_width)
        return 0;

    if ((unsigned int)count > db->draw_buffer_width - column)
        count = (int)(db->draw_buffer_width - column);

    memcpy(
        db->draw_buffer + (size_t)row * db->draw_buffer_width + column,
        vicii.dbuf + source_offset,
        (size_t)count);

    return count;
}


/** \brief Update the display on a video canvas to reflect the machine
 *         state.
 * \param canvas The canvas to update.
 * \param xs     A parameter to forward to video_canvas_render()
 * \param ys     A parameter to forward to video_canvas_render()
 * \param xi     X coordinate of the leftmost pixel to update
 * \param yi     Y coordinate of the topmost pixel to update
 * \param w      Width of the rectangle to update
 * \param h      Height of the rectangle to update
 */
void video_canvas_refresh(struct video_canvas_s *canvas,
                          unsigned int xs, unsigned int ys,
                          unsigned int xi, unsigned int yi,
                          unsigned int w, unsigned int h)
{
    extern void mad64_update_ram(unsigned char * ram);
    extern void mad64_video_refresh(unsigned char* buffer, int width, int height, int firstLine, int lastLine);

    struct draw_buffer_s *db = canvas->draw_buffer;
    if (!db)
        return;

#if 0
    extern bool monitor_is_inside_monitor();
    if (monitor_is_inside_monitor())
    {
        extern void helper_update_vice_state();
        helper_update_vice_state();
        mad64_update_current_raster_line(canvas);
    }
    else
    {
        extern void capture_and_update_vice_state(uint16_t currentPc, void* userData);
        interrupt_maincpu_trigger_trap(capture_and_update_vice_state, 0);
    }
#endif

    

    const geometry_t* geo = canvas->geometry;
    mad64_update_ram(mem_ram);
    mad64_video_refresh(db->draw_buffer, db->draw_buffer_width, db->draw_buffer_height, geo->first_displayed_line, geo->last_displayed_line);
}

/** \brief Update canvas size to match the draw buffer size requested
 *         by the emulation core.
 * \param canvas The video canvas to update.
 * \param resize_canvas Ignored - the canvas will always resize.
 */

void video_canvas_resize(struct video_canvas_s *canvas, char resize_canvas)
{
    /* printf("%s\n", __func__); */
}

/** \brief Assign a palette to the canvas.
 * \param canvas The canvas to update the palette
 * \param palette The new palette to assign
 * \return Zero on success, nonzero on failure
 */
int video_canvas_set_palette(struct video_canvas_s *canvas,
                             struct palette_s *palette)
{
    /* printf("%s\n", __func__); */

    canvas->palette = palette;

    return 0;
}

/** \brief Perform any frontend-specific initialization.
 *  \return 0 on success, nonzero on failure
 */
int video_init(void)
{
    /* printf("%s\n", __func__); */

    return 0;
}

/** \brief Perform any frontend-specific uninitialization. */
void video_shutdown(void)
{
    /* printf("%s\n", __func__); */
}
