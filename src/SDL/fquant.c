#include <stdio.h>
#include <string.h>
#include "fqcodec.h"
#include "fquant.h"

// CAT: the fq dequantiser follows the binary (decision 11; fqInitDequant 0x00558fd0, fqDequantBlock 0x00559030,
//      fqSUnpack 0x00559470). GoK reconstructed these functions (no Relic source exists for the fq library); the exe
//      differs in three places: the band and spectrum counters are bytes (they wrap at 256), the band loop stops once
//      the band index passes 25, and the spectrum loops stop at an index above 127. The 32-bit bit-field load is
//      unaligned on x86; it is read here through memcpy (same value, no misaligned access), and every shift is done
//      on unsigned 32-bit values with the x86 shift-count mask (& 31), as the exe's code does.

static float gSDBlock[FQ_EXP];
static unsigned short gCBBlock[FQ_CBNUM] = { 0x0000, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0009, 0x000b, 0x000d, 0x000f, 0x0011, 0x0014, 0x0017, 0x001b, 0x001f, 0x0025, 0x002b, 0x0033, 0x003e, 0x004a, 0x0059, 0x006e, 0x008b, 0x00b4, 0x0100 };

int fqInitDequant(void) {
	unsigned int i;

	gSDBlock[0] = FQ_SCALE;
	for (i = 1; i < FQ_EXP; i++)
		gSDBlock[i] = gSDBlock[i - 1] * FQ_SCALE;
	// CAT: the mask factor is a float division, then the multiply (fqInitDequant 0x00558fd0: 1.0f / (float)n)
	for (i = 0; i < FQ_EXP; i++)
		gSDBlock[i] = (FQ_FMASK / (float)((1 << (i + 1)) - 1)) * gSDBlock[i];

	return OK;
}

// CAT: the exe's unaligned 32-bit load of the bit stream (fqSUnpack 0x00559470: *(uint *)(buf + (offset >> 3)))
static inline uint32_t fqLoad32(const char *buf, udword offset) {
	uint32_t w;

	memcpy(&w, buf + (offset >> 3), sizeof(w));
	return w;
}

sdword fqSUnpack(udword bits, udword offset, char *buf) {
	uint32_t sh = (offset - ((offset >> 3) << 3)) & 31;
	uint32_t val;

	val = (((((uint32_t)1 << (bits & 31)) - 1) << sh) & fqLoad32(buf, offset)) >> sh;
	if ((val >> ((bits - 1) & 31)) == 1)
		val = (uint32_t)-((val << ((33 - bits) & 31)) >> ((33 - bits) & 31));   // sign and magnitude
	return (sdword)val;
}

udword fqUnpack(udword bits, udword offset, char *buf) {
	uint32_t sh = (offset - ((offset >> 3) << 3)) & 31;

	return (((((uint32_t)1 << (bits & 31)) - 1) << sh) & fqLoad32(buf, offset)) >> sh;
}

int fqDequantBlock(char *aQBlock, float *aFPBlock, float *aFSBlock, unsigned char *aEBlock, udword nLen, udword nRate, udword nSize) {
	udword pos, ua, ub, uc, ud, i, j;
	ubyte count, u, e;
	sdword s;

	memset(aFPBlock, 0, FQ_SIZE * sizeof(float));
	memset(aFSBlock, 0, FQ_SIZE * sizeof(float));

	pos = 0;
	ua = fqUnpack(2, pos, aQBlock);
	pos += 2;
	ub = fqUnpack(3, pos, aQBlock);
	pos += 3;
	uc = fqUnpack(2, pos, aQBlock);
	pos += 2;
	ud = fqUnpack(4, pos, aQBlock);
	pos += 4;

	if ((ua & 1) == 1)
		memset(aEBlock, 0, FQ_SIZE);

	if (ub > 0 && uc > 0) {
		count = 0;
		for (i = 0; pos < nRate && pos < nLen << 3; ) {
			u = (ubyte)fqUnpack(ub, pos, aQBlock);
			pos += ub;
			if (i > 0 && u == 0)
				break;
			count = (ubyte)(count + u);
			if (count > FQ_CBNUM - 2)                       // CAT: band index past 25 ends the bands
				break;
			u = (ubyte)fqUnpack(uc, pos, aQBlock);
			pos += uc;
			for (j = gCBBlock[count]; j < gCBBlock[count + 1]; j++) {
				aEBlock[j] = u;
			}
			if (++i > FQ_CBNUM - 2)
				break;
		}
	}

	if (nSize > 0 && ud > 0) {
		count = 0;
		for (i = 0; i < FQ_SIZE && pos < nRate && pos < nLen << 3; i++) {
			u = (ubyte)fqUnpack(ud, pos, aQBlock);              // CAT: the step is a byte (uchar in the exe)
			pos += ud;
			if (i > 0 && u == 0)
				break;
			count = (ubyte)(count + u);
			e = aEBlock[count];
			s = fqSUnpack(e + 2, pos, aQBlock);
			pos += e + 2;
			if (s != 0 && count < nSize && aEBlock[count] < 6) {
				if (count > 0x7f)                           // CAT: an index above 127 ends the spectrum
					break;
				aFPBlock[count] = (float)s * gSDBlock[e];
			}
		}
		if ((ua & 2) == 2) {
			memcpy(aFSBlock, aFPBlock, FQ_SIZE * sizeof(float));
		} else {
			count = 0;
			for (i = 0; i < FQ_SIZE && pos < nRate && pos < nLen << 3; i++) {
				u = (ubyte)fqUnpack(ud, pos, aQBlock);
				pos += ud;
				if (i > 0 && u == 0)
					break;
				count = (ubyte)(count + u);
				e = aEBlock[count];
				s = fqSUnpack(e + 2, pos, aQBlock);
				pos += e + 2;
				if (s != 0 && count < nSize && aEBlock[count] < 6) {
					if (count > 0x7f)
						break;
					aFSBlock[count] = (float)s * gSDBlock[e];
				}
			}
		}
	}

	return OK;
}
