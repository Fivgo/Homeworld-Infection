/*=============================================================================
    Name    : smixer.c
    Purpose : Low level audio mixer routines

    Created 01/10/1998 by salfreds
    Copyright Relic Entertainment, Inc.  All rights reserved.

    Butchered for your enjoyment...
=============================================================================*/

#include <stdio.h>
#include <string.h>
#include "fquant.h"
#include "soundcmn.h"
#include "soundlow.h"
#include "Debug.h"
#include "SoundStructs.h"
#include "Randy.h"
#include "FastMath.h"
#include "main.h"
#include "Globals.h"
#include "Universe.h"      // HWI-TEST (M3-R4 sound-burst-log): the universe tick on the flushed lines
#include <stdarg.h>
#include <math.h>

#define MIX_BLOCK_SIZE			FQ_SIZE * sizeof(short) * 2 // 256 samples, 16-bit, stereo = 1024 bytes

#define DS_NUM_BUFFER_BLOCKS	32
#define DS_MIX_BUFFER_SIZE		(32 * MIX_BLOCK_SIZE)
#define DS_MIX_BUFFER_AHEAD		(16 * MIX_BLOCK_SIZE)
#define DS_MIX_SLEEP			0L

#define WO_NUM_BUFFER_BLOCKS	32
#define WO_MIX_BUFFER_SIZE		(WO_NUM_BUFFER_BLOCKS * MIX_BLOCK_SIZE)
#define WO_MIX_BUFFER_AHEAD		(WO_NUM_BUFFER_BLOCKS * MIX_BLOCK_SIZE)
#define WO_MIX_SLEEP			0L

#define MIX_PANIC_THRES			22L // approx 4 fps - 1000/(11.60997732426*fps)
#define MIX_PANIC_DUR			16L	// approx 4 seconds - dur*fps

/* function prototypes */
void isoundmixerthreadSDL(void *dummy);
void isoundmixerqueueSDL();
sdword isoundmixerprocess(void *pBuf1, udword nSize1, void *pBuf2, udword nSize2);
// CAT: one decoder without the effect argument: the exe's mixer no longer generates q-noise or runs the tone, break,
//      noise, limit, filter, delay or acoustic-model effects on a speech stream (isoundmixerprocess 0x0057f850 inlines
//      the decode as memset/memcpy/fqDequantBlock and calls none of fqGenQNoiseE, fqAddToneE, fqAddBreakE,
//      fqAddNoiseE, fqLimitE, fqFilterE, fqDelay, fqAcModel)
sdword isoundmixerdecode(sbyte *readptr, real32 *writeptr1, real32 *writeptr2, ubyte *exponent, sdword size, uword bitrate);

/* variables */
struct fake_wavehdr {
    char *lpData;
    udword dwBufferLength;
    udword dwFlags;
} WaveHead[WO_NUM_BUFFER_BLOCKS];

udword mixerticks = 0;

// CAT: port diagnostic, not in the exe: blocks decoded per stream and output blocks with sound, read by the sound
//      self-test (/catSoundTest, SpeechEvent.c catSoundRunSelfTest)
volatile udword catSoundDecodedBlocks[SOUND_MAX_STREAM_BUFFERS];
volatile udword catSoundAudibleBlocks = 0;
udword panicdur = 0L;
udword framecount = 0L;

bool32 panicflag=FALSE;
sdword numvoices = 0;

udword buffersize;
udword dwBlockSize;
udword dwWritePos;
udword dwMixAhead;

sdword dctmode = FQ_MNORM;	// DCT mode
sdword dctsize = FQ_HSIZE;	// DCT block size

sdword dctpanicmode = SOUND_MODE_NORM;	// DCT panic mode, normal by default

real32 timebufferL[FQ_DSIZE], timebufferR[FQ_DSIZE], temptimeL[FQ_DSIZE], temptimeR[FQ_DSIZE];
real32 mixbuffer1L[FQ_SIZE], mixbuffer1R[FQ_SIZE], mixbuffer2L[FQ_SIZE], mixbuffer2R[FQ_SIZE];

//extern LPDIRECTSOUND lpDirectSound;
extern CHANNEL channels[];
extern bool32 soundinited;
extern STREAM streams[];
extern CHANNEL speechchannels[];
extern sdword numstreams;
extern bool32 bDirectSoundCertified;
extern SENTENCELUT *SentenceLUT;
extern real32 MasterEQ[];

extern SOUNDCOMPONENT	mixer;
extern SOUNDCOMPONENT	streamer;

extern bool32 bSoundPaused;
extern bool32 bSoundDeactivated;

extern sdword soundnumvoices;
extern sdword soundvoicemode;

extern udword rndFrameCount;
extern sdword channelsinuse;                // HWI-TEST (M3-R4 sound-burst-log): soundlow.c

