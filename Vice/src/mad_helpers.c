#include "vice.h"
#include "autostart.h"
#include "montypes.h"
#include "monitor.h"
#include "resources.h"
#include "mon_breakpoint.h"
#include "machine.h"
#include "maincpu.h"

void helper_autostart_prg(const char* path)
{
    resources_set_int("AutostartPrgMode", 1);
    autostart_prg(path, AUTOSTART_MODE_RUN);
}

int helper_set_breakpoint(int memaddr)
{
    MON_ADDR addr = new_addr(e_comp_space, (uint16_t)memaddr);

    int id = mon_breakpoint_add_checkpoint(addr, addr,
        true,       // Stop execution when hit
        e_exec,     // Instruction execution breakpoint
        false,      // Persistent, not temporary
        false);     // Don't print checkpoint information
    return id;
}

void helper_update_vice_state()
{
    struct monitor_cpu_type_s *cpu = monitor_cpu_for_memspace[e_comp_space];
    if (cpu)
    {
        int pc, acc, x, y, flags;
        pc = (int)cpu->mon_register_get_val(e_comp_space, e_PC);
        acc = (int)cpu->mon_register_get_val(e_comp_space, e_A);
        x = (int)cpu->mon_register_get_val(e_comp_space, e_X);
        y = (int)cpu->mon_register_get_val(e_comp_space, e_Y);
        flags = (int)cpu->mon_register_get_val(e_comp_space, e_FLAGS);

        unsigned int rasterLine;
        unsigned int rasterCycle;
        int halfCycle;
        machine_get_line_cycle(&rasterLine, &rasterCycle, &halfCycle);

        extern void mad64_update_vice_state(int rasterline, int rasterCycle, int pc, int acc, int x, int y, int flags);
        mad64_update_vice_state(rasterLine, rasterCycle, pc, acc, x, y, flags);
    }
}

void capture_and_update_vice_state(uint16_t currentPc, void* userData)
{
    helper_update_vice_state();
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

