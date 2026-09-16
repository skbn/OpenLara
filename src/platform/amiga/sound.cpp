#include "common.h"
#include "sound_int.h"

#include <proto/dos.h>

int32 sndOutputFreq = 11025;
int32 sndStereo = 0;

static int32 s_backend = 0;

uint8 ADPCM4_ADAPT[] =
    {
        // IWRAM !
        192,
        192,
        136,
        136,
        128,
        128,
        128,
        128, // -8..-1
        112,
        128,
        128,
        128,
        128,
        136,
        136,
        192, //  0..+7
};

#ifdef USE_ASM
extern "C"
{
    void sndADPCM4_fill_asm(ADPCM4_STATE &state __asm("a0"), int8 *buffer __asm("a1"), const uint8 *data __asm("a2"), int32 size __asm("d0"));
    int32 sndPCM_fill_asm(int32 pos __asm("d0"), int32 inc __asm("d1"), int32 size __asm("d2"), int32 volume __asm("d3"), const uint8 *data __asm("a0"), int8 *buffer __asm("a1"));
    int32 sndPCM_mix_asm(int32 pos __asm("d0"), int32 inc __asm("d1"), int32 size __asm("d2"), int32 volume __asm("d3"), const uint8 *data __asm("a0"), int8 *buffer __asm("a1"));
    int32 sndPCM_fill_stereo_asm(int32 pos __asm("d0"), int32 inc __asm("d1"), int32 size __asm("d2"), int32 volL __asm("d3"), int32 volR __asm("d4"), const uint8 *data __asm("a0"), int8 *buffer __asm("a1"));
    int32 sndPCM_mix_stereo_asm(int32 pos __asm("d0"), int32 inc __asm("d1"), int32 size __asm("d2"), int32 volL __asm("d3"), int32 volR __asm("d4"), const uint8 *data __asm("a0"), int8 *buffer __asm("a1"));
}

#define sndADPCM4_fill sndADPCM4_fill_asm
#define sndPCM_fill sndPCM_fill_asm
#define sndPCM_mix sndPCM_mix_asm
#define sndPCM_fill_st sndPCM_fill_stereo_asm
#define sndPCM_mix_st sndPCM_mix_stereo_asm

#else

#define sndADPCM4_fill sndADPCM4_c
#define sndPCM_fill sndPCM_fill_c
#define sndPCM_mix sndPCM_mix_c
#define sndPCM_fill_st sndPCM_fill_stereo_c
#define sndPCM_mix_st sndPCM_mix_stereo_c

#endif

void sndADPCM4_c(ADPCM4_STATE &state, int8 *buffer, const uint8 *data, int32 size);
int32 sndPCM_fill_c(int32 pos, int32 inc, int32 size, int32 volume, const uint8 *data, int8 *buffer);
int32 sndPCM_mix_c(int32 pos, int32 inc, int32 size, int32 volume, const uint8 *data, int8 *buffer);
int32 sndPCM_fill_stereo_c(int32 pos, int32 inc, int32 size, int32 volL, int32 volR, const uint8 *data, int8 *buffer);
int32 sndPCM_mix_stereo_c(int32 pos, int32 inc, int32 size, int32 volL, int32 volR, const uint8 *data, int8 *buffer);

#define HAAS_LEN 165

#define CALC_INC (((SND_SAMPLE_FREQ << SND_FIXED_SHIFT) / sndOutputFreq) * pitch >> SND_PITCH_SHIFT)

struct Music
{
    const uint8 *data;
    int32 size;
    int32 pos;
    ADPCM4_STATE state;
    int8 haas[HAAS_LEN];
    int32 haasIdx;
    int8 monoBuf[SND_SAMPLES];

    int32 decode()
    {
        int32 frames = SND_SAMPLES;

        if (sndOutputFreq == 22050)
            frames >>= 1;
        else if (sndOutputFreq == 44100)
            frames >>= 2;

        int32 len = X_MIN(size - pos, frames >> 1);

        sndADPCM4_fill(state, monoBuf, data + pos, len);

        pos += len;

        if (pos >= size)
        {
            data = NULL;
            memset(monoBuf + (len << 1), 0, (frames - (len << 1)) * sizeof(monoBuf[0]));
        }

        if (gSettings.audio_music < 16)
        {
            for (int32 i = 0; i < frames; i++)
                monoBuf[i] = (monoBuf[i] * gSettings.audio_music) >> 4;
        }

        return frames;
    }

    int8 haasDelay(int8 s)
    {
        int8 d = haas[haasIdx];

        haas[haasIdx] = s;
        haasIdx++;

        if (haasIdx >= HAAS_LEN)
            haasIdx = 0;

        return d;
    }

