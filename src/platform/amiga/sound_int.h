#ifndef H_SOUND_INT
#define H_SOUND_INT

extern int32 sndOutputFreq;
extern int32 sndStereo;

void sndFill(int8 *buffer);
void sndFillMusic(int8 *buffer);

bool sndPaulaInit();
void sndPaulaFree();
bool sndAhiInit();
void sndAhiFree();

#endif
