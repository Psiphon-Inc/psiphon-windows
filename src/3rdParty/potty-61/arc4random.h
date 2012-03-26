/*==========================================================================
 * Not sure where I found this, but it is required for the obfuscated KEX
 * handshake.
 *
 * --Mr. Hinky Dink
 * =========================================================================
 */

/* ==========================================================================
 * arc4random.h - Re-implementation of OpenBSD's arc4random().
 * --------------------------------------------------------------------------
 * Copyright (c) 2008  William Ahern
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
 * NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 * ==========================================================================
 */
#ifndef BSD_STDLIB_ARC4RANDOM_H
#define BSD_STDLIB_ARC4RANDOM_H

#ifndef HAVE_ARC4RANDOM
#if defined __OpenBSD__	\
 || defined __FreeBSD__	\
 || defined __NetBSD__	\
 || defined __APPLE__
#define HAVE_ARC4RANDOM	1
#else
#define HAVE_ARC4RANDOM	0
#endif
#endif
typedef unsigned long  uint32_t;
#if !HAVE_ARC4RANDOM

#if !_WIN32
#include <stdint.h>	/* uint32_t */
#endif

#include <stdarg.h>	/* va_list va_start() va_arg() va_end() */

#include <string.h>	/* memset(3) */

#include <time.h>	/* time(3) clock(3) */

#if _WIN32
#include <process.h>	/* _getpid() */
#include <windows.h>	/* LoadLibrary() GetProcAddress() FreeLibrary() */
#include <wincrypt.h>	/* CryptAcquireContext() CryptGenRandom() CryptReleaseContext() */



#else
#include <unistd.h>	/* getpid(2) */
#include <sys/types.h>
#include <sys/sysctl.h>	/* CTL_KERN KERN_RANDOM RANDOM_UUID sysctl(2) */
#endif

#if _REENTRANT
#if _WIN32
#include <windows.h>	/* CRITICAL_SECTION InitializeCriticalSection() EnterCriticalSection() LeaveCriticalSection() InterlockedCompareExchange() InterlockedExchange() */
#else
#include <pthread.h>	/* pthread_mutex_t pthread_mutex_lock(3) pthread_mutex_unlock(3) */
#endif
#endif


#define ARC4_STIR		0
#define ARC4_ADDRANDOM		1
#define ARC4_RANDOM		2
#define ARC4_RANDOM_BUF		3
#define ARC4_RANDOM_UNIFORM	4