// HWI-TEST (M3-R4 sound-burst-log, docs/m3-r4-sound-burst-log.md): the sound log. Neither the exe's sound system nor
//      the port's logged anything, so a burst of noise in a play session left no trace (the user's "large audio
//      spike of white noise" near the end of the full playthrough, 2026-10-28). The mixer thread and the stream
//      thread post short lines into a lock-free ring (catSoundLogPost) and the render task prints them with the
//      universe tick (catSoundLogFlush, "soundlog:" lines). Posted: a BURST at the output (CAT_BURST_BLOCKS blocks
//      in a row whose left channel peaks above CAT_BURST_PEAK with a zero-crossing rate above CAT_BURST_ZCR per cent
//      and a spectral flatness of the mixed spectrum above CAT_BURST_FLAT: white noise, not music or speech) with
//      the three loudest sources of its block (an SFX channel's patch, a speech / music stream's queue entry) and
//      its end; the panic mode's on / off; a stream starved for CAT_STARVED_BLOCKS blocks (its next block not written
//      in time); a stream end the mixer stepped past (an entry whose size is not a multiple of the block, so bytes
//      past it are decoded); the test injection /catSoundNoiseAt. Not in cataclysm.exe.
#define CAT_SLOG_SLOTS      16
#define CAT_SLOG_LEN        512
#define CAT_BURST_PEAK      12000           // of 32767
#define CAT_BURST_ZCR       30              // per cent of the left samples with a sign change (white noise: ~50)
#define CAT_BURST_FLAT      0.30f           // spectral flatness of the mixed left spectrum (white noise: > 0.5)
#define CAT_BURST_BLOCKS    3               // consecutive blocks before a report
#define CAT_BURST_REPORTS   40              // bursts reported per run
#define CAT_STARVED_BLOCKS  4               // a starvation episode shorter than this (a stream's start-up) is not logged
#define CAT_SLOG_RATE       20              // starved / missed-end lines per run
typedef struct { volatile sdword state; char text[CAT_SLOG_LEN]; } catSoundLogSlot;
static catSoundLogSlot catSoundLog[CAT_SLOG_SLOTS];
static volatile udword catSoundLogWrite = 0;
static volatile udword catSoundLogDropped = 0;
udword catSoundNoiseAt = 0;                 // /catSoundNoiseAt <block>: 12 blocks of noise into the first playing source
static bool32 catSoundInjectedThisBlock = FALSE;
static udword catBurstRun = 0, catBurstPeakMax = 0, catBurstReports = 0, catBurstStart = 0;
static bool32 catBurstReported = FALSE;
static udword catSlogStarved = 0, catSlogMissed = 0;
static udword catStarvedRun[SOUND_MAX_STREAM_BUFFERS];
static const void *catMissedEntry[SOUND_MAX_STREAM_BUFFERS];
static sdword catMissedSize[SOUND_MAX_STREAM_BUFFERS];

void catSoundLogPost(const char *fmt, ...)      // any thread
{
    udword n = __atomic_fetch_add(&catSoundLogWrite, 1, __ATOMIC_RELAXED) % CAT_SLOG_SLOTS;
    catSoundLogSlot *slot = &catSoundLog[n];
    va_list ap;

    if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) != 0)
    {                                                       // not flushed yet: dropped, counted
        __atomic_fetch_add(&catSoundLogDropped, 1, __ATOMIC_RELAXED);
        return;
    }
    va_start(ap, fmt);
    vsnprintf(slot->text, CAT_SLOG_LEN, fmt, ap);
    va_end(ap);
    __atomic_store_n(&slot->state, 1, __ATOMIC_RELEASE);
}

void catSoundLogFlush(void)                     // the render task, once a frame
{
    sdword i;
    udword dropped;

    for (i = 0; i < CAT_SLOG_SLOTS; i++)
    {
        catSoundLogSlot *slot = &catSoundLog[i];
        if (__atomic_load_n(&slot->state, __ATOMIC_ACQUIRE) == 1)
        {
            dbgMessagef("soundlog: %s tick=%u t=%.2f", slot->text, (unsigned)universe.univUpdateCounter, universe.totaltimeelapsed);
            __atomic_store_n(&slot->state, 0, __ATOMIC_RELEASE);
        }
    }
    dropped = __atomic_exchange_n(&catSoundLogDropped, 0, __ATOMIC_RELAXED);
    if (dropped != 0)
    {
        dbgMessagef("soundlog: %u lines dropped (ring full) tick=%u", dropped, (unsigned)universe.univUpdateCounter);
    }
}

static real32 catSoundFlatness(const real32 *x, sdword n)   // geometric / arithmetic mean of |x|: 1 = flat (white)
{
    sdword i;
    double sl = 0.0, sa = 0.0;

    for (i = 0; i < n; i++)
    {
        double a = fabs((double)x[i]) + 1e-3;
        sl += log(a);
        sa += a;
    }
    return (n > 0) ? (real32)(exp(sl / n) / (sa / n)) : 0.0f;
}

static real32 catSoundEnergy(CHANNEL *pchan)   // the block's decoded spectrum, scaled as it was mixed
{
    sdword k;
    double e = 0.0, v = (pchan->volfactorL > pchan->volfactorR) ? pchan->volfactorL : pchan->volfactorR;

    for (k = 0; k < FQ_SIZE; k++)
    {
        e += (double)pchan->mixbuffer1[k] * pchan->mixbuffer1[k] + (double)pchan->mixbuffer2[k] * pchan->mixbuffer2[k];
    }
    return (real32)(e * v * v);
}