    void fillMono(int8 *buffer)
    {
        int32 frames = decode();

        if (sndOutputFreq == 22050)
        {
            for (int32 i = 0; i < frames; i++)
            {
                *buffer++ = monoBuf[i];
                *buffer++ = monoBuf[i];
            }
        }
        else if (sndOutputFreq == 44100)
        {
            for (int32 i = 0; i < frames; i++)
            {
                int8 s = monoBuf[i];

                *buffer++ = s;
                *buffer++ = s;
                *buffer++ = s;
                *buffer++ = s;
            }
        }
        else
        {
            memcpy(buffer, monoBuf, frames);
        }
    }

    void fillStereo(int8 *buffer)
    {
        int32 frames = decode();

        if (sndOutputFreq == 22050)
        {
            for (int32 i = 0; i < frames; i++)
            {
                int8 d = haasDelay(monoBuf[i]);

                buffer[0] = monoBuf[i];
                buffer[1] = d;
                buffer[2] = monoBuf[i];
                buffer[3] = d;
                buffer += 4;
            }
        }
        else if (sndOutputFreq == 44100)
        {
            for (int32 i = 0; i < frames; i++)
            {
                int8 s = monoBuf[i];
                int8 d = haasDelay(s);

                buffer[0] = s;
                buffer[1] = d;
                buffer[2] = s;
                buffer[3] = d;
                buffer[4] = s;
                buffer[5] = d;
                buffer[6] = s;
                buffer[7] = d;
                buffer += 8;
            }
        }
        else
        {
            for (int32 i = 0; i < frames; i++)
            {
                buffer[0] = monoBuf[i];
                buffer[1] = haasDelay(monoBuf[i]);
                buffer += 2;
            }
        }
    }

    void reset()
    {
        memset(haas, 0, sizeof(haas));
        haasIdx = 0;
    }
};

struct Sample
{
    int32 pos;
    int32 inc;
    int32 size;
    int32 volume;
    int32 volL;
    int32 volR;
    const uint8 *data;

    void mix(int8 *buffer)
    {
        pos = sndPCM_mix(pos, inc, size, volume, data, buffer);

        if (pos >= size)
            data = NULL;
    }

    void fill(int8 *buffer)
    {
        pos = sndPCM_fill(pos, inc, size, volume, data, buffer);

        if (pos >= size)
            data = NULL;
    }

    void mixStereo(int8 *buffer)
    {
        pos = sndPCM_mix_st(pos, inc, size, volL, volR, data, buffer);

        if (pos >= size)
            data = NULL;
    }

    void fillStereo(int8 *buffer)
    {
        pos = sndPCM_fill_st(pos, inc, size, volL, volR, data, buffer);

        if (pos >= size)
            data = NULL;
    }
};

static Music music;
static Sample channels[SND_CHANNELS];
static int32 channelsCount;

static void sndPanVol(int32 volume, int32 pan, int32 &volL, int32 &volR)
{
    if (pan > 0)
    {
        volR = volume;
        volL = (volume * (64 - pan)) >> 6;
    }
    else
    {
        volL = volume;
        volR = (volume * (64 + pan)) >> 6;
    }
}

void *sndPlaySample(int32 index, int32 volume, int32 pitch, int32 mode)
{
    return sndPlaySamplePan(index, volume, 0, pitch, mode);
}

void *sndPlaySamplePan(int32 index, int32 volume, int32 pan, int32 pitch, int32 mode)
{
    if (!gSettings.audio_sfx)
        return NULL;

    volume = (volume * gSettings.audio_sfx) >> 4;

    const uint8 *data = level.soundData + level.soundOffsets[index];
    int32 size = data[0] | data[1] << 8 | data[2] << 16 | data[3] << 24;

    data += 4;

    int32 volL, volR;

    sndPanVol(volume, pan, volL, volR);

    if (mode == UNIQUE || mode == REPLAY)
    {
        for (int32 i = 0; i < channelsCount; i++)
        {
            Sample *sample = channels + i;

            if (sample->data != data)
                continue;

            sample->inc = CALC_INC;
            sample->volume = volume;
            sample->volL = volL;
            sample->volR = volR;

            if (mode == REPLAY)
                sample->pos = 0;

            return sample;
        }
    }

    if (channelsCount >= SND_CHANNELS)
        return NULL;

    Sample *sample = channels + channelsCount;
    sample->size = size << SND_FIXED_SHIFT;
    sample->pos = 0;
    sample->inc = CALC_INC;
    sample->volume = volume;
    sample->volL = volL;
    sample->volR = volR;
    sample->data = data;
    channelsCount++;

    return sample;
}

