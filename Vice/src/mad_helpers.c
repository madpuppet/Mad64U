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

/* ---------------------------------------------------------------------------------------------------------------- */
/* MAD64: disk drive status for the DriveIOWindow */

#include "diskimage.h"
#include "mad_drive.h"

/* GCR 5 bit code -> nibble, 0xff for invalid codes */
static unsigned char s_gcrDecode[32];
static int s_gcrDecodeInit = 0;

static void gcr_decode_init(void)
{
    static const unsigned char encode[16] = {
        0x0a, 0x0b, 0x12, 0x13, 0x0e, 0x0f, 0x16, 0x17, 0x09, 0x19, 0x1a, 0x1b, 0x0d, 0x1d, 0x1e, 0x15
    };
    int i;

    memset(s_gcrDecode, 0xff, sizeof(s_gcrDecode));
    for (i = 0; i < 16; i++) {
        s_gcrDecode[encode[i]] = (unsigned char)i;
    }
    s_gcrDecodeInit = 1;
}

/* decodes 'count' bytes from GCR data starting at byte 'pos' of a circular track. returns 0 on an invalid code */
static int gcr_decode(const uint8_t *track, unsigned int size, unsigned int pos, unsigned char *out, int count)
{
    unsigned int bits = 0;
    int bitCount = 0;
    int nibbles = 0;

    while (nibbles < count * 2) {
        unsigned char nib;
        if (bitCount < 5) {
            bits = (bits << 8) | track[pos % size];
            pos++;
            bitCount += 8;
        }
        nib = s_gcrDecode[(bits >> (bitCount - 5)) & 0x1f];
        bitCount -= 5;
        if (nib == 0xff) {
            return 0;
        }
        if (nibbles & 1) {
            out[nibbles >> 1] = (unsigned char)((out[nibbles >> 1] << 4) | nib);
        } else {
            out[nibbles >> 1] = nib;
        }
        nibbles++;
    }
    return 1;
}

/* finds the last sector header the head has passed on the current GCR track */
static void find_header_under_head(const drive_t *drive, int *track, int *sector)
{
    const uint8_t *data = drive->GCR_track_start_ptr;
    unsigned int size = drive->GCR_current_track_size;
    unsigned int head, back;

    *track = -1;
    *sector = -1;
    if (data == NULL || size < 16) {
        return;
    }
    if (!s_gcrDecodeInit) {
        gcr_decode_init();
    }

    head = (drive->GCR_head_offset >> 3) % size;
    for (back = 0; back < size; back++) {
        unsigned int pos = (head + size - back) % size;
        /* header block: sync ($ff) then GCR of $08 which starts with $52 */
        if (data[pos] == 0x52 && data[(pos + size - 1) % size] == 0xff) {
            unsigned char hdr[8];
            if (gcr_decode(data, size, pos, hdr, 8) && hdr[0] == 0x08
                && hdr[1] == (hdr[2] ^ hdr[3] ^ hdr[4] ^ hdr[5])) {
                *sector = hdr[2];
                *track = hdr[3];
                return;
            }
        }
    }
}

static const char *drive_type_name(int type)
{
    switch (type) {
        case DRIVE_TYPE_NONE: return "None";
        case DRIVE_TYPE_1540: return "1540";
        case DRIVE_TYPE_1541: return "1541";
        case DRIVE_TYPE_1541II: return "1541-II";
        case DRIVE_TYPE_1551: return "1551";
        case DRIVE_TYPE_1570: return "1570";
        case DRIVE_TYPE_1571: return "1571";
        case DRIVE_TYPE_1571CR: return "1571CR";
        case DRIVE_TYPE_1581: return "1581";
        case DRIVE_TYPE_2000: return "FD2000";
        case DRIVE_TYPE_4000: return "FD4000";
        case DRIVE_TYPE_2031: return "2031";
        case DRIVE_TYPE_2040: return "2040";
        case DRIVE_TYPE_3040: return "3040";
        case DRIVE_TYPE_4040: return "4040";
        case DRIVE_TYPE_1001: return "1001";
        case DRIVE_TYPE_8050: return "8050";
        case DRIVE_TYPE_8250: return "8250";
        case DRIVE_TYPE_9000: return "D9090/60";
        case DRIVE_TYPE_CMDHD: return "CMD HD";
        default: return "Unknown";
    }
}

