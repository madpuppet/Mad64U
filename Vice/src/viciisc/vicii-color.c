/*
 * vicii-color.c - Colors for the MOS 6569 (VIC-II) emulation.
 *
 * Written by
 *  John Selck <graham@cruise.de>
 *
 * Research about the YUV values by
 *  Philip Timmermann <pepto@pepto.de>
 *  John Selck <graham@cruise.de>
 *
 * This file is part of VICE, the Versatile Commodore Emulator.
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

#include "log.h"
#include "vicii.h"
#include "viciitypes.h"
#include "vicii-color.h"
#include "vicii-resources.h"
#include "video.h"

/* undefine for the old "idealized" Pepto colors */
/* #define PEPTO_COLORS */

/* undefine for the new "idealized" Pepto colors, aka "colodore" */
/* #define COLODORE_COLORS */

/* undefine for extra colors that use markos measured lumas */
/* #define MARKO_LUMAS */

/* undefine for the colors measured by Tobias */
#define TOBIAS_COLORS

/* undefine to use seperate palettes for odd/even lines */
#define SEPERATE_ODD_EVEN_COLORS

/******************************************************************************/

#if defined(PEPTO_COLORS) || defined (COLODORE_COLORS)
/* base saturation of all colors except the grey tones */

/* must stay below 64 to not result in overflows in the CRT renderer (and maybe
   elsewhere) */
#define VICII_SATURATION 48.0f

/* phase shift of all colors */

#define VICII_PHASE -4.5f

/* chroma angles in UV space */

#define ANGLE_RED        112.5f

#ifdef PEPTO_COLORS
#define ANGLE_GRN       -135.0f /* old pepto */
#else
#define ANGLE_GRN       -112.5f /* new pepto ("colodore") */
#endif

#define ANGLE_BLU          0.0f
#define ANGLE_ORN        -45.0f /* negative orange (orange is at +135.0 degree) */
#define ANGLE_BRN        157.5f

#endif

/*
  6567R56A (NTSC old)
  Voltage (ripple)
1  560 (495-640)
2  840 (780-920)
3 1180 (1100-1240)
4 1500 (1440-1600)
5 1825 (1775-1900)

  6567R8 (NTSC)
  Voltage (ripple)
1  590 (570-630)
2  860 (840-880)
3  950 (920-980)
4 1030 (1010-1050)
5 1160 (1140-1200)
6 1210 (1190-1230)
7 1380 (1360-1400)
8 1560 (1540-1580)
9 1825

   6569R1 (PAL)
   Voltage (ripple)
1   630 (590-680)
2   900 (860-960)
3  1260 (1200-1320)
4  1560 (1500-1620)
5  1850

  6569R3 (PAL)       6569R4 (PAL)       6569R5 (PAL)
  Voltage (ripple)
1 700 (680-720)       500 (480-520)     540 (520-560)
2 1020 (990-1030)     760 (740-780)     810 (790-830)
3 1090 (1070-1110)    840 (820-860)     900 (880-920)
4 1180 (1160-1200)    920 (900-940)     980 (960-1000)
5 1300 (1280-1320)   1050 (1030-1070)   1110 (1090-1130)
6 1340 (1320-1360)   1100 (1080-1120)   1150 (1130-1170)
7 1480 (1460-1500)   1300 (1280-1320)   1340 (1320-1360)
8 1620 (1600-1640)   1500 (1480-1520)   1520 (1500-1540)
9 1850               1875               1850

FIXME: measurements missing for: 8565 (PAL), 6562 (NTSC), 6572 (PAL-N)

 */

#if defined(PEPTO_COLORS)

/* new luminances */
#define LUMN0     0.0f
#define LUMN1    56.0f
#define LUMN2    74.0f
#define LUMN3    92.0f
#define LUMN4   117.0f
#define LUMN5   128.0f
#define LUMN6   163.0f
#define LUMN7   199.0f
#define LUMN8   256.0f

/* old luminances */
#define LUMO0     0.0f
#define LUMO1    56.0f
#define LUMO2   128.0f
#define LUMO3   191.0f
#define LUMO4   256.0f

#endif

#if defined (COLODORE_COLORS)

/* new luminances */
#define LUMN0     0.0f
#define LUMN1    64.0f
#define LUMN2    80.0f
#define LUMN3    96.0f
#define LUMN4   120.0f
#define LUMN5   128.0f
#define LUMN6   160.0f
#define LUMN7   192.0f
#define LUMN8   256.0f

/* old luminances */
#define LUMO0     0.0f
#define LUMO1    64.0f
#define LUMO2   128.0f
#define LUMO3   192.0f
#define LUMO4   256.0f

#endif

