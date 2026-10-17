#ifndef ___SOUNDCMN_H
#define ___SOUNDCMN_H

#include <stddef.h>
#include <SDL2/SDL.h>

#include "File.h"
#include "fqcodec.h"
#include "soundlow.h"
#include "Types.h"

#define NUM_FADE_BLOCKS		20

#define DELAY_BUF_SIZE		(18 * FQ_SIZE) // approx 200 msec delay buffer
#define SND_BLOCK_TIME      ((float)FQ_SLICE / 1000.0F) // approx 11.6 msec (in secs)

#define VCE_BACKWARDS_COMPATIBLE    1   //backwards compatible with older version with the "DATA" before the "INFO"

/* structures */

/* Basic replacement for WAVEFORMATEX. */
typedef struct
{
    Uint16 format;
    Uint16 channels;
    Uint32 frequency;
    Uint32 avgBytesPerSecond;
    Uint16 blockAlign;
/*    Unit16 bitsPerSample; */
} SDLWAVEFORMAT;

typedef struct
{
	udword			id;
	udword			priority;
	sdword			pitch;
	smemsize        dataoffset;
	smemsize		datasize;
	smemsize		loopstart;
	smemsize		loopend;
	
	sword			bitrate;
	sword			flags;
	
	ubyte			volume;
	sbyte			pan;
	sbyte			pad[2];

	SDLWAVEFORMAT	waveformat;
	sword			wavepad;
} PATCH;

typedef struct
{
    udword          id;
    udword          priority;
    sdword          pitch;
    sdword            dataoffset;
    sdword            datasize;
    sdword            loopstart;
    sdword            loopend;

    sword           bitrate;
    sword           flags;

    ubyte           volume;
    sbyte           pan;
    sbyte           pad[2];

    SDLWAVEFORMAT   waveformat;
    sword           wavepad;
} PATCH_DISK;

// the bank file's patch: 0x38 bytes, the exe's PATCH (SNDgetpatch 0x00581a90 strides 0x38; bitrate +0x1C, flags +0x1E,
// waveformat.frequency +0x28)
_Static_assert(sizeof(PATCH_DISK) == 0x38, "PATCH_DISK is the exe's 0x38-byte PATCH");
_Static_assert(offsetof(PATCH_DISK, bitrate) == 0x1C && offsetof(PATCH_DISK, flags) == 0x1E, "PATCH_DISK +0x1C/+0x1E");
_Static_assert(offsetof(PATCH_DISK, waveformat) + offsetof(SDLWAVEFORMAT, frequency) == 0x28, "PATCH_DISK +0x28");
#ifndef HW_PTR_64
_Static_assert(sizeof(PATCH) == 0x38, "ILP32 PATCH is the exe's 0x38 bytes");
#endif

typedef struct
{
	udword			id;
	udword			checksum;
	sdword			numpatches;
	sdword			firstpatch;
} BANK;

/* The patch table of a loaded bank. In the exe (32-bit) the patches start in place at BANK.firstpatch (+0x0C, a
   0x38-byte PATCH; SNDgetpatch 0x00581a90, splayFPRVL 0x00581720: bank + 0xC + patnum * 0x38). HWI LP64: PATCH holds
   pointer-sized fields (8-byte alignment), so the converted bank (loadbankAndConvertTo64Bit, SoundEvent.c) puts its
   table at the next 8-byte boundary, +0x10; reading it at +0x0C was the "misaligned address for type 'struct PATCH'"
   UBSan report of every run. */
#ifdef HW_PTR_64
#define SND_BANK_PATCHES_OFFSET  ((offsetof(BANK, firstpatch) + _Alignof(PATCH) - 1) & ~(_Alignof(PATCH) - 1))
#else
#define SND_BANK_PATCHES_OFFSET  offsetof(BANK, firstpatch)
#endif
#define SNDbankpatches(bank)     ((PATCH *)((ubyte *)(bank) + SND_BANK_PATCHES_OFFSET))