static void catSoundBurstReport(sdword peak, sdword zc, sdword n, real32 flat)
{
    struct { real32 e; sdword stream, index; } top[3];
    sdword i, j, k, count = 0;
    real32 e;

    for (i = 0; i < 3; i++) { top[i].e = 0.0f; top[i].stream = 0; top[i].index = -1; }
    for (i = 0; i < numstreams + soundnumvoices; i++)
    {
        bool32 stream = (i < numstreams);
        CHANNEL *pchan = stream ? &speechchannels[i] : &channels[i - numstreams];
        if (pchan->status < SOUND_PLAYING || (!stream && pchan->ppatch == NULL))
        {
            continue;
        }
        e = catSoundEnergy(pchan);
        for (j = 0; j < 3; j++)
        {
            if (e > top[j].e)
            {
                for (k = 2; k > j; k--) top[k] = top[k - 1];
                top[j].e = e; top[j].stream = stream; top[j].index = stream ? i : i - numstreams;
                break;
            }
        }
        count++;
    }
    catSoundLogPost("burst start block=%u peak=%d zcr=%d%% flat=%.2f sources=%d voices=%d/%d inuse=%d panic=%d dct=%s",
                    mixerticks, peak, (n > 0) ? (zc * 100 / n) : 0, flat, count, soundnumvoices, numvoices, channelsinuse,
                    (int)panicflag, (dctmode == FQ_MHALF) ? "half" : "norm");
    for (j = 0; j < 3; j++)
    {
        if (top[j].index < 0 || top[j].e <= 0.0f)
        {
            continue;
        }
        if (top[j].stream)
        {
            STREAM *pstream = &streams[top[j].index];
            CHANNEL *pchan = &speechchannels[top[j].index];
            STREAMQUEUE *pqueue = &pstream->queue[pstream->playindex];
            catSoundLogPost("burst src stream=%d e=%.3g flat=%.2f status=%d/%d entry=%d event=0x%x actor=%d offset=%ld size=%d read=%d bitrate=%d ch=%d pos=%ld writepos=%ld bs=%d%d rb=%d wb=%d queued=%d toplay=%d vol=%.0f",
                            top[j].index, top[j].e, catSoundFlatness(pchan->mixbuffer1, pchan->fqsize), pchan->status, pstream->status,
                            pstream->playindex, (unsigned)pqueue->speechEvent, pqueue->actornum, (long)pqueue->offset, pqueue->size,
                            pchan->amountread, (int)pchan->bitrate, (int)pchan->numchannels, (long)(pchan->currentpos - (sbyte *)pstream->buffer),
                            (pstream->writepos != NULL) ? (long)(pstream->writepos - pstream->buffer) : -1L,
                            pstream->blockstatus[0], pstream->blockstatus[1], pstream->readblock, pstream->writeblock,
                            pstream->numqueued, pstream->numtoplay, pchan->volume);
        }
        else
        {
            CHANNEL *pchan = &channels[top[j].index];
            PATCH *ppatch = pchan->ppatch;
            sdword bank = -1, patch = SNDpatchIndex(ppatch, &bank);
            sdword blockBytes = (ppatch->bitrate > 0) ? (ppatch->bitrate >> 3) : 1;
            catSoundLogPost("burst src chan=%d e=%.3g flat=%.2f patch=bank%d:%d id=0x%x block=%ld/%ld loop=%d-%ld-%ld pitch=%.3f vol=%.0f pan=%d status=%d prio=%d fq=%d rate=%u",
                            top[j].index, top[j].e, catSoundFlatness(pchan->mixbuffer1, pchan->fqsize), bank, patch, (unsigned)ppatch->id,
                            (long)((pchan->currentpos - (sbyte *)ppatch->dataoffset) / blockBytes),
                            (long)((ppatch->datasize - ppatch->dataoffset) / blockBytes), (int)pchan->looping,
                            (long)((ppatch->loopstart - ppatch->dataoffset) / blockBytes), (long)((ppatch->loopend - ppatch->dataoffset) / blockBytes),
                            pchan->pitch, pchan->volume, (int)pchan->pan, pchan->status, pchan->priority, pchan->fqsize,
                            (unsigned)ppatch->waveformat.frequency);
        }
    }
}

static void catSoundBurstCheck(short *buf, sdword nShorts)   // the mixer thread, after each output block
{
    sdword i, n = 0, zc = 0, peak = 0;
    short prev;
    bool32 noisy;
    real32 flat = 0.0f;

    if (buf == NULL || nShorts < 4)
    {
        return;
    }
    prev = buf[0];
    for (i = 0; i < nShorts; i += 2)                        // the left channel
    {
        short s = buf[i];
        sdword a = (s < 0) ? -(sdword)s : (sdword)s;
        if (a > peak) peak = a;
        if (((sdword)s ^ (sdword)prev) < 0) zc++;
        prev = s;
        n++;
    }
    noisy = (peak >= CAT_BURST_PEAK) && (zc * 100 >= n * CAT_BURST_ZCR);
    if (noisy)
    {
        flat = catSoundFlatness(mixbuffer1L, dctsize);      // the mixed left spectrum of this block
        noisy = (flat >= CAT_BURST_FLAT);
    }
    if (catSoundInjectedThisBlock)
    {                                                       // the test injection's blocks, measured (calibration)
        catSoundLogPost("test block=%u peak=%d zcr=%d%% flat=%.2f noisy=%d", mixerticks, peak, (n > 0) ? (zc * 100 / n) : 0,
                        (flat > 0.0f) ? flat : catSoundFlatness(mixbuffer1L, dctsize), (int)noisy);
    }
    if (noisy)
    {
        catBurstRun++;
        if ((udword)peak > catBurstPeakMax) catBurstPeakMax = (udword)peak;
        if (catBurstRun == CAT_BURST_BLOCKS && catBurstReports < CAT_BURST_REPORTS)
        {
            catBurstReported = TRUE;
            catBurstReports++;
            catBurstStart = mixerticks;
            catSoundBurstReport(peak, zc, n, flat);
        }
    }
    else
    {
        if (catBurstReported)
        {
            catSoundLogPost("burst end block=%u blocks=%u peakmax=%u%s", mixerticks, mixerticks - catBurstStart, catBurstPeakMax,
                            (catBurstReports == CAT_BURST_REPORTS) ? " (reports capped)" : "");
        }
        catBurstRun = 0;
        catBurstReported = FALSE;
        catBurstPeakMax = 0;
    }
}

static void catSoundStarvedCheck(sdword i, STREAM *pstream, CHANNEL *pchan, bool32 starved)
{
    if (starved)
    {
        catStarvedRun[i]++;
    }
    else if (catStarvedRun[i] != 0)
    {
        if (catStarvedRun[i] >= CAT_STARVED_BLOCKS && catSlogStarved < CAT_SLOG_RATE)
        {
            catSlogStarved++;
            catSoundLogPost("stream %d starved %u blocks (ended block=%u status=%d/%d bs=%d%d rb=%d wb=%d pos=%ld queued=%d)%s",
                            i, catStarvedRun[i], mixerticks, pchan->status, pstream->status, pstream->blockstatus[0], pstream->blockstatus[1],
                            pstream->readblock, pstream->writeblock, (long)(pchan->currentpos - (sbyte *)pstream->buffer), pstream->numqueued,
                            (catSlogStarved == CAT_SLOG_RATE) ? " (last one logged)" : "");
        }
        catStarvedRun[i] = 0;
    }
}

