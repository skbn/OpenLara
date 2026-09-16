#include "game.h"
#include "ecs_quant.h"

#define Object AmiObject
#include <proto/dos.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <graphics/videocontrol.h>
#include <graphics/gfxbase.h>
#include <graphics/modeid.h>
#include <clib/alib_protos.h>
#include <proto/intuition.h>
#include <exec/execbase.h>
#include <proto/exec.h>
#include <proto/lowlevel.h>
#include <proto/graphics.h>

#include <cybergraphx/cybergraphics.h>
#include <proto/cybergraphics.h>

#include <SDI_compiler.h>

#include "amiga_scalers.h"

#define UWCOLORS

#define KB_A 0x20
#define KB_S 0x21
#define KB_Z 0x31
#define KB_X 0x32
#define KB_SPACE 0x40
#define KB_TAB 0x42
#define KB_ENTER 0x44
#define KB_ESC 0x45
#define KB_DEL 0x46
#define KB_UP 0x4C
#define KB_DOWN 0x4D
#define KB_RIGHT 0x4E
#define KB_LEFT 0x4F
#define KB_HELP 0x5F
#define KB_F10 0x59
#define KB_LSHIFT 0x60
#define KB_RSHIFT 0x61
#define KB_CTRL 0x63
#define KB_LALT 0x64
#define KB_RALT 0x65

extern "C" void ASM c2p1x1_8_c5_bm(REG(d0, WORD chunkyx), REG(d1, WORD chunkyy), REG(d2, WORD offsx), REG(d3, WORD offsy), REG(a0, APTR chunkyscreen), REG(a1, struct BitMap *bitmap));
extern "C" void ASM c2p1x1_8_c5_bm_040(REG(d0, WORD chunkyx), REG(d1, WORD chunkyy), REG(d2, WORD offsx), REG(d3, WORD offsy), REG(a0, APTR chunkyscreen), REG(a1, struct BitMap *bitmap));
extern "C" void ASM c2p1x1_6_c5_bm(REG(d0, WORD chunkyx), REG(d1, WORD chunkyy), REG(d2, WORD offsx), REG(d3, WORD offsy), REG(a0, APTR chunkyscreen), REG(a1, struct BitMap *bitmap));
extern "C" void ASM c2p1x1_5_c5_bm(REG(d0, WORD chunkyx), REG(d1, WORD chunkyy), REG(d2, WORD offsx), REG(d3, WORD offsy), REG(a0, APTR chunkyscreen), REG(a1, struct BitMap *bitmap));
extern "C" void ASM c2p1x1_4_c5_bm(REG(d0, WORD chunkyx), REG(d1, WORD chunkyy), REG(d2, WORD offsx), REG(d3, WORD offsy), REG(a0, APTR chunkyscreen), REG(a1, struct BitMap *bitmap));

static struct Window *window = NULL;
static struct Screen *screen = NULL;
static UWORD *pointermem = NULL;
struct Library *CyberGfxBase = NULL;
static int currentBitMap;
static struct ScreenBuffer *sbuf[2] = {NULL, NULL};
static ULONG spal[1 + (256 * 3) + 1];
uint8 gFillmapData[256 * 32];
static int updatePalette = 0;
static int paletteHWDirty = 0;
static int use_c2p = 0;
extern int bgNeedsRemap;

static int displayWidth = FRAME_WIDTH;
static int displayHeight = FRAME_HEIGHT;
static int displayOffsetX = 0;
static int displayOffsetY = 0;
static int filterMode = FILTER_NONE;
static uint8 *scaledBuffer = NULL;

static int c2p_use_040 = 0;
static uint8 gLastGoodMode = VMODE_32;

int32 gAudioBackend = SND_BACKEND_AUTO;
int32 gAudioStereo = 0;
int32 gAudioFreq = 11025;

uint8 gHwChipset = CHIPSET_OCS;
uint8 gHwRTG = 0;
uint8 gHwAHI = 0;
uint8 gHwRTGActive = 0;
uint8 gHwMonitorPAL = 0;
uint8 gHwMonitorNTSC = 0;

bool sndAhiDetect();

static ULONG timecount;
static struct EClockVal timeval;
int32 g_timer;
int32 fps;
int32 frameIndex = 0;
int32 fpsCounter = 0;

static uint8 paletteColor[768];

#ifdef UWCOLORS
static uint8 paletteWater[768];
#endif

const void *TITLE_SCR = NULL;
const void *levelData = NULL;

static APTR timerIntHandle = NULL;
static APTR keyIntHandle = NULL;
bool keyState[128];

static void checkPalette(void);
void setVideoMode(void);
void setTextMode(void);

