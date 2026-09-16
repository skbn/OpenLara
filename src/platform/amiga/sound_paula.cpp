#include "common.h"
#include "sound_int.h"

#include <devices/audio.h>
#include <hardware/custom.h>
#include <hardware/intbits.h>
#include <hardware/dmabits.h>
#include <hardware/cia.h>
#include <graphics/gfxbase.h>
#include <proto/graphics.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <SDI_interrupt.h>

static struct CIA *ciaa = (struct CIA *)0xbfe001;
static struct Custom *custom = (struct Custom *)0xdff000;

static struct Interrupt *oldIntAud0 = NULL;
static struct Interrupt *oldIntAud2 = NULL;
static struct MsgPort *audioMP = NULL;
static struct IOAudio *audioIO = NULL;
static BYTE audioDev = -1;
static UBYTE oldFilter = 0;
static struct Interrupt AudioInt;

static int8 *s_sfxChip = NULL;
static int8 *s_musChip = NULL;
static int8 *s_sfxStage = NULL;
static int8 *s_musStage = NULL;

static uint32 curSoundBuffer = 0;
static uint32 curMusicBuffer = 0;

static void DeInterleave(int8 *dstL, int8 *dstR, const int8 *src, int32 count)
{
    do
    {
        *dstL++ = *src++;
        *dstR++ = *src++;
    } while (--count);
}

INTERRUPTPROTO(AudioFunc, ULONG, struct Custom *c, APTR data)
{
    UWORD intreqr = c->intreqr;

    if (intreqr & INTF_AUD0)
    {
        // SFX
        if (!sndStereo)
        {
            curSoundBuffer ^= SND_SAMPLES;
            int8 *bufL = s_sfxChip + curSoundBuffer;

            c->aud[0].ac_ptr = (UWORD *)bufL;
            c->aud[1].ac_ptr = (UWORD *)bufL;

            sndFill(bufL);
        }
        else
        {
            curSoundBuffer ^= 2 * SND_SAMPLES;
            int8 *bufL = s_sfxChip + curSoundBuffer;
            int8 *bufR = bufL + SND_SAMPLES;

            c->aud[0].ac_ptr = (UWORD *)bufL;
            c->aud[1].ac_ptr = (UWORD *)bufR;

            sndFill(s_sfxStage);

            DeInterleave(bufL, bufR, s_sfxStage, SND_SAMPLES);
        }

        c->intreq = INTF_AUD0;
    }

    if (intreqr & INTF_AUD2)
    {
        // Music
        if (!sndStereo)
        {
            curMusicBuffer ^= SND_SAMPLES;
            int8 *bufL = s_musChip + curMusicBuffer;

            c->aud[2].ac_ptr = (UWORD *)bufL;
            c->aud[3].ac_ptr = (UWORD *)bufL;

            sndFillMusic(bufL);
        }
        else
        {
            curMusicBuffer ^= 2 * SND_SAMPLES;
            int8 *bufL = s_musChip + curMusicBuffer;
            int8 *bufR = bufL + SND_SAMPLES;

            c->aud[2].ac_ptr = (UWORD *)bufR;
            c->aud[3].ac_ptr = (UWORD *)bufL;

            sndFillMusic(s_musStage);

            DeInterleave(bufL, bufR, s_musStage, SND_SAMPLES);
        }

        c->intreq = INTF_AUD2;
    }

    return 0;
}

void sndPaulaFree()
{
    struct Interrupt *restoreAud0 = NULL;
    struct Interrupt *restoreAud2 = NULL;

    if (oldFilter)
    {
        ciaa->ciapra |= oldFilter;
        oldFilter = 0;
    }

    if (oldIntAud0)
    {
        custom->dmacon = DMAF_AUD0 | DMAF_AUD1;
        custom->intena = INTF_AUD0;
        custom->intreq = INTF_AUD0;

        restoreAud0 = oldIntAud0;
        oldIntAud0 = NULL;
    }

    if (oldIntAud2)
    {
        custom->dmacon = DMAF_AUD2 | DMAF_AUD3;
        custom->intena = INTF_AUD2;
        custom->intreq = INTF_AUD2;

        restoreAud2 = oldIntAud2;
        oldIntAud2 = NULL;
    }

    if (!audioDev)
    {
        CloseDevice((struct IORequest *)audioIO);
        audioDev = -1;
    }

    if (restoreAud0)
        SetIntVector(INTB_AUD0, restoreAud0);

    if (restoreAud2)
        SetIntVector(INTB_AUD2, restoreAud2);

    if (audioIO)
    {
        DeleteIORequest((struct IORequest *)audioIO);
        audioIO = NULL;
    }

    if (audioMP)
    {
        DeleteMsgPort(audioMP);
        audioMP = NULL;
    }

    if (s_sfxChip)
    {
        FreeVec(s_sfxChip);
        s_sfxChip = NULL;
    }

    if (s_musChip)
    {
        FreeVec(s_musChip);
        s_musChip = NULL;
    }

    if (s_sfxStage)
    {
        FreeVec(s_sfxStage);
        s_sfxStage = NULL;
    }

    if (s_musStage)
    {
        FreeVec(s_musStage);
        s_musStage = NULL;
    }
}