static void catSoundMissedEndCheck(sdword i, STREAM *pstream, CHANNEL *pchan, STREAMQUEUE *pqueue)
{
    sdword blockBytes = (pchan->bitrate > 0) ? (pchan->bitrate >> 3) : 0;
    long past = (pstream->writepos != NULL) ? (long)(pchan->currentpos - (sbyte *)pstream->writepos) : 0L;
    sdword over = pchan->amountread - pqueue->size;

    if (blockBytes <= 0 || catSlogMissed >= CAT_SLOG_RATE)
    {
        return;
    }
    if (!((past > 0 && past < blockBytes) || (pqueue->size > 0 && over > 0 && over < blockBytes)))
    {
        return;
    }
    if (catMissedEntry[i] == (const void *)pqueue && catMissedSize[i] == pqueue->size)
    {
        return;                                             // this entry's miss is logged
    }
    catMissedEntry[i] = pqueue;
    catMissedSize[i] = pqueue->size;
    catSlogMissed++;
    catSoundLogPost("stream %d end missed block=%u: past writepos by %ld bytes, read %d of entry size %d (block %d bytes) entry=%d event=0x%x offset=%ld%s",
                    i, mixerticks, past, pchan->amountread, pqueue->size, blockBytes, pstream->playindex, (unsigned)pqueue->speechEvent,
                    (long)pqueue->offset, (catSlogMissed == CAT_SLOG_RATE) ? " (last one logged)" : "");
}

static sdword catSoundNoiseLeft = -1;           // blocks of test noise still to inject (-1: not armed yet)
static void catSoundNoiseInject(CHANNEL *pchan, bool32 stream, sdword index)   // /catSoundNoiseAt: the detector's fence
{
    static udword seed = 12345;
    sdword k;

    if (catSoundNoiseAt == 0 || mixerticks < catSoundNoiseAt || catSoundInjectedThisBlock)
    {
        return;
    }
    if (catSoundNoiseLeft < 0)
    {
        catSoundNoiseLeft = 12;                             // the first 12 blocks with a playing source from that block on
    }
    if (catSoundNoiseLeft == 0)
    {
        return;
    }
    catSoundNoiseLeft--;
    catSoundInjectedThisBlock = TRUE;
    for (k = 0; k < FQ_SIZE; k++)
    {
        seed = seed * 1103515245u + 12345u;
        pchan->mixbuffer1[k] = ((real32)(sdword)((seed >> 16) & 0x7fff) - 16384.0f) / 16384.0f * 10000.0f;
        seed = seed * 1103515245u + 12345u;
        pchan->mixbuffer2[k] = ((real32)(sdword)((seed >> 16) & 0x7fff) - 16384.0f) / 16384.0f * 10000.0f;
    }
    if (catSoundNoiseLeft == 11)
    {
        catSoundLogPost("test noise injected into %s %d from block=%u (12 blocks)", stream ? "stream" : "channel", index, mixerticks);
    }
}

// DCT mode functions
void soundMixerGetMode(sdword *mode)	// mode SOUND_MODE_NORM or SOUND_MODE_AUTO or SOUND_MODE_LOW
{
	// check DCT mode and panic mode
	if((dctmode == FQ_MHALF) && (dctpanicmode == SOUND_MODE_NORM))
	{
		*mode=SOUND_MODE_LOW;

		return;
	}

	// check DCT mode and panic mode
	if((dctmode == FQ_MNORM) && (dctpanicmode == SOUND_MODE_AUTO))
	{
		*mode=SOUND_MODE_AUTO;

		return;
	}

	*mode=SOUND_MODE_NORM;

	return;
}

void soundMixerSetMode(sdword mode)		// mode SOUND_MODE_NORM or SOUND_MODE_AUTO or SOUND_MODE_LOW
{
#ifndef _MACOSX_FIX_SOUND

	if(mode == SOUND_MODE_LOW)
	{
		// set DCT mode
		dctmode=FQ_MHALF;

		// set block size for effects
		dctsize=FQ_QSIZE;
		fqSize(dctsize);

		// set panic mode
		dctpanicmode=SOUND_MODE_NORM;

		return;
	}

	if(mode == SOUND_MODE_AUTO)
	{
		// set DCT mode
		dctmode=FQ_MNORM;

		// set block size for effects
		dctsize=FQ_HSIZE;
		fqSize(dctsize);

		// set panic mode
		dctpanicmode=SOUND_MODE_AUTO;

		return;
	}

	// if mode is SOUND_MODE_NORM
	// set DCT mode
	dctmode=FQ_MNORM;

	// set block size for effects
	dctsize=FQ_HSIZE;
	fqSize(dctsize);

	// set panic mode
	dctpanicmode=SOUND_MODE_NORM;

#endif // _MACOSX_FIX_SOUND

	return;
}

void soundPanicReset(void)
{
	// reset frame count
	framecount=0;
	panicflag=TRUE;
	return;
}