static struct TagItem vctl[] =
    {
        {VC_IntermediateCLUpdate, FALSE},
        {VTAG_END_CM, 0}};

static struct Screen *openGameScreen(ULONG modeID, int depth)
{
    return OpenScreenTags(0,
                          modeID != (ULONG)INVALID_ID ? SA_DisplayID : TAG_IGNORE, modeID,
                          SA_Depth, (ULONG)depth,
                          SA_ShowTitle, FALSE,
                          SA_Quiet, TRUE,
                          SA_Draggable, FALSE,
                          SA_Type, CUSTOMSCREEN,
                          SA_VideoControl, (ULONG)vctl,
                          TAG_DONE);
}

static struct Window *openGameWindow(void)
{
    return OpenWindowTags(0,
                          WA_InnerWidth, (ULONG)screen->Width,
                          WA_InnerHeight, (ULONG)screen->Height,
                          WA_Flags, WFLG_ACTIVATE | WFLG_RMBTRAP | WFLG_BACKDROP | WFLG_BORDERLESS,
                          WA_CustomScreen, (ULONG)screen,
                          TAG_DONE);
}

static ULONG bestMode(ULONG width, ULONG height, ULONG depth)
{
    return BestModeID(
        BIDTAG_NominalWidth, width,
        BIDTAG_NominalHeight, height,
        BIDTAG_Depth, depth,
        TAG_DONE);
}

static ULONG bestModeWithMonitor(ULONG width, ULONG height, ULONG depth, ULONG monitor)
{
    return BestModeID(
        BIDTAG_NominalWidth, width,
        BIDTAG_NominalHeight, height,
        BIDTAG_Depth, depth,
        BIDTAG_MonitorID, monitor,
        TAG_DONE);
}

static ULONG bestCMode(ULONG width, ULONG height)
{
    return BestCModeIDTags(
        CYBRBIDTG_Depth, 8,
        CYBRBIDTG_NominalWidth, (ULONG)width,
        CYBRBIDTG_NominalHeight, (ULONG)height,
        TAG_DONE);
}

static void closeWindowAndScreen(void)
{
    if (window)
    {
        CloseWindow(window);
        window = NULL;
    }

    if (screen)
    {
        CloseScreen(screen);
        screen = NULL;
    }
}

static void c2p_write_bm(WORD chunkyx, WORD chunkyy, WORD offsx, WORD offsy, APTR chunkyscreen, struct BitMap *bitmap)
{
    register WORD r_d0 __asm("d0") = chunkyx;
    register WORD r_d1 __asm("d1") = chunkyy;
    register WORD r_d2 __asm("d2") = offsx;
    register WORD r_d3 __asm("d3") = offsy;
    register APTR r_a0 __asm("a0") = chunkyscreen;
    register struct BitMap *r_a1 __asm("a1") = bitmap;

    if (c2p_use_040)
        c2p1x1_8_c5_bm_040(r_d0, r_d1, r_d2, r_d3, r_a0, r_a1);
    else
        c2p1x1_8_c5_bm(r_d0, r_d1, r_d2, r_d3, r_a0, r_a1);
}

void hwDetect()
{
    struct Library *cgfx;

    if ((GfxBase->ChipRevBits0 & SETCHIPREV_AA) == SETCHIPREV_AA)
        gHwChipset = CHIPSET_AGA;
    else if ((GfxBase->ChipRevBits0 & SETCHIPREV_ECS) == SETCHIPREV_ECS)
        gHwChipset = CHIPSET_ECS;
    else
        gHwChipset = CHIPSET_OCS;

    cgfx = OpenLibrary((STRPTR) "cybergraphics.library", 41);

    if (cgfx)
        CloseLibrary(cgfx);

    gHwRTG = (cgfx != NULL);
    gHwAHI = sndAhiDetect();

    gHwMonitorPAL = (bestModeWithMonitor(FRAME_WIDTH, FRAME_HEIGHT, 5, PAL_MONITOR_ID) != (ULONG)INVALID_ID);
    gHwMonitorNTSC = (bestModeWithMonitor(FRAME_WIDTH, FRAME_HEIGHT, 5, NTSC_MONITOR_ID) != (ULONG)INVALID_ID);
}

