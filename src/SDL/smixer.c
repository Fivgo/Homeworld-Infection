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