/*-----------------------------------------------------------------------------
	Name		:
	Description	:
	Inputs		:
	Outputs		:
	Return		:
----------------------------------------------------------------------------*/	
sdword isoundmixerinit(SDL_AudioSpec *aspec)
{
#ifndef _MACOSX_FIX_SOUND

	// Initialize codec
	fqInitDequant();

	// Set random number generator for effects
	fqRand((int (*)(int))ranRandomFnSimple,RANDOM_SOUND);

	// Set square root function for effects
	fqSqrt((double (*)(double))fmathSqrtDouble);

	// Set block size for effects
	fqSize(dctsize);

	// init the iDCT function
	if (SOUND_OK != fqDecBlock(mixbuffer1L, mixbuffer2L, timebufferL, temptimeL, FQ_MINIT, FQ_MINIT))
	{
		return (SOUND_ERR);
	}

	if (SOUND_OK != fqDecBlock(mixbuffer1R, mixbuffer2R, timebufferR, temptimeR, FQ_MINIT, FQ_MINIT))
	{
		return (SOUND_ERR);
	}

#endif // _MACOSX_FIX_SOUND

	return (SOUND_OK);
}


/*-----------------------------------------------------------------------------
	Name		:
	Description	:
	Inputs		:
	Outputs		:
	Return		:
----------------------------------------------------------------------------*/	
void isoundmixerrestore(void)
{
	mixer.timeout = 0;
}