void applyVideoSettings()
{
    switch (gSettings.video_mode)
    {
    case VMODE_16:
        ecsDepth = 4;
        break;
    case VMODE_32:
        ecsDepth = 5;
        break;
    case VMODE_64:
        ecsDepth = (gHwChipset >= CHIPSET_ECS) ? 6 : 5;
        break;
    case VMODE_AGA:
        ecsDepth = (gHwChipset == CHIPSET_AGA) ? 8 : 5;
        break;
    case VMODE_RTG:
        ecsDepth = gHwRTG ? 0 : ((gHwChipset == CHIPSET_AGA) ? 8 : 5);
        break;
    default:
        ecsDepth = (gHwChipset == CHIPSET_AGA) ? 8 : 5;
        break;
    }

    numColors = ecsDepth ? (1 << ecsDepth) : 256;
    quantMethod = gSettings.video_quant ? QUANT_WU : QUANT_LLOYD3D;
    filterMode = gSettings.video_filter ? FILTER_NEAREST : FILTER_NONE;

    for (int i = 0; i < 256; i++)
    {
        ecsRemap[i] = i;
        gInvQuant[i] = i;
    }

    gFillmap = gLightmap;
}

void applyAudioSettings()
{
    static const int32 audioFreqTable[3] = {11025, 22050, 44100};

    gAudioBackend = X_MIN((int32)gSettings.audio_backend, SND_BACKEND_AHI);
    gAudioStereo = gSettings.audio_stereo != 0;

    int32 effBackend = gAudioBackend;

    if (effBackend == SND_BACKEND_AUTO)
        effBackend = gHwAHI ? SND_BACKEND_AHI : SND_BACKEND_PAULA;

    int32 maxFreq = (effBackend == SND_BACKEND_AHI && gAhiMaxFreq >= 44100) ? 2 : 1;

    gAudioFreq = audioFreqTable[X_MIN((int32)gSettings.audio_freq, maxFreq)];
}

void videoReopen()
{
    setTextMode();
    setVideoMode();

    if (!screen && gSettings.video_monitor != VMON_AUTO)
    {
        gSettings.video_monitor = VMON_AUTO;
        setVideoMode();
    }

    if (!screen && gSettings.video_mode != VMODE_32)
    {
        gSettings.video_mode = VMODE_32;
        setVideoMode();
    }

    if (!screen && gSettings.video_mode != gLastGoodMode)
    {
        gSettings.video_mode = gLastGoodMode;
        setVideoMode();
    }

    gHwRTGActive = (CyberGfxBase != NULL);

    if (!ecsDepth)
        c2p_use_040 = (SysBase->AttnFlags & (AFF_68040 | AFF_68060)) ? 1 : 0;

    memset(ecsPalette, 0, sizeof(ecsPalette));

    for (int i = 0; i < 256; i++)
        ecsRemap[i] = i;

    if (gLightmap)
        memcpy(gLightmap, origLightmap, 256 * 32);

    gFillmap = gLightmap;

    lightmapNeedsSave = 1;
    bgNeedsRemap = 1;
    updatePalette = 1;
}

void videoRefreshPalette()
{
    applyVideoSettings();
    updatePalette = 1;
}

void audioReinit()
{
    applyAudioSettings();
    sndFree();
    sndStop();
    sndInit();
}

static void closeCyberGfxBase(void)
{
    if (CyberGfxBase)
    {
        CloseLibrary(CyberGfxBase);
        CyberGfxBase = NULL;
    }
}

static void freeScreenBuffers(void)
{
    if (sbuf[1])
    {
        FreeScreenBuffer(screen, sbuf[1]);
        sbuf[1] = NULL;
    }

    if (sbuf[0])
    {
        FreeScreenBuffer(screen, sbuf[0]);
        sbuf[0] = NULL;
    }
}

