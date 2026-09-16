#ifndef H_SUBS
#define H_SUBS

#ifdef USE_SUBTITLES

// subsTime is in 1/3 ms so a 30 Hz update frame decrements exactly 100
#define SUBS_TIME_SCALE 3
#define SUBS_FRAME_TIME 100
#define SUBS_CHAR_TIME (100 * SUBS_TIME_SCALE)
#define SUBS_LINE_HEIGHT 14
#define SUBS_MAX_WIDTH (FRAME_WIDTH - 32)

StringID subsStr;
int32 subsPos;
int32 subsLength;
int32 subsPartTime;
int32 subsPartLength;
int32 subsTime;

// getSubs(), TR1 audio tracks 22..56
StringID subsGetForTrack(int32 track)
{
    if (track >= 22 && track <= 56 && track != 24)
        return StringID(STR_TR1_SUB_22 + (track - 22));

    return STR_EMPTY;
}

// subsGetNextPart(), ms in 1/3 units, no wide chars here
void subsGetNextPart()
{
    const char *subs;
    int32 i;
    int32 j;
    int32 k;
    int32 time;

    subs = STR[subsStr];
    subsPos += subsPartLength;

    if (subsPos >= subsLength)
    {
        subsTime = 0;
        subsStr = STR_EMPTY;
        return;
    }

    for (i = subsPos; i < subsLength; i++)
    {
        if (subs[i] != '[')
            continue;

        for (j = i + 1; j < subsLength; j++)
        {
            if (subs[j] != ']')
                continue;

            time = 0;

            for (k = i + 1; k < j; k++)
                time = time * 10 + subs[k] - '0';

            subsTime += (time - subsPartTime) * SUBS_TIME_SCALE;
            subsPartTime = time;
            subsPartLength = j - subsPos + 1;

            return;
        }
    }

    subsPartLength = subsLength - subsPos;
    subsTime = subsPartLength * SUBS_CHAR_TIME;
}

// showSubs()
void subsShow(StringID str)
{
    subsStr = str;
    subsTime = 0;

    if ((str == STR_EMPTY) || !gSettings.audio_subtitles)
        return;

    subsLength = strlen(STR[str]);
    subsPos = 0;
    subsPartTime = 0;
    subsPartLength = 0;

    subsGetNextPart();
}

void subsUpdate(int32 frames)
{
    if (subsTime > 0)
    {
        subsTime -= frames * SUBS_FRAME_TIME;

        if (subsTime <= 0)
            subsGetNextPart();
    }
}

// width of one word, same glyph metrics as drawText()
static int32 subsWordWidth(const char *p, const char *end)
{
    int32 w;
    int32 index;
    char c;

    w = 0;

    while (p < end)
    {
        c = *p;

        if ((c == ' ') || (c == '@') || (c == '['))
            break;

        if (c == '$')
        {
            p++;

            if (p >= end)
                break;

            index = uint8(*p);
            p++;
        }
        else
        {
            index = charRemap(c);
            p++;
        }

        w += char_width[index] + 1;
    }
    return w;
}

// where the next visual line ends, greedy word wrap at SUBS_MAX_WIDTH
static const char *subsLineEnd(const char *p, const char *end)
{
    const char *q;
    const char *lastSpace;
    int32 w;
    int32 wordW;

    q = p;
    w = 0;
    lastSpace = NULL;

    while (q < end)
    {
        if ((*q == '[') || (*q == '@'))
            break;

        if (*q == ' ')
        {
            lastSpace = q;
            w += 6;
            q++;

            continue;
        }

        wordW = subsWordWidth(q, end);

        if (lastSpace && (w + wordW > SUBS_MAX_WIDTH))
            return lastSpace;

        w += wordW;

        while ((q < end) && (*q != ' ') && (*q != '@') && (*q != '['))
            q++;
    }

    return q;
}

// drawText() stops neither at '@' nor at '[', so wrap and split here
void subsRender()
{
    const char *subs;
    const char *end;
    const char *p;
    const char *lineEnd;
    char buf[160];
    int32 lineCount;
    int32 line;
    int32 len;
    int32 y;

    if (!gSettings.audio_subtitles || (subsTime <= 0))
        return;

    subs = STR[subsStr] + subsPos;
    end = subs + subsPartLength;

    // count wrapped lines to bottom-anchor the block
    lineCount = 0;
    p = subs;

    while (p < end)
    {
        lineEnd = subsLineEnd(p, end);
        lineCount++;
        p = lineEnd;

        if ((p < end) && (*p == '['))
            break;

        if (p < end)
            p++;
    }

    y = FRAME_HEIGHT - 8 - lineCount * SUBS_LINE_HEIGHT;
    line = 0;
    p = subs;

    while (p < end)
    {
        lineEnd = subsLineEnd(p, end);
        len = lineEnd - p;

        if (len > 0)
        {
            if (len >= (int32)sizeof(buf))
                len = sizeof(buf) - 1;

            memcpy(buf, p, len);
            buf[len] = 0;

            drawText(0, y + line * SUBS_LINE_HEIGHT, buf, TEXT_ALIGN_CENTER);

            line++;
        }

        p = lineEnd;

        if ((p >= end) || (*p == '['))
            break;

        p++;
    }
}

// sndPlayTrack() where subtitles can accompany the track
void subsPlayTrack(int32 track)
{
    sndPlayTrack(track);
    subsShow(subsGetForTrack(track));
}

#endif
#endif