void sndPlayTrack(int32 track)
{
    if (!gSettings.audio_music)
        return;

    if (track == gCurTrack)
        return;

    gCurTrack = track;

    if (track == -1)
    {
        sndStopTrack();
        return;
    }

    int32 pctrack = -1;
    switch (track)
    {
    // TODO track 3
    case TRACK_TR1_TITLE:
        pctrack = 2;
        break;

    case TRACK_TR1_CAVES:
        pctrack = 3;
        break;

    case TRACK_TR1_CISTERN:
        pctrack = 4;
        break;

    case TRACK_TR1_WIND:
        pctrack = 5;
        break;

    case TRACK_TR1_PYRAMID:
        pctrack = 6;
        break;

    case TRACK_TR1_CUT_1:
        pctrack = 8;
        break;

    case TRACK_TR1_CUT_2:
        pctrack = 10;
        break;

    case TRACK_TR1_CUT_3:
        pctrack = 9;
        break;

    case TRACK_TR1_CUT_4:
        pctrack = 7;
        break;
    }

    if (pctrack < 0)
        return;

    char buf[32] = "audio/track_00.ad4";
    
    buf[12] += pctrack / 10;
    buf[13] += pctrack % 10;

    BPTR f = Open(buf, MODE_OLDFILE);

    if (!f)
        return;

    if (music.data)
    {
        const uint8 *old = music.data;

        music.data = NULL;
        delete[] old;
    }

    Seek(f, 0, OFFSET_END);
    int32 size = Seek(f, 0, OFFSET_CURRENT);
    Seek(f, 0, OFFSET_BEGINNING);
    uint8 *data = new uint8[size];
    Read(f, data, size);
    Close(f);

    // Clear music.data before setup, and write it after to ensure
    // music.fill() has a consistent state at any point in time
    music.size = size;
    music.pos = 0;
    music.state.zM1 = 0;
    music.state.zM2 = 0;
    music.state.tap = 0;
    music.state.quant = 0x0800;
    music.reset();
    music.data = data;
}

void sndStopTrack()
{
    const uint8 *data = music.data;

    music.data = NULL;
    music.size = 0;
    music.pos = 0;
    gCurTrack = -1;

    if (data)
        delete[] data;
}

bool sndTrackIsPlaying()
{
    return gCurTrack != -1;
}

void sndStopSample(int32 index)
{
    const uint8 *data = level.soundData + level.soundOffsets[index] + 4;
    int32 i = channelsCount;

    while (--i >= 0)
    {
        if (channels[i].data == data)
            channels[i] = channels[--channelsCount];
    }
}

void sndStop()
{
    channelsCount = 0;

    sndStopTrack();
}

void sndFill(int8 *buffer)
{
    bool mix = false;

    if (!sndStereo)
        memset(buffer, SND_ENCODE(0), SND_SAMPLES);
    else
        memset(buffer, 0, 2 * SND_SAMPLES);

    if (!gSettings.audio_sfx)
    {
        channelsCount = 0;
        return;
    }

    int32 ch = channelsCount;

    while (ch--)
    {
        Sample *sample = channels + ch;

        if (!sndStereo)
        {
            if (mix)
                sample->mix(buffer);
            else
                sample->fill(buffer);
        }
        else
        {
            if (mix)
                sample->mixStereo(buffer);
            else
                sample->fillStereo(buffer);
        }

        if (!sample->data)
            channels[ch] = channels[--channelsCount];

        mix = true;
    }
}

void sndFillMusic(int8 *buffer)
{
    if (!gSettings.audio_music || !music.data)
    {
        if (!sndStereo)
            memset(buffer, SND_ENCODE(0), SND_SAMPLES);
        else
            memset(buffer, 0, 2 * SND_SAMPLES);

        return;
    }

    if (!sndStereo)
        music.fillMono(buffer);
    else
        music.fillStereo(buffer);
}

void sndInitSamples()
{
    // nothing to do
}

void sndFreeSamples()
{
    // nothing to do
}

void sndInit()
{
    sndOutputFreq = gAudioFreq;

    if (gAudioBackend == SND_BACKEND_PAULA)
        sndStereo = gAudioStereo;
    else
        sndStereo = 1;

    if (gAudioBackend != SND_BACKEND_PAULA)
    {
        if (sndAhiInit())
        {
            s_backend = 2;
            return;
        }

        sndStereo = gAudioStereo;
    }

    if (sndOutputFreq > 22050)
        sndOutputFreq = 22050;

    if (sndPaulaInit())
    {
        s_backend = 1;
        return;
    }

    s_backend = 0;
    sndStereo = 0;
}