void setVideoMode()
{
    ULONG modeID = INVALID_ID;

    displayWidth = FRAME_WIDTH;
    displayHeight = FRAME_HEIGHT;
    filterMode = FILTER_NONE;

    applyVideoSettings();

    gHwRTGActive = 0;
    use_c2p = FALSE;

    if (ecsDepth)
    {
        ULONG monitorID;

        if (gSettings.video_monitor == VMON_PAL)
            monitorID = PAL_MONITOR_ID;
        else if (gSettings.video_monitor == VMON_NTSC)
            monitorID = NTSC_MONITOR_ID;
        else if (gHwMonitorPAL)
            monitorID = PAL_MONITOR_ID;
        else
            monitorID = NTSC_MONITOR_ID;

        modeID = bestModeWithMonitor(FRAME_WIDTH, FRAME_HEIGHT, ecsDepth, monitorID);
    }
    else
    {
        CyberGfxBase = OpenLibrary((STRPTR) "cybergraphics.library", 41);

        if (filterMode != FILTER_NONE)
        {
            displayWidth = FRAME_WIDTH * 2;
            displayHeight = FRAME_HEIGHT * 2;
        }

        if (CyberGfxBase)
            modeID = bestCMode((ULONG)displayWidth, (ULONG)displayHeight);

        if (modeID == (ULONG)INVALID_ID)
            modeID = bestMode((ULONG)displayWidth, (ULONG)displayHeight, 8);

        if (modeID == (ULONG)INVALID_ID)
        {
            displayWidth = FRAME_WIDTH;
            displayHeight = FRAME_HEIGHT;
            filterMode = FILTER_NONE;

            if (CyberGfxBase)
                modeID = bestCMode(FRAME_WIDTH, FRAME_HEIGHT);

            if (modeID == (ULONG)INVALID_ID)
                modeID = bestMode(FRAME_WIDTH, FRAME_HEIGHT, 8);
        }
    }

    screen = openGameScreen(modeID, ecsDepth ? ecsDepth : 8);

    if (!screen)
    {
        if (ecsDepth != 5)
        {
            closeCyberGfxBase();

            ecsDepth = 5;
            numColors = 32;

            modeID = bestMode(FRAME_WIDTH, FRAME_HEIGHT, ecsDepth);
            screen = openGameScreen(modeID, ecsDepth);
        }

        if (!screen)
        {
            closeCyberGfxBase();

            return;
        }
    }

    displayWidth = screen->Width;
    displayHeight = screen->Height;
    displayOffsetX = 0;
    displayOffsetY = 0;

    if (filterMode != FILTER_NONE && (displayWidth != FRAME_WIDTH * 2 || displayHeight != FRAME_HEIGHT * 2))
        filterMode = FILTER_NONE;

    if (scaledBuffer)
    {
        FreeVec(scaledBuffer);
        scaledBuffer = NULL;
    }

    if (ecsDepth && ecsDepth < 8)
        LoadRGB4(&screen->ViewPort, ecsPalette, 1 << ecsDepth);

    window = openGameWindow();

    if (!window)
    {
        closeWindowAndScreen();
        closeCyberGfxBase();

        return;
    }

    if ((GetBitMapAttr(screen->RastPort.BitMap, BMA_FLAGS) & BMF_STANDARD))
    {
        sbuf[0] = AllocScreenBuffer(screen, 0, SB_SCREEN_BITMAP);
        sbuf[1] = AllocScreenBuffer(screen, 0, SB_COPY_BITMAP);

        if (!sbuf[0] || !sbuf[1])
        {
            freeScreenBuffers();
            closeWindowAndScreen();
            closeCyberGfxBase();

            return;
        }

        use_c2p = TRUE;
    }

    if (!use_c2p && ecsDepth)
    {
        int32 fallbackDepth = 0;

        if (ecsDepth == 5)
            fallbackDepth = 4;
        else if (ecsDepth == 4)
            fallbackDepth = (gHwChipset == CHIPSET_AGA) ? 8 : 5;
        else if (ecsDepth == 6 || ecsDepth == 8)
            fallbackDepth = 5;

        if (fallbackDepth && fallbackDepth != ecsDepth)
        {
            closeWindowAndScreen();

            ecsDepth = fallbackDepth;
            numColors = 1 << ecsDepth;

            modeID = bestMode(FRAME_WIDTH, FRAME_HEIGHT, ecsDepth);
            screen = openGameScreen(modeID, ecsDepth);

            if (screen)
            {
                displayWidth = screen->Width;
                displayHeight = screen->Height;

                if ((GetBitMapAttr(screen->RastPort.BitMap, BMA_FLAGS) & BMF_STANDARD))
                {
                    sbuf[0] = AllocScreenBuffer(screen, 0, SB_SCREEN_BITMAP);
                    sbuf[1] = AllocScreenBuffer(screen, 0, SB_COPY_BITMAP);

                    if (sbuf[0] && sbuf[1])
                        use_c2p = TRUE;
                    else
                        freeScreenBuffers();
                }

                if (use_c2p)
                {
                    window = openGameWindow();

                    if (!window)
                    {
                        freeScreenBuffers();
                        closeWindowAndScreen();
                        use_c2p = FALSE;
                    }
                }
                else
                {
                    closeWindowAndScreen();
                }
            }
        }
    }

    if (!screen || !window)
        return;

    if (!use_c2p && ecsDepth)
    {
        closeWindowAndScreen();
        closeCyberGfxBase();

        return;
    }

    if (!use_c2p)
    {
        if (filterMode != FILTER_NONE)
            scaledBuffer = (uint8 *)AllocVec(displayWidth * displayHeight, MEMF_ANY | MEMF_CLEAR);

        if (!scaledBuffer)
        {
            displayWidth = FRAME_WIDTH;
            displayHeight = FRAME_HEIGHT;
            displayOffsetX = (screen->Width - FRAME_WIDTH) / 2;
            displayOffsetY = (screen->Height - FRAME_HEIGHT) / 2;
            filterMode = FILTER_NONE;
        }
    }

    pointermem = (UWORD *)AllocVec(2 * 6, MEMF_CHIP | MEMF_CLEAR);

    if (pointermem && window->Pointer != pointermem)
        SetPointer(window, pointermem, 1, 1, 0, 0);

    gHwRTGActive = (CyberGfxBase != NULL);
    gLastGoodMode = gSettings.video_mode;
}

