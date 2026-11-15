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
#include "viciitypes.h"
#include <string.h>

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





/* MAD64: VIC-II beam position for the VicScreen visualiser.
   beamX is the number of pixels of the current raster line that have been output (8 per cycle). */
void helper_get_vicii_beam(int *line, int *beamX, int *cyclesPerLine, int *screenHeight)
{
    int count = vicii.dbuf_offset;
    int curLine = (int)vicii.raster_line;

    /* at raster cycle index 0 the previous line has just been completed; nothing of the new line is drawn yet */
    if (vicii.raster_cycle == 0) {
        count = 0;
        if (vicii.start_of_frame)
            curLine = 0;
    }

    *line = curLine;
    *beamX = count;
    *cyclesPerLine = vicii.cycles_per_line;
    *screenHeight = (int)vicii.screen_height;
}

/* MAD64: snapshot of everything the VIC-II can see, so MAD64 can render a frame itself.
   ram: 64k, colourRam: 1k, charRom: 4k, vbank: base address of the 16k video bank (0, $4000, $8000, $c000) */
void helper_get_vicii_memory(unsigned char *ram, unsigned char *colourRam, unsigned char *charRom, int *vbank)
{
    extern uint8_t mem_chargen_rom[];

    memcpy(ram, mem_ram, 0x10000);
    memcpy(colourRam, mem_color_ram_vicii, 0x400);
    memcpy(charRom, mem_chargen_rom, 0x1000);
    *vbank = vicii.vbank_phi1 & 0xc000;
}

/* MAD64: VIC-II registers for the VicScreen register panel.
   written: values last written by the CPU (e.g. raster IRQ compare in $d011/$d012)
   read:    what the CPU would read now, without side effects (e.g. current raster line, collisions) */
void helper_get_vicii_regs(unsigned char *written, unsigned char *read, int count)
{
    extern uint8_t vicii_peek(uint16_t addr);
    int i;

    if (count > 0x40)
        count = 0x40;

    for (i = 0; i < count; i++) {
        written[i] = vicii.regs[i];
        read[i] = vicii_peek((uint16_t)i);
    }
}

#include "sid.h"
#include "sid-snapshot.h"

/* MAD64: select the emulated SID chip, keeping the current SID engine. is8580: 0 = 6581, 1 = 8580 */
void helper_set_sid_model(int is8580)
{
    int engine = SID_ENGINE_RESID;

    resources_get_int("SidEngine", &engine);
    sid_set_engine_model(engine, is8580 ? SID_MODEL_8580 : SID_MODEL_6581);
}

/* MAD64: snapshot of the SID chip state for the SidWindow.
   regs: the 32 registers as last written by the CPU
   accumulator / shiftRegister / envelope / envelopeState: 3 entries each, the live state of the voices
   (envelopeState: 0 attack, 1 decay/sustain, 2 release, 3 frozen)
   model: 0 = 6581, otherwise 8580 */
void helper_get_sid_state(unsigned char *regs, unsigned int *accumulator, unsigned int *shiftRegister,
                          unsigned char *envelope, unsigned char *envelopeState, int *model)
{
    sid_snapshot_state_t state;
    uint8_t *written;
    int i, sidModel = 0;

    memset(&state, 0, sizeof(state));
    sid_state_read(0, &state);

    written = sid_get_siddata(0);
    for (i = 0; i < 0x20; i++) {
        regs[i] = written ? written[i] : state.sid_register[i];
    }

    for (i = 0; i < 3; i++) {
        accumulator[i] = state.accumulator[i];
        shiftRegister[i] = state.shift_register[i];
        envelope[i] = state.envelope_counter[i];
        envelopeState[i] = state.envelope_state[i];
    }

    resources_get_int("SidModel", &sidModel);
    *model = sidModel;
}