/******************************************************************************/

#if defined(PEPTO_COLORS) || defined (COLODORE_COLORS)
/* very old vic-ii palette with less luminances */
static video_cbm_color_t vicii_colors_old[VICII_NUM_COLORS] =
{
    { LUMO0, ANGLE_ORN, VICII_SATURATION, -0, "Black"       },
    { LUMO4, ANGLE_BRN, VICII_SATURATION,  0, "White"       },
    { LUMO1, ANGLE_RED, VICII_SATURATION,  1, "Red"         },
    { LUMO3, ANGLE_RED, VICII_SATURATION, -1, "Cyan"        },
    { LUMO2, ANGLE_GRN, VICII_SATURATION, -1, "Purple"      },
    { LUMO2, ANGLE_GRN, VICII_SATURATION,  1, "Green"       },
    { LUMO1, ANGLE_BLU, VICII_SATURATION,  1, "Blue"        },
    { LUMO3, ANGLE_BLU, VICII_SATURATION, -1, "Yellow"      },
    { LUMO2, ANGLE_ORN, VICII_SATURATION, -1, "Orange"      },
    { LUMO1, ANGLE_BRN, VICII_SATURATION,  1, "Brown"       },
    { LUMO2, ANGLE_RED, VICII_SATURATION,  1, "Light Red"   },
    { LUMO1, ANGLE_RED, VICII_SATURATION, -0, "Dark Grey"   },
    { LUMO2, ANGLE_GRN, VICII_SATURATION, -0, "Medium Grey" },
    { LUMO3, ANGLE_GRN, VICII_SATURATION,  1, "Light Green" },
    { LUMO2, ANGLE_BLU, VICII_SATURATION,  1, "Light Blue"  },
    { LUMO3, ANGLE_BLU, VICII_SATURATION, -0, "Light Grey"  }
};

/* the wellknown vic-ii palette used for 99% of all vic-ii chips */
static video_cbm_color_t vicii_colors[VICII_NUM_COLORS] =
{
    { LUMN0, ANGLE_ORN, VICII_SATURATION, -0, "Black"       },
    { LUMN8, ANGLE_BRN, VICII_SATURATION,  0, "White"       },
    { LUMN2, ANGLE_RED, VICII_SATURATION,  1, "Red"         },
    { LUMN6, ANGLE_RED, VICII_SATURATION, -1, "Cyan"        },
    { LUMN3, ANGLE_GRN, VICII_SATURATION, -1, "Purple"      },
    { LUMN5, ANGLE_GRN, VICII_SATURATION,  1, "Green"       },
    { LUMN1, ANGLE_BLU, VICII_SATURATION,  1, "Blue"        },
    { LUMN7, ANGLE_BLU, VICII_SATURATION, -1, "Yellow"      },
    { LUMN3, ANGLE_ORN, VICII_SATURATION, -1, "Orange"      },
    { LUMN1, ANGLE_BRN, VICII_SATURATION,  1, "Brown"       },
    { LUMN5, ANGLE_RED, VICII_SATURATION,  1, "Light Red"   },
    { LUMN2, ANGLE_RED, VICII_SATURATION, -0, "Dark Grey"   },
    { LUMN4, ANGLE_GRN, VICII_SATURATION, -0, "Medium Grey" },
    { LUMN7, ANGLE_GRN, VICII_SATURATION,  1, "Light Green" },
    { LUMN4, ANGLE_BLU, VICII_SATURATION,  1, "Light Blue"  },
    { LUMN6, ANGLE_BLU, VICII_SATURATION, -0, "Light Grey"  }
};

static video_cbm_palette_t vicii_palette_old =
{
    VICII_NUM_COLORS,
    vicii_colors_old,
    NULL, NULL,
    VICII_PHASE,
    CBM_PALETTE_YUV
};

static video_cbm_palette_t vicii_palette =
{
    VICII_NUM_COLORS,
    vicii_colors,
    NULL, NULL,
    VICII_PHASE,
    CBM_PALETTE_YUV
};
#endif

/******************************************************************************/

#if defined(MARKO_LUMAS) && (defined(PEPTO_COLORS) || defined (COLODORE_COLORS))

#define LUMA(x,y,z) ((((float)x-y)*256.0f)/(z-y))