void setTextMode()
{
    if (use_c2p && screen && sbuf[0] && sbuf[1])
    {
        ChangeScreenBuffer(screen, sbuf[0]);
        WaitTOF();

        ChangeScreenBuffer(screen, sbuf[0]);
        WaitTOF();
    }

    freeScreenBuffers();
    closeWindowAndScreen();

    if (pointermem)
    {
        FreeVec(pointermem);
        pointermem = NULL;
    }

    if (scaledBuffer)
    {
        FreeVec(scaledBuffer);
        scaledBuffer = NULL;
    }

    closeCyberGfxBase();
}

void osSetPalette(const uint16 *palette)
{
    uint8 *ppc = paletteColor;
    uint8 *ppw = paletteWater;

    for (int32 i = 0; i < 256; i++)
    {
        uint16 p = *palette++;
        uint8 r = (p & 31) << 3;
        uint8 g = ((p >> 5) & 31) << 3;
        uint8 b = ((p >> 10) & 31) << 3;

        *ppc++ = r;
        *ppc++ = g;
        *ppc++ = b;

#ifdef UWCOLORS
        *ppw++ = ((uint16)r * 170) >> 8;
        *ppw++ = ((uint16)g * 170) >> 8;
        *ppw++ = b;
#endif
    }

    updatePalette = 1;

#ifndef UWCOLORS
    paletteCurrent = paletteColor;
#endif
}

void timerISR()
{
    frameIndex++;
}

void videoAcquire()
{
    setVideoMode();

    if (screen)
    {
        updatePalette = 1;
        lightmapNeedsSave = 1;
        bgNeedsRemap = 1;
    }

    timerIntHandle = AddTimerInt((APTR)timerISR, NULL);

    if (timerIntHandle != NULL)
        StartTimerInt(timerIntHandle, (1000 * 1000) / 60, TRUE);
}

void videoRelease()
{
    if (timerIntHandle != NULL)
    {
        StopTimerInt(timerIntHandle);
        RemTimerInt(timerIntHandle);

        timerIntHandle = NULL;
    }

    setTextMode();
}

void waitVBlank()
{
    WaitTOF();
}

static void loadPaletteHW()
{
    if (!paletteHWDirty || !screen)
        return;

    if (paletteHWDirty == 1)
        LoadRGB4(&screen->ViewPort, ecsPalette, 1 << ecsDepth);
    else
        LoadRGB32(&screen->ViewPort, spal);

    paletteHWDirty = 0;
}

void updatePaletteNow()
{
    if (!updatePalette || !screen)
        return;

    gFillmap = gLightmap;

    if (ecsDepth == 8)
    {
        if (lightmapNeedsSave)
        {
            memcpy(origLightmap, gLightmap, 256 * 32);
            lightmapNeedsSave = 0;
        }

        ULONG *sp = spal;
        uint8 *pp = paletteCurrent;

        *sp++ = 256 << 16;

        for (int32 i = 0; i < 256; i++)
        {
            uint8 r = *pp++;
            uint8 g = *pp++;
            uint8 b = *pp++;

            *sp++ = (ULONG)r << 24;
            *sp++ = (ULONG)g << 24;
            *sp++ = (ULONG)b << 24;
        }

        *sp = 0;

        for (int32 i = 0; i < 256; i++)
            ecsRemap[i] = i;

        memcpy(gLightmap, origLightmap, 256 * 32);

        paletteHWDirty = 2;
    }
    else if (ecsDepth)
    {
        int16_t colorWeight[256];

        if (lightmapNeedsSave)
        {
            memcpy(origLightmap, gLightmap, 256 * 32);

            lightmapNeedsSave = 0;
        }

        ecsComputeColorWeights(origLightmap, level.tiles, level.tilesCount, colorWeight);

        if (quantMethod == QUANT_WU)
            ecsWuQuant(paletteCurrent, colorWeight, ecsPalette);
        else if (quantMethod == QUANT_LLOYD3D)
            ecsLloyd3DQuant(paletteCurrent, colorWeight, ecsPalette);
        else
            ecsLloydQuant(paletteCurrent, colorWeight, ecsPalette);

        ecsBuildRemap(paletteCurrent, ecsRemap);

        paletteHWDirty = 1;

        ecsRemapLightmap(ecsDepth, origLightmap, gLightmap, ecsRemap);

        ecsbuildFillmap();
    }
    else
    {
        ULONG *sp = spal;
        uint8 *pp = paletteCurrent;

        *sp++ = 256 << 16;

        for (int32 i = 0; i < 256; i++)
        {
            uint8 r = *pp++;
            uint8 g = *pp++;
            uint8 b = *pp++;

            *sp++ = (ULONG)r << 24;
            *sp++ = (ULONG)g << 24;
            *sp++ = (ULONG)b << 24;
        }

        *sp = 0;

        paletteHWDirty = 2;
    }

    updatePalette = 0;
}