#if _WIN32
static unsigned long arc4random_(int op, volatile int locked, ...) {
#else
static inline uint32_t arc4random_(int op, volatile int locked, ...) {
#endif
	static struct {
		unsigned char s[256];
		unsigned char i, j;
		int c;
#if _REENTRANT
#if _WIN32
		struct {
			volatile LONG init;
			CRITICAL_SECTION cs;
		} mux;
#else
		pthread_mutex_t mux;
#endif
#endif
	} arc4 = {
		{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
		  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
		  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
		  0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
		  0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
		  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
		  0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
		  0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
		  0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
		  0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57,
		  0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
		  0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
		  0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
		  0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
		  0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
		  0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
		  0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
		  0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
		  0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
		  0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
		  0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
		  0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7,
		  0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
		  0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
		  0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
		  0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
		  0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
		  0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7,
		  0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
		  0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
		  0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff }, };
	unsigned long rval = 0;

#if _REENTRANT
	if (!locked++) {
#if _WIN32
		volatile LONG init;

		do {
			switch (init = InterlockedCompareExchange((LONG *)&arc4.mux.init, 1, 0)) {
			case 2: /* initialized already. */
				break;
			case 1: /* try again. */
				Sleep(1);

				break;
			case 0: /* must initialize. */
				InitializeCriticalSection(&arc4.mux.cs);

				InterlockedExchange((LONG *)&arc4.mux.init, 2);

				break;
			}
		} while (1 == init);

		EnterCriticalSection(&arc4.mux.cs);
#else
		pthread_mutex_lock(&arc4.mux);
#endif
	}
#endif


	switch (op) {
	case ARC4_STIR: {
		union {
			time_t tim;
			clock_t clk;
			int pid;
			unsigned char bytes[128];
		} rnd;
		int n;

		memset(&rnd, '\0', sizeof rnd);

		rnd.tim	^= time(0);
		rnd.clk	^= clock();

#if _WIN32
		rnd.pid	^= _getpid();
#else
		rnd.pid	^= getpid();
#endif

#if __linux
		{
			int mib[] = { CTL_KERN, KERN_RANDOM, RANDOM_UUID };
			unsigned char uuid[128];
			size_t len, n;

			for (len = 0; len < sizeof uuid; len += n) {
				n	= sizeof uuid - len;

				if (0 != sysctl(mib, sizeof mib / sizeof mib[0], &uuid[len], &n, (void *)0, 0))
					break;
			}

			for (n = 0; n < len && n < sizeof rnd; n++)
				rnd.bytes[n]	^= uuid[n];

			goto stir;
		}
#endif

#if _WIN32
		{
			HMODULE lib	= 0;
			HCRYPTPROV ctx	= 0;
			// I had to add the WINAPI casts below to stop crashing in Debug build
			//   - Hinky
			struct {
				BOOL (WINAPI *acquire)(HCRYPTPROV *, LPCSTR, LPCSTR, DWORD, DWORD);
				BOOL (WINAPI *genrandom)(HCRYPTPROV, DWORD, BYTE *);
				BOOL (WINAPI *release)(HCRYPTPROV, DWORD);
			} crypt;
			unsigned char bytes[sizeof rnd.bytes];

			if (!(lib = LoadLibrary(TEXT("ADVAPI32.DLL"))))
				goto unload;

			if (!(crypt.acquire = (BOOL (WINAPI*)())GetProcAddress(lib, "CryptAcquireContextA"))
			||  !(crypt.genrandom = (BOOL (WINAPI*)())GetProcAddress(lib, "CryptGenRandom"))
			||  !(crypt.release = (BOOL (WINAPI*)())GetProcAddress(lib, "CryptReleaseContext")))
				goto unload;

			if (!crypt.acquire(&ctx, 0, 0, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
				goto unload;

			if (!crypt.genrandom(ctx, sizeof bytes, bytes))
				goto unload;

			for (n = 0; n < sizeof bytes && n < sizeof rnd.bytes; n++)
				rnd.bytes[n]	^= bytes[n];

unload:
			if (ctx)
				crypt.release(ctx, 0);

			if (lib)
				FreeLibrary(lib);

			goto stir;
		}
#endif

stir:
		arc4random_(ARC4_ADDRANDOM, locked, rnd.bytes, sizeof rnd.bytes);

		arc4.c	= 1600000 + (256 / 4);

		/*
		 * OpenBSD only discards 256 bytes. Probably safer to
		 * discard at least 768, but w'ever.
		 */
		for (n = 0; n < (256 / 4); n++)
			arc4random_(ARC4_RANDOM, locked);

		break;
	}

	case ARC4_ADDRANDOM: {
		va_list ap;
		unsigned char *src;
		int len, n;
		unsigned char si;

		va_start(ap, locked);

		src	= va_arg(ap, unsigned char *);
		len	= va_arg(ap, int);

		va_end(ap);

		if (len <= 0)
			break;

		/*
		 * From David Mazieres' OpenBSD code. Seems to be a novel
		 * admixture of RC4's key setup and byte stream generator.
		 */
		arc4.i--;
		for (n = 0; n < 256; n++) {
			arc4.i	= (arc4.i + 1) % 256;
			si	= arc4.s[arc4.i];
			arc4.j	= (arc4.j + si + src[n % len]) % 256;

			arc4.s[arc4.i]	= arc4.s[arc4.j];
			arc4.s[arc4.j]	= si;
		}
		arc4.j	= arc4.i;

		break;
	}

	case ARC4_RANDOM: {
		unsigned char si, sj;
		int n;

		if (arc4.c <= 0)
			arc4random_(ARC4_STIR, locked);

		/*
		 * Again from David Maziere's OpenBSD code.
		 */
		for (n = 0; n < 4; n++) {
			rval	<<= 8;

			arc4.i		= (arc4.i + 1) % 256;
			si		= arc4.s[arc4.i];
			arc4.j		= (arc4.j + si) % 256;
			sj		= arc4.s[arc4.j];
			arc4.s[arc4.i]	= sj;
			arc4.s[arc4.j]	= si;

			rval		|= arc4.s[(si + sj) % 256];
		}

		arc4.c	-= 4;

		break;
	}

	case ARC4_RANDOM_BUF: {
		va_list ap;
		unsigned char *dst;
		size_t lim;

		va_start(ap, locked);

		dst	= va_arg(ap, unsigned char *);
		lim	= va_arg(ap, size_t);

		va_end(ap);

		while (lim > 0) {
			uint32_t r	= arc4random_(ARC4_RANDOM, locked);

			switch (lim % 4) {
			case 0:
				dst[--lim]	= (r >> 24U);
			case 3:
				dst[--lim]	= (r >> 16U);
			case 2:
				dst[--lim]	= (r >> 8U);
			case 1:
				dst[--lim]	= (r >> 0U);
			} /* switch (lim % 4) */
		} /* while (lim) */

		break;
	}

	default:
		break;
	} /* switch (op) */

#if _REENTRANT
	if (locked == 1) {
#if _WIN32
		LeaveCriticalSection(&arc4.mux.cs);
#else
		pthread_mutex_unlock(&arc4.mux);
#endif
	}
#endif

	return rval;
} /* arc4random_() */

#define arc4random_stir()		arc4random_(ARC4_STIR, 0)
#define arc4random_addrandom(p, n)	arc4random_(ARC4_ADDRANDOM, 0, (p), (n))
#define arc4random()			arc4random_(ARC4_RANDOM, 0)
#define arc4random_buf(p, n)		arc4random_(ARC4_RANDOM_BUF, 0, (p), (n))

#ifndef HAVE_ARC4RANDOM_BUF
#define HAVE_ARC4RANDOM_BUF	1
#endif

#ifndef HAVE_ARC4RANDOM_UNIFORM
#define HAVE_ARC4RANDOM_UNIFORM	1
#endif

#endif /* !HAVE_ARC4RANDOM */


#ifndef HAVE_ARC4RANDOM_BUF
#if defined __OpenBSD__ || defined __FreeBSD__

#include <sys/param.h>	/* OpenBSD, __FreeBSD_version */

#if OpenBSD >= 200811 || __FreeBSD_version >= 800107
#define HAVE_ARC4RANDOM_BUF	1
#else
#define HAVE_ARC4RANDOM_BUF	0
#endif

#else
#define HAVE_ARC4RANDOM_BUF	0
#endif
#endif

#if !HAVE_ARC4RANDOM_BUF

#include <stddef.h>	/* size_t */

static inline void arc4random_buf(void *dst, size_t lim) {
		while (lim > 0) {
			unsigned long r	= arc4random();

			switch (lim % 4) {
			case 0:
				((unsigned char *)dst)[--lim]	= (r >> 24U);
			case 3:
				((unsigned char *)dst)[--lim]	= (r >> 16U);
			case 2:
				((unsigned char *)dst)[--lim]	= (r >> 8U);
			case 1:
				((unsigned char *)dst)[--lim]	= (r >> 0U);
			} /* switch (lim % 4) */
		} /* while (lim) */
} /* arc4random_buf() */

#endif /* !HAVE_ARC4RANDOM_BUF */


#endif /* !BSD_STDLIB_ARC4RANDOM_H */


#if 0

void *malloc(size_t);
void free(void *);

#include <stdio.h>	/* printf(3) */
#include <ctype.h>	/* isdigit(3) */

int main(int argc, char *argv[]) {
	unsigned long i, j, n = 1600000;
	unsigned char *p;

	if (argc > 1) {
		for (n = 0; isdigit(*argv[1]); argv[1]++) {
			n	*= 10;
			n	+= *argv[1] - '0';
		}
	}

	for (i = 0; i < n; i++)
		printf("%lu\n", (unsigned long)arc4random());

	if (argc > 2) {
		for (n = 0; isdigit(*argv[2]); argv[2]++) {
			n	*= 10;
			n	+= *argv[2] - '0';
		}
	} else
		n	= 0xffff & arc4random();

	p	= malloc(n);

	arc4random_buf(p, n);

	for (i = 0; i < n;) {
		char hex[16]	= "0123456789abcdef";
		char ln[56];

		for (j = 0; i < n && j < 16; j++, i++) {
			ln[(j * 3) + 0]	= hex[0x0f & (p[i] >> 4)];
			ln[(j * 3) + 1]	= hex[0x0f & (p[i] >> 0)];
			ln[(j * 3) + 2]	= ' ';
		}

		fwrite(ln, 3, j, stdout);
		fputc('\n', stdout);
	}

	free(p);

	return 0;
} /* main() */

#endif