typedef struct
{
	sdword			priority;
	sdword			status;
	sdword			handle;

	bool32			mute;

	PATCH			*ppatch;
	
	real32			volume;
	real32			volfade;
	
	sword			voltarget;
	sword			volticksleft;
	
	sword			pan;
	sword			pantarget;
	sword			panfade;
	sword			panticksleft;

	real32			pitch;
	real32			pitchtarget;
	real32			pitchfade;
	sdword			pitchticksleft;
	
	sword			heading;
	sword			bitrate;

	sdword			fqsize;
	sdword			looping;

	sdword			numchannels;	//new

	sbyte			*freqdata;
	sbyte			*currentpos;
	sbyte			*endpos;
	
	sdword			amountread;

	real32			shift;
	
	real32			volfactorL;
	real32			volfactorR;

	ubyte			exponentblockL[FQ_SIZE];
	ubyte			exponentblockR[FQ_SIZE];

	real32			mixbuffer1[FQ_SIZE];
	real32			mixbuffer2[FQ_SIZE];

	real32			mixbuffer1R[FQ_SIZE];
	real32			mixbuffer2R[FQ_SIZE];

	real32			filter[SOUND_EQ_SIZE];
	real32			cardiodfilter[SOUND_EQ_SIZE];
	bool32			usecardiod;
} CHANNEL;		/* 80 bytes + mixbuffers */

typedef struct
{
	filehandle		fhandle;	/* this is the stream file handle */
	smemsize		offset;		/* this is the offset within that file */
	sdword			flags;		/* looping? */
	sdword			size;
	
	sword			vol;
	sword			pan;
	
	sword			numchannels;
	sword			bitrate;
	
	real32			fadetime;
	real32			volfactorL;
	real32			volfactorR;

	real32			silencetime;

	sbyte			*nextplay;

	PATCH			*pmixPatch;
	sdword			mixHandle;
	sword			mixLevel;
	sword			pad;

	STREAMDELAY		*delay;
	STREAMEQ		*eq;
	EFFECT			*effect;

    sdword          actornum;   //needed for subtitles
    sdword          speechEvent;//ditto
} STREAMQUEUE;

typedef struct
{
	sdword			ID;			/* this is the ID to check that this is a valid stream */
	sdword			size;		/* this is the size of this stream */
} STREAMHEADER;

#define UNINITIALISED_STREAM_HEADER  {0, 0}

typedef struct
{
	STREAMHEADER	header;

	ubyte			*buffer;	/* this is a pointer to the buffer to read data from the stream file */
	sdword			buffersize;	/* this is the size of the stream buffer / SOUND_STREAM_NUM_BUFFERS */
	sdword			blocksize;	/* this is the size of the stream buffer / SOUND_STREAM_NUM_BUFFERS */

	ubyte			*writepos;
	ubyte			*bufferend;

	sdword			blockstatus[2];

//	sdword			bufferstatus[SOUND_STREAM_NUM_BUFFERS];
//	sdword			readbuffer;	/* this is which buffer is being read to the DirectSound buffer */
//	sdword			writebuffer;/* this is which buffer is being written into from the CD */
	sdword			readblock;	/* this is which buffer is being read to the DirectSound buffer */
	sdword			writeblock;	/* this is which buffer is being written into from the CD */

	sdword			dataleft;	/* amount of data left to read in this stream */
	smemsize		lastpos;

//	sdword			writepos;	/* location in the DirectSound buffer to write at */

	sdword			status;

	sdword			filepos;

	sdword			playing;

	sdword			handle;		/* this is the sound handle that this stream is being played on */
	
	sdword			queueindex;	/* this is which queue to play next */
	sdword			writeindex;	/* this is which queue to play next */
	sdword			numqueued;
	sdword			numtoplay;

	sdword			playindex;

	STREAMQUEUE		queue[SOUND_MAX_STREAM_QUEUE];

	sdword			delaypos1;
	sdword			delaypos2;
	real32			delaybuffer1[DELAY_BUF_SIZE];
	real32			delaybuffer2[DELAY_BUF_SIZE];

    real32          dataPeriod; //inverse of data rate, seconds per byte
} STREAM;

typedef struct
{
	FILE*			fp;	        /* this is the O/S handle to the file */
	sdword			handle;		/* this handle is used for indexing into the streamfiles array */
} STREAMFILE;


typedef struct
{
	sdword status;
	udword timeout;
} SOUNDCOMPONENT;

/* functions */
sdword isoundmixerinit(SDL_AudioSpec *aspec);
void isoundmixerrestore(void);
int isoundstreamupdate(void *dummy);

sdword SNDreleasebuffer(CHANNEL *pchan);
sdword SNDchannel(sdword handle);
void SNDcalcvolpan(CHANNEL *pchan);

sdword SNDcreatehandle(sdword channel);
PATCH *SNDgetpatch(void *bankaddress, sdword patnum);

sdword smixCreateDSoundBuffer(SDLWAVEFORMAT *pcmwf);
sdword smixInitMixBuffer(SDL_AudioSpec *aspec);

sdword streamStartThread(void);
void streamWaitThread(void);    // HWI-FIX: joins the "soundstream" thread at quit (sstream.c)

#endif