void blit()
{
    if (screen && window && screen == IntuitionBase->FirstScreen && !(window->Flags & WFLG_WINDOWACTIVE))
        ActivateWindow(window);

    if (!screen || !window)
        return;

    if (use_c2p)
    {
        currentBitMap ^= 1;

        if (ecsDepth == 4)
            c2p1x1_4_c5_bm(FRAME_WIDTH, FRAME_HEIGHT, 0, 0, fb, sbuf[currentBitMap]->sb_BitMap);
        else if (ecsDepth == 5)
            c2p1x1_5_c5_bm(FRAME_WIDTH, FRAME_HEIGHT, 0, 0, fb, sbuf[currentBitMap]->sb_BitMap);
        else if (ecsDepth == 6)
            c2p1x1_6_c5_bm(FRAME_WIDTH, FRAME_HEIGHT, 0, 0, fb, sbuf[currentBitMap]->sb_BitMap);
        else
            c2p_write_bm(FRAME_WIDTH, FRAME_HEIGHT, 0, 0, fb, sbuf[currentBitMap]->sb_BitMap);

        ChangeScreenBuffer(screen, sbuf[currentBitMap]);
    }
    else if (CyberGfxBase)
    {
        if (scaledBuffer)
        {
            if (filterMode == FILTER_SCALE2X && displayWidth == FRAME_WIDTH * 2 && displayHeight == FRAME_HEIGHT * 2)
                scale2x(fb, scaledBuffer, displayWidth);
            else if (displayWidth == FRAME_WIDTH * 2 && displayHeight == FRAME_HEIGHT * 2)
                scaleNearest2x(fb, scaledBuffer, displayWidth);

            WritePixelArray(scaledBuffer, 0, 0, displayWidth, window->RPort, displayOffsetX, displayOffsetY, displayWidth, displayHeight, RECTFMT_LUT8);
        }
        else
        {
            WritePixelArray(fb, 0, 0, FRAME_WIDTH, window->RPort, displayOffsetX, displayOffsetY, FRAME_WIDTH, FRAME_HEIGHT, RECTFMT_LUT8);
        }
    }
}

static void checkPalette(void)
{
    const ItemObj *lara = players[0];

#ifdef UWCOLORS
    const Room *camRoom = NULL;
    uint8 *palette = NULL;
#endif

    if (!lara)
    {
        paletteCurrent = paletteColor;
        return;
    }

#ifdef UWCOLORS
    camRoom = lara->extraL->camera.view.room;
    palette = (ROOM_FLAG_WATER(camRoom->info->flags) && inventory.state == INV_STATE_NONE) ? paletteWater : paletteColor;

    if (paletteCurrent != palette)
    {
        updatePalette = 1;
        paletteCurrent = palette;
    }
#endif
}

void keyISR(REG(d0, ULONG scancode))
{
    keyState[scancode & 0x7F] = ((scancode & 0x80) == 0);
}

void inputAcquire()
{
    keyIntHandle = AddKBInt((APTR)keyISR, NULL);
}

void inputRelease()
{
    if (keyIntHandle != NULL)
    {
        RemKBInt(keyIntHandle);
        keyIntHandle = NULL;
    }
}