void helper_get_drive_info(int unit, mad_drive_info_t *info)
{
    diskunit_context_t *dc;
    disk_image_t *image;
    const char *name;
    int i, jobBase = 0, tsBase = 0;

    memset(info, 0, sizeof(*info));
    info->headerTrack = -1;
    info->headerSector = -1;
    if (unit < 0 || unit >= MAD_DRIVE_UNITS || unit >= NUM_DISK_UNITS) {
        return;
    }

    dc = diskunit_context[unit];
    if (dc != NULL) {
        info->type = (int)dc->type;
    }
    strncpy(info->typeName, drive_type_name(info->type), sizeof(info->typeName) - 1);

    image = file_system_get_image((unsigned int)unit + 8, 0);
    if (image != NULL) {
        info->hasImage = 1;
        info->imageType = (int)image->type;
        info->imageTracks = (int)image->tracks;
        info->readOnly = (int)image->read_only;
        name = file_system_get_disk_name((unsigned int)unit + 8, 0);
        if (name != NULL) {
            strncpy(info->imageName, name, sizeof(info->imageName) - 1);
        }
    }

    if (dc == NULL || info->type == DRIVE_TYPE_NONE || !dc->enable) {
        return;
    }
    info->trueEmulation = 1;

    /* job queue location in drive RAM */
    switch (info->type) {
        case DRIVE_TYPE_1540:
        case DRIVE_TYPE_1541:
        case DRIVE_TYPE_1541II:
        case DRIVE_TYPE_1551:
        case DRIVE_TYPE_1570:
        case DRIVE_TYPE_1571:
        case DRIVE_TYPE_1571CR:
        case DRIVE_TYPE_2031:
            jobBase = 0x00;
            tsBase = 0x06;
            info->jobCount = 6;
            break;
        case DRIVE_TYPE_1581:
            jobBase = 0x02;
            tsBase = 0x0b;
            info->jobCount = 9;
            break;
        default:
            break;
    }
    for (i = 0; i < info->jobCount; i++) {
        info->jobCode[i] = dc->drive_ram[jobBase + i];
        info->jobTrack[i] = dc->drive_ram[tsBase + i * 2];
        info->jobSector[i] = dc->drive_ram[tsBase + i * 2 + 1];
    }

    /* GCR drive mechanics */
    if (dc->drives[0] != NULL) {
        drive_t *drive = dc->drives[0];
        info->halfTrack = drive->current_half_track;
        info->side = (int)drive->side;
        info->motorOn = (drive->byte_ready_active & BRA_MOTOR_ON) != 0;
        info->ledOn = drive->led_status != 0;
        info->writeMode = drive->read_write_mode == 0;
        info->trackSize = (int)drive->GCR_current_track_size;
        info->headOffset = drive->GCR_current_track_size ? (int)((drive->GCR_head_offset >> 3) % drive->GCR_current_track_size) : 0;
        if (info->type != DRIVE_TYPE_1581 && info->type != DRIVE_TYPE_2000 && info->type != DRIVE_TYPE_4000) {
            find_header_under_head(drive, &info->headerTrack, &info->headerSector);
        }
    }
}

int helper_disk_image_unit(const void *image)
{
    unsigned int unit;

    if (image == NULL) {
        return -1;
    }
    for (unit = 0; unit < MAD_DRIVE_UNITS; unit++) {
        if ((const void *)file_system_get_image(unit + 8, 0) == image) {
            return (int)unit;
        }
    }
    return -1;
}

/* MAD64: snapshot of memory as the CPU currently sees it, for the disassembler.
   BASIC, KERNAL, character ROM and cartridge ROM are mapped in according to the current $01 / EXROM / GAME
   configuration. I/O ($d000-$dfff when mapped in) is left as the RAM underneath, since peeking I/O from
   another thread is not safe and it is never code anyway. mem: 64k */
void helper_get_cpu_memory(unsigned char *mem)
{
    extern const unsigned int c64meminit_io_config[32];
    int config = mem_get_current_bank_config() & 0x1f;
    unsigned int addr;

    memcpy(mem, mem_ram, 0x10000);
    for (addr = 0x8000; addr <= 0xffff; addr++) {
        if (addr >= 0xd000 && addr < 0xe000 && c64meminit_io_config[config])
            continue;
        mem[addr] = mem_peek_with_config(config, (uint16_t)addr, NULL);
    }
}

#include "joystick.h"

/* MAD64: set the state of a C64 control port joystick from a host controller.
   port: 1 or 2. value: bit 0 up, 1 down, 2 left, 3 right, 4 fire */
void helper_set_joystick(int port, int value)
{
    if (port < 1 || port > 2)
        return;
    joystick_set_value_absolute((unsigned int)(port - 1), (uint16_t)(value & 0x1f));
}

/* MAD64: cold reboot - power cycle the machine (clears RAM, as if switched off and on). Emulation thread only. */
void helper_cold_reset(void)
{
    machine_trigger_reset(MACHINE_RESET_MODE_POWER_CYCLE);
}

/* MAD64: snapshot of the C64 inputs for the InputScreen window (emulation thread).
   keyMatrix: 8 rows (CIA1 port A bit), bit n set = key on port B bit n held
   joystick:  2 ports, bit 0 up, 1 down, 2 left, 3 right, 4 fire
   potx/poty: 2 ports, pot values of the attached devices (0xff if none)
   device:    2 ports x deviceLen chars, attached device names
   returns the port(s) the SID is reading pots from (1 = port 1, 2 = port 2, 3 = both) */
int helper_get_input_state(unsigned char *keyMatrix, unsigned char *joystick, unsigned char *potx, unsigned char *poty, char *device, int deviceLen)
{
    extern int mad_joyport_get_state(int port, char *name, int nameLen, uint8_t *potx, uint8_t *poty);
    int i, mask = 1;

    for (i = 0; i < 8; i++)
        keyMatrix[i] = (unsigned char)keyarr[i];

    for (i = 0; i < 2; i++) {
        joystick[i] = (unsigned char)(get_joystick_value(i) & 0x1f);
        mask = mad_joyport_get_state(i, device + i * deviceLen, deviceLen, &potx[i], &poty[i]);
    }
    return mask;
}