static video_cbm_color_t vicii_colors_6567r56a[VICII_NUM_COLORS] =
{
    { LUMA( 560,560,1825), ANGLE_ORN, VICII_SATURATION, -0, "Black"       },
    { LUMA(1825,560,1825), ANGLE_BRN, VICII_SATURATION,  0, "White"       },
    { LUMA( 840,560,1825), ANGLE_RED, VICII_SATURATION,  1, "Red"         },
    { LUMA(1500,560,1825), ANGLE_RED, VICII_SATURATION, -1, "Cyan"        },
    { LUMA(1180,560,1825), ANGLE_GRN, VICII_SATURATION, -1, "Purple"      },
    { LUMA(1180,560,1825), ANGLE_GRN, VICII_SATURATION,  1, "Green"       },
    { LUMA( 840,560,1825), ANGLE_BLU, VICII_SATURATION,  1, "Blue"        },
    { LUMA(1500,560,1825), ANGLE_BLU, VICII_SATURATION, -1, "Yellow"      },
    { LUMA(1180,560,1825), ANGLE_ORN, VICII_SATURATION, -1, "Orange"      },
    { LUMA( 840,560,1825), ANGLE_BRN, VICII_SATURATION,  1, "Brown"       },
    { LUMA(1180,560,1825), ANGLE_RED, VICII_SATURATION,  1, "Light Red"   },
    { LUMA( 840,560,1825), ANGLE_RED, VICII_SATURATION, -0, "Dark Grey"   },
    { LUMA(1180,560,1825), ANGLE_GRN, VICII_SATURATION, -0, "Medium Grey" },
    { LUMA(1500,560,1825), ANGLE_GRN, VICII_SATURATION,  1, "Light Green" },
    { LUMA(1180,560,1825), ANGLE_BLU, VICII_SATURATION,  1, "Light Blue"  },
    { LUMA(1500,560,1825), ANGLE_BLU, VICII_SATURATION, -0, "Light Grey"  }
};

static video_cbm_color_t vicii_colors_6567r8[VICII_NUM_COLORS] =
{
    { LUMA( 590,590,1825), ANGLE_ORN, VICII_SATURATION, -0, "Black"       },
    { LUMA(1825,590,1825), ANGLE_BRN, VICII_SATURATION,  0, "White"       },
    { LUMA( 950,590,1825), ANGLE_RED, VICII_SATURATION,  1, "Red"         },
    { LUMA(1380,590,1825), ANGLE_RED, VICII_SATURATION, -1, "Cyan"        },
    { LUMA(1030,590,1825), ANGLE_GRN, VICII_SATURATION, -1, "Purple"      },
    { LUMA(1210,590,1825), ANGLE_GRN, VICII_SATURATION,  1, "Green"       },
    { LUMA( 860,590,1825), ANGLE_BLU, VICII_SATURATION,  1, "Blue"        },
    { LUMA(1560,590,1825), ANGLE_BLU, VICII_SATURATION, -1, "Yellow"      },
    { LUMA(1030,590,1825), ANGLE_ORN, VICII_SATURATION, -1, "Orange"      },
    { LUMA( 860,590,1825), ANGLE_BRN, VICII_SATURATION,  1, "Brown"       },
    { LUMA(1210,590,1825), ANGLE_RED, VICII_SATURATION,  1, "Light Red"   },
    { LUMA( 950,590,1825), ANGLE_RED, VICII_SATURATION, -0, "Dark Grey"   },
    { LUMA(1160,590,1825), ANGLE_GRN, VICII_SATURATION, -0, "Medium Grey" },
    { LUMA(1560,590,1825), ANGLE_GRN, VICII_SATURATION,  1, "Light Green" },
    { LUMA(1160,590,1825), ANGLE_BLU, VICII_SATURATION,  1, "Light Blue"  },
    { LUMA(1380,590,1825), ANGLE_BLU, VICII_SATURATION, -0, "Light Grey"  }
};

static video_cbm_color_t vicii_colors_6569r1[VICII_NUM_COLORS] =
{
    { LUMA( 630,630,1850), ANGLE_ORN, VICII_SATURATION, -0, "Black"       },
    { LUMA(1850,630,1850), ANGLE_BRN, VICII_SATURATION,  0, "White"       },
    { LUMA( 900,630,1850), ANGLE_RED, VICII_SATURATION,  1, "Red"         },
    { LUMA(1560,630,1850), ANGLE_RED, VICII_SATURATION, -1, "Cyan"        },
    { LUMA(1260,630,1850), ANGLE_GRN, VICII_SATURATION, -1, "Purple"      },
    { LUMA(1260,630,1850), ANGLE_GRN, VICII_SATURATION,  1, "Green"       },
    { LUMA( 900,630,1850), ANGLE_BLU, VICII_SATURATION,  1, "Blue"        },
    { LUMA(1560,630,1850), ANGLE_BLU, VICII_SATURATION, -1, "Yellow"      },
    { LUMA(1260,630,1850), ANGLE_ORN, VICII_SATURATION, -1, "Orange"      },
    { LUMA( 900,630,1850), ANGLE_BRN, VICII_SATURATION,  1, "Brown"       },
    { LUMA(1260,630,1850), ANGLE_RED, VICII_SATURATION,  1, "Light Red"   },
    { LUMA( 900,630,1850), ANGLE_RED, VICII_SATURATION, -0, "Dark Grey"   },
    { LUMA(1260,630,1850), ANGLE_GRN, VICII_SATURATION, -0, "Medium Grey" },
    { LUMA(1560,630,1850), ANGLE_GRN, VICII_SATURATION,  1, "Light Green" },
    { LUMA(1260,630,1850), ANGLE_BLU, VICII_SATURATION,  1, "Light Blue"  },
    { LUMA(1560,630,1850), ANGLE_BLU, VICII_SATURATION, -0, "Light Grey"  }
};

