#ifndef MAD_DRIVE_H
#define MAD_DRIVE_H

/* MAD64: disk drive status shared between the VICE side helpers (C) and ViceBridge (C++). */

#define MAD_DRIVE_UNITS     4       /* units 8..11 */
#define MAD_DRIVE_NAME_MAX  260
#define MAD_DRIVE_MAX_JOBS  9

typedef struct mad_drive_info_s {
    int type;                   /* VICE DRIVE_TYPE_*, 0 = no drive */
    int trueEmulation;          /* drive CPU and mechanics are emulated */
    int hasImage;               /* a disk image is attached */
    int imageType;              /* DISK_IMAGE_TYPE_* */
    int imageTracks;            /* number of tracks in the image */
    int readOnly;

    /* mechanics (true drive emulation only) */
    int halfTrack;              /* head position, 2 = track 1 */
    int side;
    int motorOn;
    int ledOn;
    int writeMode;              /* head is in write mode */
    int headOffset;             /* byte offset of the head on the current track */
    int trackSize;              /* bytes of GCR data on the current track */
    int headerTrack;            /* last sector header passed by the head, -1 if unknown */
    int headerSector;

    /* DOS job queue (true drive emulation, CBM DOS drives only) */
    int jobCount;
    unsigned char jobCode[MAD_DRIVE_MAX_JOBS];
    unsigned char jobTrack[MAD_DRIVE_MAX_JOBS];
    unsigned char jobSector[MAD_DRIVE_MAX_JOBS];

    char typeName[32];
    char imageName[MAD_DRIVE_NAME_MAX];
} mad_drive_info_t;

#ifdef __cplusplus
extern "C" {
#endif

/* fills in the status of a unit (0..MAD_DRIVE_UNITS-1). Call on the emulation thread. */
void helper_get_drive_info(int unit, mad_drive_info_t *info);

/* returns the unit (0..MAD_DRIVE_UNITS-1) a disk image is attached to, or -1 */
int helper_disk_image_unit(const void *image);

/* called by the disk image layer whenever a sector is read from or written to an image */
void mad64_disk_sector_access(const void *image, unsigned int track, unsigned int sector, int isWrite);

#ifdef __cplusplus
}
#endif

#endif
