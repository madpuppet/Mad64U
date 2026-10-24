#include "vice.h"
#include "autostart.h"
#include "resources.h"
#include "machine.h"
#include "maincpu.h"
#include "mem.h"  // Vice/src in your include paths
#include "sound.h"
#include "keyboard.h"
#include "drive.h"
#include "sysFile.h"
#include "cartridge.h"
#include "attach.h"
#include "vsync.h"

void helper_autostart_crt(const char* path)
{
    if (cartridge_attach_image(CARTRIDGE_CRT, path) < 0)
        return;

    machine_trigger_reset(MACHINE_RESET_MODE_POWER_CYCLE);
}

void helper_autostart_prg(const char* path)
{
    resources_set_int("AutostartPrgMode", 1);
    autostart_prg(path, AUTOSTART_MODE_RUN);
}

void helper_mount_d64(const char* path)
{
    resources_set_int("Drive8Type", DRIVE_TYPE_1541);
    file_system_attach_disk(8, 0, path);
}

void helper_set_warp_mode(bool enable)
{
    vsync_set_warp_mode(enable);
}

int helper_autostart_d64(const char* path)
{
    resources_set_int("Drive8Type", DRIVE_TYPE_1541);
    return autostart_disk(
        8,                   // Device number
        0,                   // Drive within device
        path,                // D64 filename
        NULL,                // Program name: NULL defaults to "*"
        0,                   // No specific directory entry
        AUTOSTART_MODE_RUN);
}

void helper_get_raster_pos(unsigned int *rasterLine, unsigned int *rasterCycle)
{
    int halfCycle;
    machine_get_line_cycle(rasterLine, rasterCycle, &halfCycle);
}

void helper_set_video_mode(int pal)
{
    resources_set_int("MachineVideoStandard", pal ? MACHINE_SYNC_PAL : MACHINE_SYNC_NTSC);
    resources_set_int("MachinePowerFrequency", pal ? 50 : 60);
}

void helper_set_volume(int volume)
{
    resources_set_int("SoundVolume", volume);
}

uint64_t helper_get_clock_cycle()
{
    return maincpu_clk;
}

void helper_suspend_sound()
{
    sound_suspend();
}

void helper_resume_sound()
{
    sound_resume();
}

void helper_keyboard_set(int row, int column, int value)
{
    keyboard_set_keyarr(row, column, value);
}