static video_cbm_color_t vicii_colors_6569r3[VICII_NUM_COLORS] =
{
    { LUMA( 700,700,1850), ANGLE_ORN, VICII_SATURATION, -0, "Black"       },
    { LUMA(1850,700,1850), ANGLE_BRN, VICII_SATURATION,  0, "White"       },
    { LUMA(1090,700,1850), ANGLE_RED, VICII_SATURATION,  1, "Red"         },
    { LUMA(1480,700,1850), ANGLE_RED, VICII_SATURATION, -1, "Cyan"        },
    { LUMA(1180,700,1850), ANGLE_GRN, VICII_SATURATION, -1, "Purple"      },
    { LUMA(1340,700,1850), ANGLE_GRN, VICII_SATURATION,  1, "Green"       },
    { LUMA(1020,700,1850), ANGLE_BLU, VICII_SATURATION,  1, "Blue"        },
    { LUMA(1620,700,1850), ANGLE_BLU, VICII_SATURATION, -1, "Yellow"      },
    { LUMA(1180,700,1850), ANGLE_ORN, VICII_SATURATION, -1, "Orange"      },
    { LUMA(1020,700,1850), ANGLE_BRN, VICII_SATURATION,  1, "Brown"       },
    { LUMA(1340,700,1850), ANGLE_RED, VICII_SATURATION,  1, "Light Red"   },
    { LUMA(1090,700,1850), ANGLE_RED, VICII_SATURATION, -0, "Dark Grey"   },
    { LUMA(1300,700,1850), ANGLE_GRN, VICII_SATURATION, -0, "Medium Grey" },
    { LUMA(1620,700,1850), ANGLE_GRN, VICII_SATURATION,  1, "Light Green" },
    { LUMA(1300,700,1850), ANGLE_BLU, VICII_SATURATION,  1, "Light Blue"  },
    { LUMA(1480,700,1850), ANGLE_BLU, VICII_SATURATION, -0, "Light Grey"  }
};

static video_cbm_palette_t vicii_palette_6567r56a =
{
    VICII_NUM_COLORS,
    vicii_colors_6567r56a,
    NULL, NULL,
    VICII_PHASE,
    CBM_PALETTE_YUV
};

static video_cbm_palette_t vicii_palette_6567r8 =
{
    VICII_NUM_COLORS,
    vicii_colors_6567r8,
    NULL, NULL,
    VICII_PHASE,
    CBM_PALETTE_YUV
};

static video_cbm_palette_t vicii_palette_6569r1 =
{
    VICII_NUM_COLORS,
    vicii_colors_6569r1,
    NULL, NULL,
    VICII_PHASE,
    CBM_PALETTE_YUV
};

static video_cbm_palette_t vicii_palette_6569r3 =
{
    VICII_NUM_COLORS,
    vicii_colors_6569r3,
    NULL, NULL,
    VICII_PHASE,
    CBM_PALETTE_YUV
};

#endif

/******************************************************************************/

#ifdef TOBIAS_COLORS

