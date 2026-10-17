#include <stdio.h>
#include <string.h>
#include <math.h>
#include "fqcodec.h"
#include "dct.h"

#ifndef PI
#define PI		3.14159265358979323846F
#endif

static float gWHBlock[FQ_HSIZE];
static float gWBlock[FQ_SIZE];
static float gWDBlock[FQ_DSIZE];

static float gCHBlock[FQ_HSIZE];
static float gCBlock[FQ_SIZE];
static float gCDBlock[FQ_DSIZE];

// CAT: the time-block writer follows the binary (fqWriteTBlock 0x00558bc0). GoK's version ran every sample through a
//      soft limiter (compress(), a GoK addition the exe does not have) and restarted the source index at 0 for the
//      second buffer; the exe rounds each sample with Relic's rint (fqRint), clamps it to [-0x8000, 0x7fff], keeps one
//      source index across both buffers and fails (-1) past 0x200 source samples.
static int fqWriteTBlockBuf(float *aLBlock, float *aRBlock, short nChan, short *pBuf, udword nSize, udword *pSrc) {
	udword i;
	sdword v = 0;

	if ((pBuf != 0) && (nSize > 0)) {
		for (i = 0; i + 1 < nSize; i += 2) {
			if (*pSrc > FQ_DSIZE)
				return ERR;
			v = fqRint(aLBlock[*pSrc]);
			v = (v < 0x8000) ? ((v < -0x8000) ? -0x8000 : v) : 0x7fff;
			pBuf[i] = (short)v;
			if (nChan > 1) {
				v = fqRint(aRBlock[*pSrc]);
				v = (v < 0x8000) ? ((v < -0x8000) ? -0x8000 : v) : 0x7fff;
			}
			pBuf[i + 1] = (short)v;
			(*pSrc)++;
		}
	}
	return OK;
}

int fqWriteTBlock(float *aLBlock, float *aRBlock, short nChan, void *pBuf1, udword nSize1, void *pBuf2, udword nSize2) {
	udword src = 0;

	if (fqWriteTBlockBuf(aLBlock, aRBlock, nChan, pBuf1, nSize1, &src) != OK)
		return ERR;
	return fqWriteTBlockBuf(aLBlock, aRBlock, nChan, pBuf2, nSize2, &src);
}

int fqDecOver(float *aFPBlock, float *aFSBlock, float *aTPBlock, float *aTSBlock, float *aCBlock, float *aWBlock, udword nSize) {
	float buf[FQ_DSIZE];
	udword h = nSize >> 1, i;

	// CAT: the exe copies the whole FQ_DSIZE block, whatever nSize is (fqDecOver 0x00558800)
	memcpy(aTPBlock, aTSBlock, FQ_DSIZE * sizeof(float));
	idct(aFPBlock, buf, aCBlock, nSize);
	idct(aFSBlock, aTSBlock, aCBlock, nSize);
	for(i = 0; i < h; i++) {
		aTPBlock[i + h] = (aTPBlock[i + h] * aWBlock[i + h]) + (buf[i] * aWBlock[i]);
		aTSBlock[i] = (buf[i + h] * aWBlock[i + h]) + (aTSBlock[i] * aWBlock[i]);
	}

	return OK;
}

int fqDecBlock(float *aFPBlock, float *aFSBlock, float *aTPBlock, float *aTSBlock, int nMode, int nFact) {
	float buf[FQ_DSIZE];
	unsigned int i;

	if(nMode == 0) {
		memset(aTSBlock, 0, FQ_DSIZE * sizeof(float));
		// CAT: the window steps are the exe's float constants PI/128, PI/256, PI/512 (0x0072bdd8, 0x0072bdd4,
		//      0x0072bdd0), multiplied by the index at full precision; an Initdct failure fails the init
		//      (fqDecBlock 0x005588c0)
		for (i = 0; i < FQ_HSIZE; i++)
			gWHBlock[i] = (float)sin((double)i * (double)(PI / (float)FQ_HSIZE));
		for (i = 0; i < FQ_SIZE; i++)
			gWBlock[i] = (float)sin((double)i * (double)(PI / (float)FQ_SIZE));
		for (i = 0; i < FQ_DSIZE; i++)
			gWDBlock[i] = (float)sin((double)i * (double)(PI / (float)FQ_DSIZE));
		if (Initdct(gCHBlock, FQ_HSIZE) == ERR)
			return ERR;
		if (Initdct(gCBlock, FQ_SIZE) == ERR)
			return ERR;
		return (Initdct(gCDBlock, FQ_DSIZE) == ERR) ? ERR : OK;
	} else {
		if(nFact == 4) {
			fqDecOver(aFPBlock, aFSBlock, aTPBlock, aTSBlock, gCHBlock, gWHBlock, FQ_HSIZE);
		} else {
			if(nFact == 2) {
				if(nMode == 4) {
					fqDecOver(aFPBlock, aFSBlock, buf, aTSBlock, gCHBlock, gWHBlock, FQ_HSIZE);
					for(i = 0; i < FQ_SIZE - 1; i += 2) {
						aTPBlock[i] = buf[i >> 1];
						aTPBlock[i + 1] = buf[i >> 1];
					}
				} else {
					fqDecOver(aFPBlock, aFSBlock, aTPBlock, aTSBlock, gCBlock, gWBlock, FQ_SIZE);
				}
			} else {
				if(nMode == 4) {
					fqDecOver(aFPBlock, aFSBlock, buf, aTSBlock, gCHBlock, gWHBlock, FQ_HSIZE);
					for(i = 0; i < FQ_DSIZE - 1; i += 4) {
						aTPBlock[i] = buf[i >> 2];
						aTPBlock[i + 1] = buf[i >> 2];
						aTPBlock[i + 2] = buf[i >> 2];
						aTPBlock[i + 3] = buf[i >> 2];
					}
				} else {
					if(nMode == 2) {
						fqDecOver(aFPBlock, aFSBlock, buf, aTSBlock, gCBlock, gWBlock, FQ_SIZE);
						for(i = 0; i < FQ_DSIZE - 1; i += 2) {
							aTPBlock[i] = buf[i >> 1];
							aTPBlock[i + 1] = buf[i >> 1];
						}
					} else {
						fqDecOver(aFPBlock, aFSBlock, aTPBlock, aTSBlock, gCDBlock, gWDBlock, FQ_DSIZE);
					}
				}
			}
		}
	}
	return OK;
}