/*-----------------------------------------------------------------------------
	Name		:
	Description	:
	Inputs		:
	Outputs		:
	Return		:
----------------------------------------------------------------------------*/	
sdword isoundmixerprocess(void *pBuf1, udword nSize1, void *pBuf2, udword nSize2)
{
#ifndef _MACOSX_FIX_SOUND

	sdword i, amountread;
	CHANNEL	*pchan;
	STREAM *pstream;
	STREAMQUEUE *pqueue;
	sdword chan;

	////////////////////////
	// begin panic mode code
	////////////////////////

	// check game running and sound deactivated flags
	if((gameIsRunning) && (!bSoundDeactivated))
	{
		// check ticks
		if(mixerticks%MIX_PANIC_THRES == 0)
		{
			// compare frame numbers
			if((framecount > 0L) && (framecount == rndFrameCount))
			{
				// check panic flag
				if(panicflag == FALSE)
				{
					// check panic mode
					if(dctpanicmode == SOUND_MODE_AUTO)
					{
						// set DCT mode
						dctmode=FQ_MHALF;

						// set block size for effects
						dctsize=FQ_QSIZE;
						fqSize(dctsize);
					}

					// check voice mode
					if(soundvoicemode == SOUND_MODE_AUTO)
					{
						// set number of voices to min
						soundnumvoices=SOUND_MIN_VOICES;

						// argh... fuck! panic, panic :)
						soundPanic();
					}

					// set panic flag
					panicflag=TRUE;
					catSoundLogPost("panic on block=%u voices %d->%d dct=%s (no frame for %u blocks)", mixerticks, numvoices, soundnumvoices, (dctmode == FQ_MHALF) ? "half" : "norm", (unsigned)MIX_PANIC_THRES);   // HWI-TEST (M3-R4 sound-burst-log)
				}

				// reset duration
				panicdur=0;
			}


			// check duration
			if(panicdur >= MIX_PANIC_DUR)
			{
				// check panic flag
				if(panicflag == TRUE)
				{
					// check panic mode
					if(dctpanicmode == SOUND_MODE_AUTO)
					{
						// set DCT mode
						dctmode=FQ_MNORM;

						// set block size for effects
						dctsize=FQ_HSIZE;
						fqSize(dctsize);
					}

					// check voice mode
					if(soundvoicemode == SOUND_MODE_AUTO)
					{
						// set number of voices
						if(numvoices > 0)
						{
							// set number of voices back
							soundnumvoices=numvoices;
						}
					}

					// set panic flag
					panicflag=FALSE;
					catSoundLogPost("panic off block=%u voices %d dct=%s", mixerticks, soundnumvoices, (dctmode == FQ_MHALF) ? "half" : "norm");   // HWI-TEST (M3-R4 sound-burst-log)
				}

				// reset duration
				panicdur=0;
			}
			else
			{
				// increment duration
				panicdur++;
			}

			// check panic flag
			if(panicflag == FALSE)
			{
				// save number of voices
				numvoices=soundnumvoices;
			}
			
			// save frame number
			framecount=rndFrameCount;
		}
	}

	///////////////////////
	// end panic mode code
	///////////////////////
	
	/* clear the mixbuffers */
	memset(mixbuffer1L, 0, FQ_SIZE * sizeof(real32));
	memset(mixbuffer1R, 0, FQ_SIZE * sizeof(real32));
	memset(mixbuffer2L, 0, FQ_SIZE * sizeof(real32));
	memset(mixbuffer2R, 0, FQ_SIZE * sizeof(real32));
	catSoundInjectedThisBlock = FALSE;                     // HWI-TEST (M3-R4 sound-burst-log)
	
	/* mix the speech first */
	if (streams != NULL)
	{
		for (i = 0; i < numstreams; i++)
		{
			if(speechchannels[i].status >= SOUND_PLAYING)
			{
				pstream = &streams[i];
				pchan = &speechchannels[i];
				pqueue = &pstream->queue[pstream->playindex];
	
				catSoundStarvedCheck(i, pstream, pchan, (pstream->blockstatus[pstream->readblock] == 0) && (pchan->status != SOUND_STOPPING));   // HWI-TEST (M3-R4 sound-burst-log)
				if (pstream->blockstatus[pstream->readblock] == 0)
				{
					if (pchan->status == SOUND_STOPPING)
					{
						pstream->writepos = 0;
						pstream->playindex = pstream->writeindex;
						pstream->numtoplay = 0;
						pchan->status = SOUND_INUSE;
					}
					continue;
				}
	
				if (pstream->blockstatus[pstream->readblock] == 1)
				{
					pstream->blockstatus[pstream->readblock] = 2;
				}
	
				amountread = isoundmixerdecode(pchan->currentpos, pchan->mixbuffer1, pchan->mixbuffer2, pchan->exponentblockL,
								pchan->fqsize, pchan->bitrate);
				pchan->currentpos += amountread;
				pchan->amountread += amountread;
				catSoundDecodedBlocks[i]++;                     // CAT: port diagnostic (sound self-test)
				catSoundNoiseInject(pchan, TRUE, i);            // HWI-TEST (M3-R4 sound-burst-log): /catSoundNoiseAt
				
				/* figure out any volume fades, pan fades, etc */
				if (pchan->volticksleft)
				{
					pchan->volume += pchan->volfade;
					pchan->volticksleft--;
					
					if (pchan->volticksleft <= 0)
					{
						pchan->volfade = 0.0f;
						pchan->volume = (real32)pchan->voltarget;
						pchan->volticksleft = 0;
						pchan->voltarget = -1;
					}
	
					// CAT: the clamp rounds with Relic's rint (fChopT 0x00c16840 in isoundmixerprocess 0x0057f850)
					if (fqRint(pchan->volume) > SOUND_VOL_MAX)
					{
						pchan->volume = SOUND_VOL_MAX;
					}
	
					if (fqRint(pchan->volume) < SOUND_VOL_MIN)
					{
						pchan->volume = SOUND_VOL_MIN;
					}
	
					if ((pchan->status == SOUND_STOPPING) && (pchan->volume == SOUND_VOL_MIN))
					{
						if (pstream->numqueued == 0)
						{
							pstream->status = SOUND_STREAM_INUSE;
							pstream->queueindex = 0;
							pstream->writeindex = 0;
							pstream->playindex = 0;
							pstream->numqueued = 0;
						}
						else
						{
							pstream->status = SOUND_STREAM_STARTING;
							pstream->writeindex = pstream->queueindex - pstream->numqueued;
							if (pstream->writeindex < 0)
							{
								pstream->writeindex += SOUND_MAX_STREAM_QUEUE;
							}
							pstream->playindex = pstream->writeindex;
						}
						pchan->status = SOUND_FREE;
						continue;
					}
					
					SNDcalcvolpan(pchan);
				}
	
				/******************************************************************************/
				/* Shane - pchan->volfactorL BELOW IS PROBABLY NOT THE PROPER VOLUME. - Janik */
				/******************************************************************************/

				if (pchan->numchannels < SOUND_STEREO)	// added this check because of a strange bug that caused the music to
				{										// have a mixHandle of 0, which played a buzzing in the left channel
														// this doesn't fix the bug, but it'll keep the buzzing from happening
					/* mix in SFX */
					if (pqueue->mixHandle >= SOUND_OK)
					{
	//					fqMix(pchan->mixbuffer1,channels[SNDchannel(pqueue->mixHandle)].mixbuffer1, pchan->volfactorL);
	//					fqMix(pchan->mixbuffer2,channels[SNDchannel(pqueue->mixHandle)].mixbuffer2, pchan->volfactorL);
						chan = SNDchannel(pqueue->mixHandle);
						fqMix(pchan->mixbuffer1,channels[chan].mixbuffer1, 1.0f);	//channels[chan].volfactorL);
						fqMix(pchan->mixbuffer2,channels[chan].mixbuffer2, 1.0f);	//channels[chan].volfactorL);
					}
	
					/* do the EQ */
					if (pqueue->eq != NULL)
					{
						if (pqueue->eq->flags && STREAM_FLAGS_EQ)
						{
							/* equalize this sucker */
							fqEqualize(pchan->mixbuffer1, pqueue->eq->eq);
							fqEqualize(pchan->mixbuffer2, pqueue->eq->eq);
						}
					}
					// CAT: no effect or delay stage and no effect scale level (isoundmixerprocess 0x0057f850)
				}
	
				/* add it to mix buffer */
				fqMix(mixbuffer1L,pchan->mixbuffer1,pchan->volfactorL);
				fqMix(mixbuffer2L,pchan->mixbuffer2,pchan->volfactorL);

				/* if this is stereo, do the right channel */
				if (pchan->numchannels == SOUND_STEREO)
				{
					amountread = isoundmixerdecode(pchan->currentpos, pchan->mixbuffer1R, pchan->mixbuffer2R, pchan->exponentblockR,
									pchan->fqsize, pchan->bitrate);
					pchan->currentpos += amountread;
					pchan->amountread += amountread;

					fqMix(mixbuffer1R,pchan->mixbuffer1R,pchan->volfactorR);
					fqMix(mixbuffer2R,pchan->mixbuffer2R,pchan->volfactorR);
				}
				else
				{
					fqMix(mixbuffer1R,pchan->mixbuffer1,pchan->volfactorR);
					fqMix(mixbuffer2R,pchan->mixbuffer2,pchan->volfactorR);
				}
	
				/* this is a clock for the fequency filtering/effects */
				if (pqueue->effect != NULL)
				{
					pqueue->effect->nClockCount++;
				}
	
				if ((pchan->currentpos == (sbyte *)(pstream->buffer + (pstream->blocksize * (pstream->readblock + 1)))) ||
					(pchan->currentpos == (sbyte *)pstream->writepos))
				{
					pstream->blockstatus[pstream->readblock++] = 0;
					if (pstream->readblock >= 2)
					{
						pstream->readblock = 0;
					}
				}
				else
				{
					catSoundMissedEndCheck(i, pstream, pchan, pqueue);   // HWI-TEST (M3-R4 sound-burst-log)
				}
	
				if (pchan->amountread == pqueue->size)
				{
					pchan->amountread = 0;
					if (pstream->numtoplay > 0)
					{
						/* finished this queued stream, go on to the next */
						pstream->playindex++;
						pstream->numtoplay--;
						if (pstream->playindex >= SOUND_MAX_STREAM_QUEUE)
						{
							pstream->playindex = 0;
						}

						if (pqueue->mixHandle >= SOUND_OK)
						{
							if (pqueue->pmixPatch != pstream->queue[pstream->playindex].pmixPatch)
							{
								/* we're mixing in a patch to this stream, shut it down */
								soundstop(pqueue->mixHandle, 0.0f);
								pqueue->mixHandle = SOUND_DEFAULT;
								pqueue->pmixPatch = NULL;
								pqueue->mixLevel = SOUND_VOL_MIN;
							}
							else
							{
								pstream->queue[pstream->playindex].mixHandle = pqueue->mixHandle;
							}
						}			
					}
				}
	
				if (pchan->currentpos >= pchan->endpos)
				{
					/* get the next block */
					pchan->currentpos = pchan->freqdata;
				}
			}
		}
	}

	/* mix the SFX channels */
	for (i = 0; i < soundnumvoices; i++)
	{
		if (channels[i].status >= SOUND_PLAYING)
		{
			pchan = &channels[i];

			// CAT: a playing channel without a patch is released (isoundmixerprocess 0x0057f850)
			if (pchan->ppatch == NULL)
			{
				SNDreleasebuffer(pchan);
				continue;
			}

			if (!pchan->looping)
			{
				if (pchan->currentpos >= (sbyte *)pchan->ppatch->datasize)
				{
					SNDreleasebuffer(pchan);
					continue;
				}
			}
			
			if (pchan->status == SOUND_LOOPEND)
			{
				pchan->status = SOUND_PLAYING;
				pchan->currentpos = (sbyte *)pchan->ppatch->loopend;
				pchan->looping = FALSE;
			}
			else if (pchan->status == SOUND_RESTART)
			{
				pchan->status = SOUND_PLAYING;
				pchan->currentpos = (sbyte *)pchan->ppatch->dataoffset;
			}


			amountread = isoundmixerdecode(pchan->currentpos, pchan->mixbuffer1, pchan->mixbuffer2, pchan->exponentblockL,
							pchan->fqsize, pchan->ppatch->bitrate);
			pchan->currentpos += amountread;
			catSoundNoiseInject(pchan, FALSE, i);               // HWI-TEST (M3-R4 sound-burst-log): /catSoundNoiseAt

			if (pchan->looping)
			{
				if (pchan->currentpos >= (sbyte *)pchan->ppatch->loopend)
				{
					pchan->currentpos = (sbyte *)pchan->ppatch->loopstart;
				}
			}

			/* figure out any volume fades, pan fades, etc */
			if (pchan->volticksleft)
			{
				pchan->volume += pchan->volfade;
				pchan->volticksleft--;
				
				if (pchan->volticksleft <= 0)
				{
					pchan->volfade = 0.0f;
					pchan->volume = (real32)pchan->voltarget;
					pchan->volticksleft = 0;
					pchan->voltarget = -1;
				}

				// CAT: the clamp and the stop test round with Relic's rint (isoundmixerprocess 0x0057f850)
				if (fqRint(pchan->volume) > SOUND_VOL_MAX)
				{
					pchan->volume = SOUND_VOL_MAX;
				}

				if (fqRint(pchan->volume) < SOUND_VOL_MIN)
				{
					pchan->volume = SOUND_VOL_MIN;
				}

				if ((pchan->status == SOUND_STOPPING) && (fqRint(pchan->volume) == SOUND_VOL_MIN))
				{
					SNDreleasebuffer(pchan);
					continue;
				}
				
				SNDcalcvolpan(pchan);
			}

			if (pchan->panticksleft)
			{
				pchan->pan += pchan->panfade;
				pchan->panticksleft--;
				
				if (pchan->panticksleft <= 0)
				{
					pchan->panfade = 0;
					pchan->pan = pchan->pantarget;
					pchan->panticksleft = 0;
				}
			
				if (pchan->pan > SOUND_PAN_MAX)
				{
					pchan->pan =  SOUND_PAN_MIN + (pchan->pan - SOUND_PAN_MAX);
				}
			
				if (pchan->pan < SOUND_PAN_MIN)
				{
					pchan->pan = SOUND_PAN_MAX + (pchan->pan - SOUND_PAN_MIN);
				}
				
				SNDcalcvolpan(pchan);
			}

			if (pchan->pitchticksleft)
			{
				pchan->pitch += pchan->pitchfade;
				pchan->pitchticksleft--;
				
				if (pchan->pitchticksleft <= 0)
				{
					pchan->pitchfade = 0.0f;
					pchan->pitch = pchan->pitchtarget;
					pchan->pitchticksleft = 0;
				}
			}
			
			/******************************************************/
			/* Shane - Try the pitch shift again below... - Janik */
			/******************************************************/
#if 1
			/* do pitch shift */
			fqPitchShift(pchan->mixbuffer1, pchan->pitch);
			fqPitchShift(pchan->mixbuffer2, pchan->pitch);
#endif

			if (pchan->usecardiod)
			{
				/* apply cardiod filter for fake doppler */
				fqEqualize(pchan->mixbuffer1, pchan->cardiodfilter);
				fqEqualize(pchan->mixbuffer2, pchan->cardiodfilter);
			}
			
			/* equalize this sucker */
			fqEqualize(pchan->mixbuffer1, pchan->filter);
			fqEqualize(pchan->mixbuffer2, pchan->filter);

			if (!pchan->mute)
			{
				fqMix(mixbuffer1L,pchan->mixbuffer1,pchan->volfactorL);
				fqMix(mixbuffer2L,pchan->mixbuffer2,pchan->volfactorL);
				fqMix(mixbuffer1R,pchan->mixbuffer1,pchan->volfactorR);
				fqMix(mixbuffer2R,pchan->mixbuffer2,pchan->volfactorR);
			}
		}
		
	}
	
	fqEqualize(mixbuffer1L, MasterEQ);
	fqEqualize(mixbuffer2L, MasterEQ);
	fqEqualize(mixbuffer1R, MasterEQ);
	fqEqualize(mixbuffer2R, MasterEQ);

	fqDecBlock(mixbuffer1L, mixbuffer2L, timebufferL, temptimeL, dctmode, FQ_MNORM);
	fqDecBlock(mixbuffer1R, mixbuffer2R, timebufferR, temptimeR, dctmode, FQ_MNORM);

	if (!reverseStereo)
	{
		/* play normally */
		fqWriteTBlock(timebufferL, timebufferR, 2, pBuf1, nSize1, pBuf2, nSize2);
	}
	else
	{
		/* swap the left and right channels */
		fqWriteTBlock(timebufferR, timebufferL, 2, pBuf1, nSize1, pBuf2, nSize2);
	}

	catSoundBurstCheck((short *)pBuf1, (sdword)nSize1);    // HWI-TEST (M3-R4 sound-burst-log): nSize1 counts shorts
	mixerticks++;

	// CAT: port diagnostic (sound self-test): did this block put sound into the output buffer?
	if (pBuf1 != NULL)
	{
		for (i = 0; i < (sdword)nSize1; i++)                // nSize1 counts shorts (fqWriteTBlock)
		{
			if (((short *)pBuf1)[i] != 0)
			{
				catSoundAudibleBlocks++;
				break;
			}
		}
	}

#endif // _MAOSX_FIX_ME

	return (SOUND_OK);
}