void sndFree()
{
    if (s_backend == 2)
    {
        sndAhiFree();
        s_backend = 0;

        return;
    }

    if (s_backend == 1)
    {
        sndPaulaFree();
        s_backend = 0;

        return;
    }
}

#define DECODE_ADPCM4(n)                                         \
    tap = zM2 + tap - (tap >> 3);                                \
    *buffer++ = SND_ENCODE(X_CLAMP(tap >> 8, SND_MIN, SND_MAX)); \
    res = ((n & 0xF) ^ 8) - 8;                                   \
    out = res * quant + (zM1 - zM2);                             \
    zM2 = zM1;                                                   \
    zM1 = out;                                                   \
    quant = (quant * (int32)ADPCM4_ADAPT[res + 8] + 127) >> 7;

void sndADPCM4_c(ADPCM4_STATE &state, int8 *buffer, const uint8 *data, int32 size)
{
    int32 zM1 = state.zM1;
    int32 zM2 = state.zM2;
    int32 tap = state.tap;
    int32 quant = state.quant;
    int32 res, out;

    for (int32 i = 0; i < size; i++)
    {
        uint32 n = *data++;

        DECODE_ADPCM4(n);
        n >>= 4;
        DECODE_ADPCM4(n);
    }

    state.zM1 = zM1;
    state.zM2 = zM2;
    state.tap = tap;
    state.quant = quant;
}

int32 sndPCM_fill_c(int32 pos, int32 inc, int32 size, int32 volume, const uint8 *data, int8 *buffer)
{
    int32 last = pos + SND_SAMPLES * inc;

    if (last > size)
        last = size;

    if (volume == (1 << SND_VOL_SHIFT))
    {
        while (pos < last)
        {
            *buffer++ = SND_DECODE(data[pos >> SND_FIXED_SHIFT]);
            pos += inc;
        }
    }
    else
    {
        while (pos < last)
        {
            *buffer++ = ((SND_DECODE(data[pos >> SND_FIXED_SHIFT]) * volume) >> SND_VOL_SHIFT);
            pos += inc;
        }
    }

    return pos;
}

int32 sndPCM_mix_c(int32 pos, int32 inc, int32 size, int32 volume, const uint8 *data, int8 *buffer)
{
    int32 last = pos + SND_SAMPLES * inc;

    if (last > size)
        last = size;

    if (volume == (1 << SND_VOL_SHIFT))
    {
        while (pos < last)
        {
            int16 amp = *buffer + SND_DECODE(data[pos >> SND_FIXED_SHIFT]);

            *buffer++ = X_CLAMP(amp, SND_MIN, SND_MAX);
            pos += inc;
        }
    }
    else
    {
        while (pos < last)
        {
            int16 amp = *buffer + ((SND_DECODE(data[pos >> SND_FIXED_SHIFT]) * volume) >> SND_VOL_SHIFT);

            *buffer++ = X_CLAMP(amp, SND_MIN, SND_MAX);
            pos += inc;
        }
    }

    return pos;
}

int32 sndPCM_fill_stereo_c(int32 pos, int32 inc, int32 size, int32 volL, int32 volR, const uint8 *data, int8 *buffer)
{
    int32 last = pos + SND_SAMPLES * inc;

    if (last > size)
        last = size;

    while (pos < last)
    {
        int32 s = SND_DECODE(data[pos >> SND_FIXED_SHIFT]);

        buffer[0] = (s * volL) >> SND_VOL_SHIFT;
        buffer[1] = (s * volR) >> SND_VOL_SHIFT;

        buffer += 2;
        pos += inc;
    }

    return pos;
}

int32 sndPCM_mix_stereo_c(int32 pos, int32 inc, int32 size, int32 volL, int32 volR, const uint8 *data, int8 *buffer)
{
    int32 last = pos + SND_SAMPLES * inc;

    if (last > size)
        last = size;

    while (pos < last)
    {
        int32 s = SND_DECODE(data[pos >> SND_FIXED_SHIFT]);

        buffer[0] = X_CLAMP(buffer[0] + ((s * volL) >> SND_VOL_SHIFT), SND_MIN, SND_MAX);
        buffer[1] = X_CLAMP(buffer[1] + ((s * volR) >> SND_VOL_SHIFT), SND_MIN, SND_MAX);

        buffer += 2;
        pos += inc;
    }

    return pos;
}