void inputUpdate()
{
    ULONG portState;

    keys = 0;

    if (keyState[KB_UP])
        keys |= IK_UP;

    if (keyState[KB_RIGHT])
        keys |= IK_RIGHT;

    if (keyState[KB_DOWN])
        keys |= IK_DOWN;

    if (keyState[KB_LEFT])
        keys |= IK_LEFT;

    if (keyState[KB_CTRL])
        keys |= IK_A;

    if (keyState[KB_DEL])
        keys |= IK_B;

    if (keyState[KB_LALT])
        keys |= IK_X;

    if (keyState[KB_RALT])
        keys |= IK_X;

    if (keyState[KB_HELP])
        keys |= IK_L;

    if (keyState[KB_LSHIFT])
        keys |= IK_R;

    if (keyState[KB_RSHIFT])
        keys |= IK_R;

    if (keyState[KB_SPACE])
        keys |= IK_Y;

    if (keyState[KB_ESC])
        keys |= IK_SELECT;

    portState = ReadJoyPort(1);

    if (portState)
    {
        if (portState & JPF_JOY_UP)
            keys |= IK_UP;

        if (portState & JPF_JOY_DOWN)
            keys |= IK_DOWN;

        if (portState & JPF_JOY_RIGHT)
            keys |= IK_RIGHT;

        if (portState & JPF_JOY_LEFT)
            keys |= IK_LEFT;

        if (portState & JPF_BUTTON_REVERSE)
            keys |= IK_L;

        if (portState & JPF_BUTTON_FORWARD)
            keys |= IK_R;

        if (portState & JPF_BUTTON_GREEN)
            keys |= IK_X;

        if (portState & JPF_BUTTON_BLUE)
            keys |= IK_B;

        if (portState & JPF_BUTTON_YELLOW)
            keys |= IK_Y;

        if (portState & JPF_BUTTON_RED)
            keys |= IK_A;

        if (portState & JPF_BUTTON_PLAY)
            keys |= IK_SELECT;
    }
}

int32 osGetSystemTimeMS()
{
    ULONG delta = ElapsedTime(&timeval);

    timecount += (delta * 1000) >> 16;

    return timecount;
}

bool osSaveSettings()
{
    BPTR f = Open("settings.dat", MODE_NEWFILE);

    if (!f)
        return false;

    Write(f, &gSettings, sizeof(gSettings));

    Close(f);

    return true;
}

bool osLoadSettings()
{
    BPTR f = Open("settings.dat", MODE_OLDFILE);
    uint8 version;

    if (!f)
        return false;

    Read(f, &version, 1);

    if (version != gSettings.version)
    {
        Close(f);
        return false;
    }

    Read(f, (uint8 *)&gSettings + 1, sizeof(gSettings) - 1);

    Close(f);

    return true;
}

bool osCheckSave()
{
    BPTR f = Open("savegame.dat", MODE_OLDFILE);

    if (!f)
        return false;

    Close(f);

    return true;
}

bool osSaveGame()
{
    BPTR f = Open("savegame.dat", MODE_NEWFILE);

    if (!f)
        return false;

    Write(f, &gSaveGame, sizeof(gSaveGame));
    Write(f, &gSaveData, gSaveGame.dataSize);

    Close(f);

    return true;
}

bool osLoadGame()
{
    BPTR f = Open("savegame.dat", MODE_OLDFILE);
    uint32 version;

    if (!f)
        return false;

    Read(f, &version, sizeof(version));

    if (SAVEGAME_VER != version)
    {
        Close(f);
        return false;
    }

    Read(f, &gSaveGame.dataSize, sizeof(gSaveGame) - sizeof(version));
    Read(f, &gSaveData, gSaveGame.dataSize);

    Close(f);

    return true;
}

void osJoyVibrate(int32 index, int32 L, int32 R) {}

const void *osLoadScreen(LevelID id)
{
    if (!TITLE_SCR)
    {
        uint8 *data = new uint8[FRAME_WIDTH * FRAME_HEIGHT];
        BPTR f;

        if (!data)
            return NULL;

        memset(data, 0, FRAME_WIDTH * FRAME_HEIGHT);

        f = Open("data/TITLE.SCR", MODE_OLDFILE);

        if (f)
        {
            Read(f, data, FRAME_WIDTH * FRAME_HEIGHT);
            Close(f);
        }

        if (ecsDepth)
            bgNeedsRemap = 1;

        TITLE_SCR = data;
    }

    if (ecsDepth)
        bgNeedsRemap = 1;

    return TITLE_SCR;
}

const void *osLoadLevel(LevelID id)
{
    char buf[32];
    BPTR f;
    int32 size;
    uint8 *data = NULL;

    if (levelData)
        delete[] (uint8 *)levelData;

    if (TITLE_SCR)
    {
        delete[] (uint8 *)TITLE_SCR;
        TITLE_SCR = NULL;
    }

    strcpy(buf, "data/");
    strcat(buf, (const char *)gLevelInfo[id].data);

#if (USE_FMT & LVL_FMT_PKD)
    strcat(buf, ".PKD");
#endif

#if (USE_FMT & LVL_FMT_PHD)
    strcat(buf, ".PHD");
#endif

#if (USE_FMT & LVL_FMT_PSX)
    strcat(buf, ".PSX");
#endif

    f = Open(buf, MODE_OLDFILE);

    if (!f)
        return NULL;

    Seek(f, 0, OFFSET_END);
    size = Seek(f, 0, OFFSET_CURRENT);
    Seek(f, 0, OFFSET_BEGINNING);

    data = new uint8[size];

    if (!data)
    {
        Close(f);
        return NULL;
    }

    Read(f, data, size);

    Close(f);

    levelData = data;
    lightmapNeedsSave = 1;

    return (void *)levelData;
}

