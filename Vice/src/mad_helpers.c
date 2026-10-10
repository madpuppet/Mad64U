#include "vice.h"
#include "autostart.h"
#include "montypes.h"
#include "monitor.h"
#include "resources.h"
#include "mon_breakpoint.h"
#include "machine.h"
#include "maincpu.h"
#include "mem.h"  // Vice/src in your include paths

void helper_autostart_prg(const char* path)
{
    resources_set_int("AutostartPrgMode", 1);
    autostart_prg(path, AUTOSTART_MODE_RUN);
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

uint64_t helper_get_clock_cycle()
{
    return maincpu_clk;
}