static video_cbm_color_t vicii_colors_6569r1[VICII_NUM_COLORS] =
{
    { 0.000f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Black"       },
    { 1.000f * 256.0f,           0.00f         , 0.000f * 256.0f,  0, "White"       },
    { 0.237f * 256.0f,          95.50f         , 0.217f * 256.0f,  1, "Red"         },
    { 0.763f * 256.0f,         275.50f - 180.0f, 0.210f * 256.0f, -1, "Cyan"        },
    { 0.500f * 256.0f,          54.00f - 180.0f, 0.219f * 256.0f, -1, "Purple"      },
    { 0.500f * 256.0f,         241.70f         , 0.213f * 256.0f,  1, "Green"       },
    { 0.237f * 256.0f,         355.60f - 360.0f, 0.217f * 256.0f,  1, "Blue"        },
    { 0.763f * 256.0f,         175.60f - 180.0f, 0.211f * 256.0f, -1, "Yellow"      },
    { 0.500f * 256.0f,         128.25f - 180.0f, 0.217f * 256.0f, -1, "Orange"      },
    { 0.237f * 256.0f,         146.50f         , 0.214f * 256.0f,  1, "Brown"       },
    { 0.500f * 256.0f,          95.50f         , 0.217f * 256.0f,  1, "Light Red"   },
    { 0.237f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Dark Grey"   },
    { 0.500f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Medium Grey" },
    { 0.763f * 256.0f,         241.70f - 360.0f, 0.213f * 256.0f,  1, "Light Green" },
    { 0.500f * 256.0f,         355.60f - 360.0f, 0.217f * 256.0f,  1, "Light Blue"  },
    { 0.763f * 256.0f,          0.00f         , 0.000f * 256.0f,  0, "Light Grey"  }
};

static video_cbm_color_t vicii_colors_6569r1_even[VICII_NUM_COLORS] =
{
    { 0.000f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Black"       },
    { 1.000f * 256.0f,           0.00f         , 0.000f * 256.0f,  0, "White"       },
    { 0.237f * 256.0f,          89.00f         , 0.202f * 256.0f,  1, "Red"         },
    { 0.763f * 256.0f,         269.25f - 180.0f, 0.191f * 256.0f, -1, "Cyan"        },
    { 0.500f * 256.0f,          48.50f - 180.0f, 0.226f * 256.0f, -1, "Purple"      },
    { 0.500f * 256.0f,         235.45f         , 0.222f * 256.0f,  1, "Green"       },
    { 0.237f * 256.0f,         -12.40f - 360.0f, 0.234f * 256.0f,  1, "Blue"        },
    { 0.763f * 256.0f,         168.60f - 180.0f, 0.231f * 256.0f, -1, "Yellow"      },
    { 0.500f * 256.0f,         122.00f - 180.0f, 0.213f * 256.0f, -1, "Orange"      },
    { 0.237f * 256.0f,         140.00f         , 0.226f * 256.0f,  1, "Brown"       },
    { 0.500f * 256.0f,          89.00f         , 0.202f * 256.0f,  1, "Light Red"   },
    { 0.237f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Dark Grey"   },
    { 0.500f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Medium Grey" },
    { 0.763f * 256.0f,         235.45f - 360.0f, 0.222f * 256.0f,  1, "Light Green" },
    { 0.500f * 256.0f,         -12.40f - 360.0f, 0.234f * 256.0f,  1, "Light Blue"  },
    { 0.763f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Light Grey"  }
};

static video_cbm_color_t vicii_colors_6569r1_odd[VICII_NUM_COLORS] =
{
    { 0.000f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Black"       },
    { 1.000f * 256.0f,           0.00f         , 0.000f * 256.0f,  0, "White"       },
    { 0.237f * 256.0f,         102.00f         , 0.232f * 256.0f,  1, "Red"         },
    { 0.763f * 256.0f,         281.75f - 180.0f, 0.228f * 256.0f, -1, "Cyan"        },
    { 0.500f * 256.0f,          59.50f - 180.0f, 0.211f * 256.0f, -1, "Purple"      },
    { 0.500f * 256.0f,         247.95f         , 0.205f * 256.0f,  1, "Green"       },
    { 0.237f * 256.0f,         363.60f - 360.0f, 0.200f * 256.0f,  1, "Blue"        },
    { 0.763f * 256.0f,         182.60f - 180.0f, 0.191f * 256.0f, -1, "Yellow"      },
    { 0.500f * 256.0f,         134.50f - 180.0f, 0.221f * 256.0f, -1, "Orange"      },
    { 0.237f * 256.0f,         153.00f         , 0.202f * 256.0f,  1, "Brown"       },
    { 0.500f * 256.0f,         102.00f         , 0.232f * 256.0f,  1, "Light Red"   },
    { 0.237f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Dark Grey"   },
    { 0.500f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Medium Grey" },
    { 0.763f * 256.0f,         247.95f - 360.0f, 0.205f * 256.0f,  1, "Light Green" },
    { 0.500f * 256.0f,         363.60f - 360.0f, 0.200f * 256.0f,  1, "Light Blue"  },
    { 0.763f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Light Grey"  }
};

static video_cbm_palette_t vicii_palette_6569r1 =
{
    VICII_NUM_COLORS,
    vicii_colors_6569r1,
#ifdef SEPERATE_ODD_EVEN_COLORS
    vicii_colors_6569r1_odd,
    vicii_colors_6569r1_even,
#else
    NULL, NULL,
#endif
    0.0,
    CBM_PALETTE_YUV
};

static video_cbm_color_t vicii_colors_6569r5[VICII_NUM_COLORS] =
{
    { 0.000f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Black"       },
    { 1.000f * 256.0f,           0.00f         , 0.000f * 256.0f,  0, "White"       },
    { 0.306f * 256.0f,          95.50f         , 0.217f * 256.0f,  1, "Red"         },
    { 0.639f * 256.0f,         275.50f - 180.0f, 0.210f * 256.0f, -1, "Cyan"        },
    { 0.363f * 256.0f,          54.00f - 180.0f, 0.219f * 256.0f, -1, "Purple"      },
    { 0.500f * 256.0f,         241.70f         , 0.213f * 256.0f,  1, "Green"       },
    { 0.237f * 256.0f,         355.60f - 360.0f, 0.217f * 256.0f,  1, "Blue"        },
    { 0.763f * 256.0f,         175.60f - 180.0f, 0.211f * 256.0f, -1, "Yellow"      },
    { 0.363f * 256.0f,         128.25f - 180.0f, 0.217f * 256.0f, -1, "Orange"      },
    { 0.237f * 256.0f,         146.50f         , 0.214f * 256.0f,  1, "Brown"       },
    { 0.500f * 256.0f,          95.50f         , 0.217f * 256.0f,  1, "Light Red"   },
    { 0.306f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Dark Grey"   },
    { 0.461f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Medium Grey" },
    { 0.763f * 256.0f,         241.70f - 360.0f, 0.213f * 256.0f,  1, "Light Green" },
    { 0.461f * 256.0f,         355.60f - 360.0f, 0.217f * 256.0f,  1, "Light Blue"  },
    { 0.639f * 256.0f,           0.00f         , 0.000f * 256.0f,  0, "Light Grey"  }
};

static video_cbm_color_t vicii_colors_6569r5_even[VICII_NUM_COLORS] =
{
    { 0.000f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Black"       },
    { 1.000f * 256.0f,           0.00f         , 0.000f * 256.0f,  0, "White"       },
    { 0.306f * 256.0f,          89.00f         , 0.202f * 256.0f,  1, "Red"         },
    { 0.639f * 256.0f,         269.25f - 180.0f, 0.191f * 256.0f, -1, "Cyan"        },
    { 0.363f * 256.0f,          48.50f - 180.0f, 0.226f * 256.0f, -1, "Purple"      },
    { 0.500f * 256.0f,         235.45f         , 0.222f * 256.0f,  1, "Green"       },
    { 0.237f * 256.0f,         -12.40f - 360.0f, 0.234f * 256.0f,  1, "Blue"        },
    { 0.763f * 256.0f,         168.60f - 180.0f, 0.231f * 256.0f, -1, "Yellow"      },
    { 0.363f * 256.0f,         122.00f - 180.0f, 0.213f * 256.0f, -1, "Orange"      },
    { 0.237f * 256.0f,         140.00f         , 0.226f * 256.0f,  1, "Brown"       },
    { 0.500f * 256.0f,          89.00f         , 0.202f * 256.0f,  1, "Light Red"   },
    { 0.306f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Dark Grey"   },
    { 0.461f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Medium Grey" },
    { 0.763f * 256.0f,         235.45f - 360.0f, 0.222f * 256.0f,  1, "Light Green" },
    { 0.461f * 256.0f,         -12.40f - 360.0f, 0.234f * 256.0f,  1, "Light Blue"  },
    { 0.639f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Light Grey"  }
};

static video_cbm_color_t vicii_colors_6569r5_odd[VICII_NUM_COLORS] =
{
    { 0.000f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Black"       },
    { 1.000f * 256.0f,           0.00f         , 0.000f * 256.0f,  0, "White"       },
    { 0.306f * 256.0f,         102.00f         , 0.232f * 256.0f,  1, "Red"         },
    { 0.639f * 256.0f,         281.75f - 180.0f, 0.228f * 256.0f, -1, "Cyan"        },
    { 0.363f * 256.0f,          59.50f - 180.0f, 0.211f * 256.0f, -1, "Purple"      },
    { 0.500f * 256.0f,         247.95f         , 0.205f * 256.0f,  1, "Green"       },
    { 0.237f * 256.0f,         363.60f - 360.0f, 0.200f * 256.0f,  1, "Blue"        },
    { 0.763f * 256.0f,         182.60f - 180.0f, 0.191f * 256.0f, -1, "Yellow"      },
    { 0.363f * 256.0f,         134.50f - 180.0f, 0.221f * 256.0f, -1, "Orange"      },
    { 0.237f * 256.0f,         153.00f         , 0.202f * 256.0f,  1, "Brown"       },
    { 0.500f * 256.0f,         102.00f         , 0.232f * 256.0f,  1, "Light Red"   },
    { 0.306f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Dark Grey"   },
    { 0.461f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Medium Grey" },
    { 0.763f * 256.0f,         247.95f - 360.0f, 0.205f * 256.0f,  1, "Light Green" },
    { 0.461f * 256.0f,         363.60f - 360.0f, 0.200f * 256.0f,  1, "Light Blue"  },
    { 0.639f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Light Grey"  }
};

static video_cbm_palette_t vicii_palette_6569r5 =
{
    VICII_NUM_COLORS,
    vicii_colors_6569r5,
#ifdef SEPERATE_ODD_EVEN_COLORS
    vicii_colors_6569r5_odd,
    vicii_colors_6569r5_even,
#else
    NULL, NULL,
#endif
    0.0,
    CBM_PALETTE_YUV
};

static video_cbm_color_t vicii_colors_8565r2[VICII_NUM_COLORS] =
{
    { 0.000f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Black"       },
    { 1.000f * 256.0f,           0.00f         , 0.000f * 256.0f,  0, "White"       },
    { 0.306f * 256.0f,         102.50f         , 0.214f * 256.0f,  1, "Red"         },
    { 0.639f * 256.0f,         281.50f - 180.0f, 0.216f * 256.0f, -1, "Cyan"        },
    { 0.363f * 256.0f,          51.00f - 180.0f, 0.214f * 256.0f, -1, "Purple"      },
    { 0.500f * 256.0f,         238.70f         , 0.214f * 256.0f,  1, "Green"       },
    { 0.237f * 256.0f,         345.10f - 360.0f, 0.214f * 256.0f,  1, "Blue"        },
    { 0.763f * 256.0f,         165.10f - 180.0f, 0.214f * 256.0f, -1, "Yellow"      },
    { 0.363f * 256.0f,         126.00f - 180.0f, 0.213f * 256.0f, -1, "Orange"      },
    { 0.237f * 256.0f,         146.00f         , 0.213f * 256.0f,  1, "Brown"       },
    { 0.500f * 256.0f,         102.50f         , 0.214f * 256.0f,  1, "Light Red"   },
    { 0.306f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Dark Grey"   },
    { 0.461f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Medium Grey" },
    { 0.763f * 256.0f,         238.70f - 360.0f, 0.214f * 256.0f,  1, "Light Green" },
    { 0.461f * 256.0f,         345.10f - 360.0f, 0.214f * 256.0f,  1, "Light Blue"  },
    { 0.639f * 256.0f,           0.00f         , 0.000f * 256.0f,  0, "Light Grey"  }
};

static video_cbm_color_t vicii_colors_8565r2_even[VICII_NUM_COLORS] =
{
    { 0.000f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Black"       },
    { 1.000f * 256.0f,           0.00f         , 0.000f * 256.0f,  0, "White"       },
    { 0.306f * 256.0f,          93.50f         , 0.212f * 256.0f,  1, "Red"         },
    { 0.639f * 256.0f,         273.00f - 180.0f, 0.215f * 256.0f, -1, "Cyan"        },
    { 0.363f * 256.0f,          43.00f - 180.0f, 0.214f * 256.0f, -1, "Purple"      },
    { 0.500f * 256.0f,         231.70f         , 0.216f * 256.0f,  1, "Green"       },
    { 0.237f * 256.0f,         -24.40f - 360.0f, 0.215f * 256.0f,  1, "Blue"        },
    { 0.763f * 256.0f,         169.60f - 180.0f, 0.215f * 256.0f, -1, "Yellow"      },
    { 0.363f * 256.0f,         120.00f - 180.0f, 0.211f * 256.0f, -1, "Orange"      },
    { 0.237f * 256.0f,         146.50f         , 0.212f * 256.0f,  1, "Brown"       },
    { 0.500f * 256.0f,          93.50f         , 0.212f * 256.0f,  1, "Light Red"   },
    { 0.306f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Dark Grey"   },
    { 0.461f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Medium Grey" },
    { 0.763f * 256.0f,         231.70f - 360.0f, 0.216f * 256.0f,  1, "Light Green" },
    { 0.461f * 256.0f,         -24.40f - 360.0f, 0.215f * 256.0f,  1, "Light Blue"  },
    { 0.639f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Light Grey"  }
};

static video_cbm_color_t vicii_colors_8565r2_odd[VICII_NUM_COLORS] =
{
    { 0.000f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Black"       },
    { 1.000f * 256.0f,           0.00f         , 0.000f * 256.0f,  0, "White"       },
    { 0.306f * 256.0f,         111.50f         , 0.215f * 256.0f,  1, "Red"         },
    { 0.639f * 256.0f,         290.00f - 180.0f, 0.217f * 256.0f, -1, "Cyan"        },
    { 0.363f * 256.0f,          59.00f - 180.0f, 0.213f * 256.0f, -1, "Purple"      },
    { 0.500f * 256.0f,         245.70f         , 0.212f * 256.0f,  1, "Green"       },
    { 0.237f * 256.0f,         354.60f - 360.0f, 0.213f * 256.0f,  1, "Blue"        },
    { 0.763f * 256.0f,         160.60f - 180.0f, 0.214f * 256.0f, -1, "Yellow"      },
    { 0.363f * 256.0f,         132.00f - 180.0f, 0.215f * 256.0f, -1, "Orange"      },
    { 0.237f * 256.0f,         145.50f         , 0.213f * 256.0f,  1, "Brown"       },
    { 0.500f * 256.0f,         111.50f         , 0.215f * 256.0f,  1, "Light Red"   },
    { 0.306f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Dark Grey"   },
    { 0.461f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Medium Grey" },
    { 0.763f * 256.0f,         245.70f - 360.0f, 0.212f * 256.0f,  1, "Light Green" },
    { 0.461f * 256.0f,         354.60f - 360.0f, 0.213f * 256.0f,  1, "Light Blue"  },
    { 0.639f * 256.0f,           0.00f         , 0.000f * 256.0f, -0, "Light Grey"  }
};

static video_cbm_palette_t vicii_palette_8565r2 =
{
    VICII_NUM_COLORS,
    vicii_colors_8565r2,
#ifdef SEPERATE_ODD_EVEN_COLORS
    vicii_colors_8565r2_odd,
    vicii_colors_8565r2_even,
#else
    NULL, NULL,
#endif
    0.0,
    CBM_PALETTE_YUV
};

#endif

/******************************************************************************/

int vicii_color_update_palette(struct video_canvas_s *canvas)
{
    video_cbm_palette_t *cp;


    /* first setup the fallback default */
#if defined(PEPTO_COLORS) || defined (COLODORE_COLORS)
    cp = &vicii_palette;
#endif
#ifdef TOBIAS_COLORS
    cp = &vicii_palette_6569r5;
#endif

#if defined(PEPTO_COLORS) || defined (COLODORE_COLORS)
    switch (vicii_resources.model) {
        case VICII_MODEL_6567R56A:      /* NTSC, 64 cycle, ? luma, "old" */
            cp = &vicii_palette_6567r56a;
            break;
        case VICII_MODEL_6567:          /* NTSC, 65 cycle, 9 luma, "old" */
            cp = &vicii_palette_6567r8;
            break;
        case VICII_MODEL_6569R1:        /* PAL, 63 cycle, 5 luma, "old" */
            cp = &vicii_palette_6569r1;
            break;
        case VICII_MODEL_6569:          /* PAL, 63 cycle, 9 luma, "old" */
            cp = &vicii_palette_6569r3;
            break;
        case VICII_MODEL_6572:          /* PAL-N, 65 cycle, ? luma, "?" */
        case VICII_MODEL_8565:          /* PAL, 63 cycle, 9 luma, "new" */
        case VICII_MODEL_8562:          /* NTSC, 65 cycle, 9 luma, "new" */
            cp = &vicii_palette;
            break;
        default:
            log_error(LOG_DEFAULT, "vicii_color_update_palette: unknown VICII type.");
            break;
    }
#endif

    /* now setup palette according to selected VICII */
#ifdef TOBIAS_COLORS
    switch (vicii_resources.model) {
        case VICII_MODEL_6567R56A:      /* NTSC, 64 cycle, ? luma, "old" */
        case VICII_MODEL_6569R1:        /* PAL, 63 cycle, 5 luma, "old" */
            cp = &vicii_palette_6569r1;
            break;
        case VICII_MODEL_6567:          /* NTSC, 65 cycle, 9 luma, "old" */
        case VICII_MODEL_6572:          /* PAL-N, 65 cycle, ? luma, "?" */
        case VICII_MODEL_6569:          /* PAL, 63 cycle, 9 luma, "old" */
            cp = &vicii_palette_6569r5;
            break;
        case VICII_MODEL_8562:          /* NTSC, 65 cycle, 9 luma, "new" */
        case VICII_MODEL_8565:          /* PAL, 63 cycle, 9 luma, "new" */
            cp = &vicii_palette_8565r2;
            break;
        default:
            log_error(LOG_DEFAULT, "vicii_color_update_palette: unknown VICII type.");
            break;
    }
#endif
    video_color_palette_internal(canvas, cp);
    return 0;
}