APTR AllocMemAligned(ULONG byteSize, ULONG attributes, ULONG alignSize, ULONG alignOffset)
{
    ULONG totalSize = byteSize + alignSize - 1 + alignOffset;
    APTR finalMem = NULL;
    APTR rawMem = AllocMem(totalSize, attributes);

    if (rawMem)
    {
        APTR aligned_mem;

        Forbid();

        aligned_mem = (APTR)((((ULONG)rawMem + alignSize - 1) & ~(alignSize - 1)) - alignOffset);

        FreeMem(rawMem, totalSize);

        finalMem = AllocAbs(byteSize, aligned_mem);

        Permit();
    }

    return finalMem;
}

int main(void)
{
    int32 lastFrameIndex = -1;

    GfxBase = (struct GfxBase *)OpenLibrary((STRPTR) "graphics.library", 0);
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR) "intuition.library", 0);

    if (!GfxBase || !IntuitionBase)
        return 0;

    hwDetect();
    initSettings();

    videoAcquire();

    if (!screen || !window)
    {
        inputRelease();
        videoRelease();
        return 0;
    }

    inputAcquire();

    timecount = 0;

    ElapsedTime(&timeval);

#ifdef ALIGNED_LIGHTMAP
    gLightmap = (uint8 *)AllocMemAligned(256 * 32, MEMF_ANY, 0x10000, 0);
#else
    gLightmap = (uint8 *)AllocMem(256 * 32, MEMF_ANY);
#endif

    gFillmap = gLightmap;

    if (!gLightmap)
    {
        inputRelease();
        videoRelease();
        return 0;
    }

    memset(gLightmap, 0, 256 * 32);

    if (!ecsDepth)
        c2p_use_040 = (SysBase->AttnFlags & AFF_68040) ? 1 : (SysBase->AttnFlags & AFF_68060) ? 1
                                                                                              : 0;

    uint8 *pp = paletteColor;

    for (int32 i = 0; i < 256; i++)
    {
        *pp++ = i;
        *pp++ = i;
        *pp++ = i;
    }

    paletteCurrent = paletteColor;
    updatePalette = 1;

    applyAudioSettings();
    
    sndInit();

    // gLevelID = LVL_TR1_1;
    gLevelID = LVL_TR1_TITLE;
    // gLevelID = LVL_TR1_CUT_1;
    // gLevelID = LVL_TR1_GYM;

    gameInit();

    while (1)
    {
        int32 frame = frameIndex / 2;
        int32 delta = frame - lastFrameIndex;

        inputUpdate();

        if (keyState[KB_F10])
            break;

#ifdef NO_FPS_CAP
        if (delta)
        {
            lastFrameIndex = frame;

            gameUpdate(delta);
        }
        else if (gSettings.video_vsync)
            continue;
#else

        if (!delta)
            continue;

        lastFrameIndex = frame;

        gameUpdate(delta);
#endif

        checkPalette();

        updatePaletteNow();

#ifdef PROFILING
        waitVBlank();
#else
        if (gSettings.video_vsync)
            waitVBlank();
#endif

        loadPaletteHW();

        gameRender();

        fpsCounter++;

        if (frameIndex >= 60)
        {
            frameIndex -= 60;
            lastFrameIndex -= 30;

            fps = fpsCounter;

            fpsCounter = 0;
        }

        blit();
    }

    extern void sndFree();
    sndFree();

    if (gLightmap)
    {
        FreeMem(gLightmap, 256 * 32);
        gLightmap = NULL;
    }

    inputRelease();

    videoRelease();

    if (GfxBase)
    {
        CloseLibrary((struct Library *)GfxBase);
        GfxBase = NULL;
    }

    if (IntuitionBase)
    {
        CloseLibrary((struct Library *)IntuitionBase);
        IntuitionBase = NULL;
    }

    return 0;
}

#define abs _abs_
#define itoa _itoa_
#include <stdlib.h>

void *operator new(size_t size)
{
    void *ptr = malloc(size);
    return ptr;
}

void *operator new[](size_t size)
{
    void *ptr = malloc(size);
    return ptr;
}

void operator delete(void *ptr) noexcept
{
    free(ptr);
}

void operator delete[](void *ptr) noexcept
{
    free(ptr);
}

void operator delete(void *ptr, size_t size) noexcept
{
    free(ptr);
}

void operator delete[](void *ptr, size_t size) noexcept
{
    free(ptr);
}