static bool sndPaulaSetup()
{
    int32 stereo = sndStereo;
    int32 chipFrames = stereo ? 4 : 2;
    UWORD period;

    if (GfxBase->DisplayFlags & PAL)
        period = 3546895 / sndOutputFreq;
    else
        period = 3579545 / sndOutputFreq;

    s_sfxChip = (int8 *)AllocVec(chipFrames * SND_SAMPLES, MEMF_CHIP | MEMF_CLEAR | MEMF_PUBLIC);

    if (!s_sfxChip)
        return false;

    s_musChip = (int8 *)AllocVec(chipFrames * SND_SAMPLES, MEMF_CHIP | MEMF_CLEAR | MEMF_PUBLIC);

    if (!s_musChip)
        return false;

    if (stereo)
    {
        s_sfxStage = (int8 *)AllocVec(2 * SND_SAMPLES, MEMF_PUBLIC | MEMF_CLEAR);

        if (!s_sfxStage)
            return false;

        s_musStage = (int8 *)AllocVec(2 * SND_SAMPLES, MEMF_PUBLIC | MEMF_CLEAR);

        if (!s_musStage)
            return false;
    }

    AudioInt.is_Node.ln_Type = NT_INTERRUPT;
    AudioInt.is_Node.ln_Pri = 100;
    AudioInt.is_Node.ln_Name = (char *)"OpenLara Audio Interrupt";
    AudioInt.is_Code = (void (*)())AudioFunc;

    // disable the DMA and interrupts
    custom->dmacon = DMAF_AUD0 | DMAF_AUD1 | DMAF_AUD2 | DMAF_AUD3;
    custom->intena = INTF_AUD0 | INTF_AUD2;
    custom->intreq = INTF_AUD0 | INTF_AUD2;

    // SFX
    oldIntAud0 = SetIntVector(INTB_AUD0, &AudioInt);
    curSoundBuffer = 0;

    // left channel
    custom->aud[0].ac_len = SND_SAMPLES / sizeof(UWORD);
    custom->aud[0].ac_per = period;
    custom->aud[0].ac_vol = 64;
    custom->aud[0].ac_ptr = (UWORD *)s_sfxChip;

    // right channel
    custom->aud[1].ac_len = SND_SAMPLES / sizeof(UWORD);
    custom->aud[1].ac_per = period;
    custom->aud[1].ac_vol = 64;
    custom->aud[1].ac_ptr = (UWORD *)(s_sfxChip + (stereo ? SND_SAMPLES : 0));

    // Music
    oldIntAud2 = SetIntVector(INTB_AUD2, &AudioInt);
    curMusicBuffer = 0;

    // right channel
    custom->aud[2].ac_len = SND_SAMPLES / sizeof(UWORD);
    custom->aud[2].ac_per = period;
    custom->aud[2].ac_vol = 64;
    custom->aud[2].ac_ptr = (UWORD *)(s_musChip + (stereo ? SND_SAMPLES : 0));

    // left channel
    custom->aud[3].ac_len = SND_SAMPLES / sizeof(UWORD);
    custom->aud[3].ac_per = period;
    custom->aud[3].ac_vol = 64;
    custom->aud[3].ac_ptr = (UWORD *)s_musChip;

    // enable the DMA and interrupts
    custom->intena = INTF_SETCLR | INTF_AUD0 | INTF_AUD2;
    custom->dmacon = DMAF_SETCLR | DMAF_AUD0 | DMAF_AUD1 | DMAF_AUD2 | DMAF_AUD3;

    // turn on the low-pass filter
    oldFilter = ciaa->ciapra & CIAF_LED;
    ciaa->ciapra &= ~CIAF_LED;

    return true;
}

bool sndPaulaInit()
{
    bool ok = false;

    if ((audioMP = CreateMsgPort()))
    {
        if ((audioIO = (struct IOAudio *)CreateIORequest(audioMP, sizeof(struct IOAudio))))
        {
            UBYTE whichannel[] = {15};

            audioIO->ioa_Request.io_Message.mn_Node.ln_Pri = 127;
            audioIO->ioa_Request.io_Command = ADCMD_ALLOCATE;
            audioIO->ioa_Request.io_Flags = ADIOF_NOWAIT;
            audioIO->ioa_AllocKey = 0;
            audioIO->ioa_Data = whichannel;
            audioIO->ioa_Length = sizeof(whichannel);

            if (!(audioDev = OpenDevice((STRPTR)AUDIONAME, 0, (struct IORequest *)audioIO, 0)))
                ok = sndPaulaSetup();
        }
    }

    if (!ok)
        sndPaulaFree();

    return ok;
}