/*-----------------------------------------------------------------------------
	Name		:
	Description	:
	Inputs		:
	Outputs		:
	Return		:
----------------------------------------------------------------------------*/
sdword isoundmixerdecode(sbyte *readptr, real32 *writeptr1, real32 *writeptr2, ubyte *exponent, sdword size, uword bitrate)
{
#ifdef _MACOSX_FIX_SOUND
    return 0;
#else
    sbyte tempblock[FQ_LEN];

	memset(tempblock, 0, FQ_LEN);
	memcpy(tempblock, readptr, (bitrate >> 3));

	// Check size
	if(size > dctsize) size=dctsize;
	
	fqDequantBlock((char *)tempblock, writeptr1, writeptr2, exponent,
					FQ_LEN, bitrate, size);

	return (bitrate >> 3);
#endif 
}

/*-----------------------------------------------------------------------------
	Name		:
	Description	:
	Inputs		:
	Outputs		:
	Return		:
----------------------------------------------------------------------------*/	
static Uint8 oddbuf[MIX_BLOCK_SIZE];
static udword oddbufpos = 0;

void isoundmixerqueueSDL(Uint8 *stream, int len)
{
	udword size_written = 0;

	/* dbgAssertOrIgnore((len % MIX_BLOCK_SIZE) == 0); */

//GE01
//This code is different, so I'll just put my code in a different block then come back
//to fix it once I know that sound is working....
#ifdef HW_PTR_64_BROKEN	// this skips and makes noise; the original code in #else works ok on X86_64
    while (size_written + MIX_BLOCK_SIZE < len) {
        // process 256 samples, 16-bit (independent of # of channels)
        isoundmixerprocess(stream + size_written,
                FQ_SIZE * sizeof(short),
                NULL, 0);
        /* write(adump_fd, lpvWritePtr, FQ_SIZE * sizeof(short)); */
        size_written += MIX_BLOCK_SIZE;
    }
    if (size_written < len) {    //Don't have a full block, so problem with memory overwrites..
        isoundmixerprocess(stream + size_written,
//              FQ_SIZE * sizeof(short),
                (len - size_written) /2,
                NULL, 0);
        size_written += (len - size_written);
    }
        
#else
	if (oddbufpos > 0) {
		memcpy(stream, oddbuf + oddbufpos, MIX_BLOCK_SIZE - oddbufpos);
		size_written += MIX_BLOCK_SIZE - oddbufpos;
		oddbufpos = 0;
	}

	while (size_written < len) {
		// process 256 samples, 16-bit (independent of # of channels)
		if (size_written + MIX_BLOCK_SIZE > len) {
			isoundmixerprocess(oddbuf, FQ_SIZE * sizeof(short), NULL, 0);
			oddbufpos = len - size_written;
			memcpy(stream + size_written, oddbuf, oddbufpos);
			size_written += oddbufpos;
		} else {
			isoundmixerprocess(stream + size_written,
					FQ_SIZE * sizeof(short),
					NULL, 0);
			/* write(adump_fd, lpvWritePtr, FQ_SIZE * sizeof(short)); */
			size_written += MIX_BLOCK_SIZE;
		}
	}
#endif
}

void soundfeedercb(void *userdata, Uint8 *stream, int len)
{
	memset(stream, 0, len);
	if (mixer.status >= SOUND_PLAYING) {
		isoundmixerqueueSDL(stream, len);
	}
	else {
		memset(stream, 0, len);
		if (mixer.status == SOUND_STOPPED &&
				!bSoundPaused && !bSoundDeactivated)
		{
			mixer.status = SOUND_PLAYING;
		}
	}
	if (mixer.status == SOUND_STOPPING)
	{
		/* check and see if its done yet */
		if (mixer.timeout <= mixerticks)
		{
			mixer.timeout = 0;
			mixer.status = SOUND_STOPPED;
			SDL_PauseAudio(TRUE);
		}
	}
	if (bSoundPaused && (mixer.status == SOUND_PLAYING))
	{
		mixer.status = SOUND_STOPPING;
	}
	if (bSoundDeactivated && (mixer.status == SOUND_PLAYING))
	{
		mixer.status = SOUND_STOPPED;
		SDL_PauseAudio(TRUE);
	}
}
