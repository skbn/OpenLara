#include "common.h"
#include "sound_int.h"

#include <devices/ahi.h>
#include <proto/exec.h>
#include <proto/ahi.h>
#include <proto/dos.h>
#include <utility/hooks.h>
#include <SDI_hook.h>

#define AHI_CHANNELS 2

struct Library *AHIBase = NULL;
static struct MsgPort *AHImp = NULL;
static struct AHIRequest *AHIio = NULL;
static struct AHIAudioCtrl *actrl = NULL;
static struct Hook SoundHook;

int32 gAhiMaxFreq = 0;

static int8 *s_sfxBuf = NULL;
static int8 *s_musBuf = NULL;
static int32 s_curSfx = 0;
static int32 s_curMus = 0;

HOOKPROTO(SoundFunc, ULONG, struct AHIAudioCtrl *ctrl, struct AHISoundMessage *smsg)
{
    if (smsg->ahism_Channel == 0)
    {
        int8 *buffer = &s_sfxBuf[2 * SND_SAMPLES * s_curSfx];

        s_curSfx ^= 1;

        AHI_SetSound(0, s_curSfx, 0, 0, actrl, 0);

        sndFill(buffer);
    }
    else
    {
        int8 *buffer = &s_musBuf[2 * SND_SAMPLES * s_curMus];

        s_curMus ^= 1;

        AHI_SetSound(1, 2 | s_curMus, 0, 0, actrl, 0);

        sndFillMusic(buffer);
    }

    return 0;
}

void sndAhiFree()
{
    if (actrl)
    {
        AHI_ControlAudio(actrl, AHIC_Play, FALSE, TAG_END);
        AHI_FreeAudio(actrl);
        actrl = NULL;
    }

    if (s_sfxBuf)
    {
        FreeVec(s_sfxBuf);
        s_sfxBuf = NULL;
    }

    if (s_musBuf)
    {
        FreeVec(s_musBuf);
        s_musBuf = NULL;
    }

    if (AHIBase)
    {
        CloseDevice((struct IORequest *)AHIio);
        AHIBase = NULL;
    }

    if (AHIio)
    {
        DeleteIORequest((struct IORequest *)AHIio);
        AHIio = NULL;
    }

    if (AHImp)
    {
        DeleteMsgPort(AHImp);
        AHImp = NULL;
    }
}

static bool sndAhiSetup()
{
    struct AHISampleInfo sample[4];
    struct AHIEffMasterVolume vol = {AHIET_MASTERVOLUME, AHI_CHANNELS * 2 * 0x10000};

    AHI_SetEffect(&vol, actrl);

    sample[0].ahisi_Type = AHIST_S8S;
    sample[0].ahisi_Length = SND_SAMPLES;
    sample[0].ahisi_Address = s_sfxBuf;
    sample[1] = sample[0];
    sample[1].ahisi_Address = s_sfxBuf + 2 * SND_SAMPLES;

    sample[2].ahisi_Type = AHIST_S8S;
    sample[2].ahisi_Length = SND_SAMPLES;
    sample[2].ahisi_Address = s_musBuf;
    sample[3] = sample[2];
    sample[3].ahisi_Address = s_musBuf + 2 * SND_SAMPLES;

    if (AHI_LoadSound(0, AHIST_DYNAMICSAMPLE, &sample[0], actrl))
        return false;

    if (AHI_LoadSound(1, AHIST_DYNAMICSAMPLE, &sample[1], actrl))
        return false;

    if (AHI_LoadSound(2, AHIST_DYNAMICSAMPLE, &sample[2], actrl))
        return false;

    if (AHI_LoadSound(3, AHIST_DYNAMICSAMPLE, &sample[3], actrl))
        return false;

    s_curSfx = 0;
    s_curMus = 0;

    AHI_Play(actrl, AHIP_BeginChannel, 0, AHIP_Freq, sndOutputFreq, AHIP_Vol, 0x10000, AHIP_Pan, 0x8000, AHIP_Sound, 0, AHIP_EndChannel, 0, TAG_END);
    AHI_Play(actrl, AHIP_BeginChannel, 1, AHIP_Freq, sndOutputFreq, AHIP_Vol, 0x10000, AHIP_Pan, 0x8000, AHIP_Sound, 2, AHIP_EndChannel, 0, TAG_END);

    AHI_ControlAudio(actrl, AHIC_Play, TRUE, TAG_END);

    return true;
}

static bool sndAhiOpen()
{
    char namebuf[64] = "";

    gAhiMaxFreq = 0;

    if (!(AHImp = CreateMsgPort()))
    {
        sndAhiFree();
        return false;
    }

    if (!(AHIio = (struct AHIRequest *)CreateIORequest(AHImp, sizeof(struct AHIRequest))))
    {
        sndAhiFree();
        return false;
    }

    if (OpenDevice(AHINAME, AHI_NO_UNIT, (struct IORequest *)AHIio, 0))
    {
        sndAhiFree();
        return false;
    }

    AHIBase = (struct Library *)AHIio->ahir_Std.io_Device;

    AHI_GetAudioAttrs(AHI_DEFAULT_ID, NULL, AHIDB_BufferLen, sizeof(namebuf), AHIDB_Driver, (IPTR)namebuf, AHIDB_MaxMixFreq, (IPTR)&gAhiMaxFreq, TAG_END);

    if (!strcmp(namebuf, "paula"))
    {
        gAhiMaxFreq = 0;

        sndAhiFree();
        
        return false;
    }

    return true;
}

bool sndAhiDetect()
{
    bool ok = sndAhiOpen();

    sndAhiFree();

    return ok;
}

bool sndAhiInit()
{
    bool ok = false;

    if (!sndAhiOpen())
        return false;

    SoundHook.h_Entry = (ULONG (*)())&SoundFunc;
    SoundHook.h_SubEntry = NULL;
    SoundHook.h_Data = NULL;

    actrl = AHI_AllocAudio(AHIA_AudioID, AHI_DEFAULT_ID, AHIA_MixFreq, sndOutputFreq, AHIA_Channels, AHI_CHANNELS, AHIA_Sounds, AHI_CHANNELS * 2, AHIA_SoundFunc, (IPTR)&SoundHook, TAG_END);

    if (actrl)
    {
        s_sfxBuf = (int8 *)AllocVec(4 * SND_SAMPLES, MEMF_CHIP | MEMF_CLEAR | MEMF_PUBLIC);
        s_musBuf = (int8 *)AllocVec(4 * SND_SAMPLES, MEMF_CHIP | MEMF_CLEAR | MEMF_PUBLIC);

        if (s_sfxBuf && s_musBuf)
            ok = sndAhiSetup();
    }

    if (!ok)
        sndAhiFree();

    return ok;
}
