/* Per Bruce Leidl, author of the obfuscated-openssh patch...

"Obfuscated-openssh is just a patch to the main OpenSSH code.  It is released under the same license as OpenSSH."

Therefore the following has been included.

Mr. Hinky Dink */

/*
This file is part of the OpenSSH software.

The licences which components of this software fall under are as
follows.  First, we will summarize and say that all components
are under a BSD licence, or a licence more free than that.

OpenSSH contains no GPL code.

1)
     * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
     *                    All rights reserved
     *
     * As far as I am concerned, the code I have written for this software
     * can be used freely for any purpose.  Any derived versions of this
     * software must be clearly marked as such, and if the derived work is
     * incompatible with the protocol description in the RFC file, it must be
     * called by a name other than "ssh" or "Secure Shell".

    [Tatu continues]
     *  However, I am not implying to give any licenses to any patents or
     * copyrights held by third parties, and the software includes parts that
     * are not under my direct control.  As far as I know, all included
     * source code is used in accordance with the relevant license agreements
     * and can be used freely for any purpose (the GNU license being the most
     * restrictive); see below for details.

    [However, none of that term is relevant at this point in time.  All of
    these restrictively licenced software components which he talks about
    have been removed from OpenSSH, i.e.,

     - RSA is no longer included, found in the OpenSSL library
     - IDEA is no longer included, its use is deprecated
     - DES is now external, in the OpenSSL library
     - GMP is no longer used, and instead we call BN code from OpenSSL
     - Zlib is now external, in a library
     - The make-ssh-known-hosts script is no longer included
     - TSS has been removed
     - MD5 is now external, in the OpenSSL library
     - RC4 support has been replaced with ARC4 support from OpenSSL
     - Blowfish is now external, in the OpenSSL library

    [The licence continues]

    Note that any information and cryptographic algorithms used in this
    software are publicly available on the Internet and at any major
    bookstore, scientific library, and patent office worldwide.  More
    information can be found e.g. at "http://www.cs.hut.fi/crypto".

    The legal status of this program is some combination of all these
    permissions and restrictions.  Use only at your own responsibility.
    You will be responsible for any legal consequences yourself; I am not
    making any claims whether possessing or using this is legal or not in
    your country, and I am not taking any responsibility on your behalf.


			    NO WARRANTY

    BECAUSE THE PROGRAM IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY
    FOR THE PROGRAM, TO THE EXTENT PERMITTED BY APPLICABLE LAW.  EXCEPT WHEN
    OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES
    PROVIDE THE PROGRAM "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED
    OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE ENTIRE RISK AS
    TO THE QUALITY AND PERFORMANCE OF THE PROGRAM IS WITH YOU.  SHOULD THE
    PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY SERVICING,
    REPAIR OR CORRECTION.

    IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
    WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY AND/OR
    REDISTRIBUTE THE PROGRAM AS PERMITTED ABOVE, BE LIABLE TO YOU FOR DAMAGES,
    INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING
    OUT OF THE USE OR INABILITY TO USE THE PROGRAM (INCLUDING BUT NOT LIMITED
    TO LOSS OF DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY
    YOU OR THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER
    PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGES.

2)
    The 32-bit CRC compensation attack detector in deattack.c was
    contributed by CORE SDI S.A. under a BSD-style license.

     * Cryptographic attack detector for ssh - source code
     *
     * Copyright (c) 1998 CORE SDI S.A., Buenos Aires, Argentina.
     *
     * All rights reserved. Redistribution and use in source and binary
     * forms, with or without modification, are permitted provided that
     * this copyright notice is retained.
     *
     * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
     * WARRANTIES ARE DISCLAIMED. IN NO EVENT SHALL CORE SDI S.A. BE
     * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY OR
     * CONSEQUENTIAL DAMAGES RESULTING FROM THE USE OR MISUSE OF THIS
     * SOFTWARE.
     *
     * Ariel Futoransky <futo@core-sdi.com>
     * <http://www.core-sdi.com>

3)
    ssh-keyscan was contributed by David Mazieres under a BSD-style
    license.

     * Copyright 1995, 1996 by David Mazieres <dm@lcs.mit.edu>.
     *
     * Modification and redistribution in source and binary forms is
     * permitted provided that due credit is given to the author and the
     * OpenBSD project by leaving this copyright notice intact.

4)
    The Rijndael implementation by Vincent Rijmen, Antoon Bosselaers
    and Paulo Barreto is in the public domain and distributed
    with the following license:

     * @version 3.0 (December 2000)
     *
     * Optimised ANSI C code for the Rijndael cipher (now AES)
     *
     * @author Vincent Rijmen <vincent.rijmen@esat.kuleuven.ac.be>
     * @author Antoon Bosselaers <antoon.bosselaers@esat.kuleuven.ac.be>
     * @author Paulo Barreto <paulo.barreto@terra.com.br>
     *
     * This code is hereby placed in the public domain.
     *
     * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ''AS IS'' AND ANY EXPRESS
     * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
     * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
     * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE
     * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
     * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
     * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
     * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
     * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
     * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
     * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

5)
    One component of the ssh source code is under a 3-clause BSD license,
    held by the University of California, since we pulled these parts from
    original Berkeley code.

     * Copyright (c) 1983, 1990, 1992, 1993, 1995
     *      The Regents of the University of California.  All rights reserved.
     *
     * Redistribution and use in source and binary forms, with or without
     * modification, are permitted provided that the following conditions
     * are met:
     * 1. Redistributions of source code must retain the above copyright
     *    notice, this list of conditions and the following disclaimer.
     * 2. Redistributions in binary form must reproduce the above copyright
     *    notice, this list of conditions and the following disclaimer in the
     *    documentation and/or other materials provided with the distribution.
     * 3. Neither the name of the University nor the names of its contributors
     *    may be used to endorse or promote products derived from this software
     *    without specific prior written permission.
     *
     * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
     * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
     * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
     * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
     * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
     * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
     * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
     * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
     * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
     * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
     * SUCH DAMAGE.

6)
    Remaining components of the software are provided under a standard
    2-term BSD licence with the following names as copyright holders:

	Markus Friedl
	Theo de Raadt
	Niels Provos
	Dug Song
	Aaron Campbell
	Damien Miller
	Kevin Steves
	Daniel Kouril
	Wesley Griffin
	Per Allansson
	Nils Nordman
	Simon Wilkinson

    Portable OpenSSH additionally includes code from the following copyright
    holders, also under the 2-term BSD license:

	Ben Lindstrom
	Tim Rice
	Andre Lucas
	Chris Adams
	Corinna Vinschen
	Cray Inc.
	Denis Parker
	Gert Doering
	Jakob Schlyter
	Jason Downs
	Juha Yrj?l?
	Michael Stone
	Networks Associates Technology, Inc.
	Solar Designer
	Todd C. Miller
	Wayne Schroeder
	William Jones
	Darren Tucker
	Sun Microsystems
	The SCO Group
	Daniel Walsh

     * Redistribution and use in source and binary forms, with or without
     * modification, are permitted provided that the following conditions
     * are met:
     * 1. Redistributions of source code must retain the above copyright
     *    notice, this list of conditions and the following disclaimer.
     * 2. Redistributions in binary form must reproduce the above copyright
     *    notice, this list of conditions and the following disclaimer in the
     *    documentation and/or other materials provided with the distribution.
     *
     * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
     * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
     * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
     * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
     * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
     * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
     * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
     * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
     * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
     * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

8) Portable OpenSSH contains the following additional licenses:

    a) md5crypt.c, md5crypt.h

	 * "THE BEER-WARE LICENSE" (Revision 42):
	 * <phk@login.dknet.dk> wrote this file.  As long as you retain this
	 * notice you can do whatever you want with this stuff. If we meet
	 * some day, and you think this stuff is worth it, you can buy me a
	 * beer in return.   Poul-Henning Kamp

    b) snprintf replacement

	* Copyright Patrick Powell 1995
	* This code is based on code written by Patrick Powell
	* (papowell@astart.com) It may be used for any purpose as long as this
	* notice remains intact on all source code distributions

    c) Compatibility code (openbsd-compat)

       Apart from the previously mentioned licenses, various pieces of code
       in the openbsd-compat/ subdirectory are licensed as follows:

       Some code is licensed under a 3-term BSD license, to the following
       copyright holders:

	Todd C. Miller
	Theo de Raadt
	Damien Miller
	Eric P. Allman
	The Regents of the University of California
	Constantin S. Svintsoff

	* Redistribution and use in source and binary forms, with or without
	* modification, are permitted provided that the following conditions
	* are met:
	* 1. Redistributions of source code must retain the above copyright
	*    notice, this list of conditions and the following disclaimer.
	* 2. Redistributions in binary form must reproduce the above copyright
	*    notice, this list of conditions and the following disclaimer in the
	*    documentation and/or other materials provided with the distribution.
	* 3. Neither the name of the University nor the names of its contributors
	*    may be used to endorse or promote products derived from this software
	*    without specific prior written permission.
	*
	* THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
	* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
	* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
	* ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
	* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
	* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
	* OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
	* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
	* LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
	* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
	* SUCH DAMAGE.

       Some code is licensed under an ISC-style license, to the following
       copyright holders:

	Internet Software Consortium.
	Todd C. Miller
	Reyk Floeter
	Chad Mynhier

	* Permission to use, copy, modify, and distribute this software for any
	* purpose with or without fee is hereby granted, provided that the above
	* copyright notice and this permission notice appear in all copies.
	*
	* THE SOFTWARE IS PROVIDED "AS IS" AND TODD C. MILLER DISCLAIMS ALL
	* WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
	* OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL TODD C. MILLER BE LIABLE
	* FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
	* WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
	* OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
	* CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

       Some code is licensed under a MIT-style license to the following
       copyright holders:

	Free Software Foundation, Inc.

	* Permission is hereby granted, free of charge, to any person obtaining a  *
	* copy of this software and associated documentation files (the            *
	* "Software"), to deal in the Software without restriction, including      *
	* without limitation the rights to use, copy, modify, merge, publish,      *
	* distribute, distribute with modifications, sublicense, and/or sell       *
	* copies of the Software, and to permit persons to whom the Software is    *
	* furnished to do so, subject to the following conditions:                 *
	*                                                                          *
	* The above copyright notice and this permission notice shall be included  *
	* in all copies or substantial portions of the Software.                   *
	*                                                                          *
	* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
	* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
	* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
	* IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
	* DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
	* OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
	* THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
	*                                                                          *
	* Except as contained in this notice, the name(s) of the above copyright   *
	* holders shall not be used in advertising or otherwise to promote the     *
	* sale, use or other dealings in this Software without prior written       *
	* authorization.                                                           *
	****************************************************************************/


#include "defines.h"
#include <openssl/evp.h>
#include <openssl/rc4.h>
#include "arc4random.h"
#include "obfuscate.h"

#define OBFUSCATE_KEY_LENGTH 	16
#define OBFUSCATE_SEED_LENGTH	16
#define OBFUSCATE_HASH_ITERATIONS 6000
#define OBFUSCATE_MAX_PADDING	8192
#define OBFUSCATE_MAGIC_VALUE	0x0BF5CA7E

static RC4_KEY rc4_input;
static RC4_KEY rc4_output;

static const char *obfuscate_keyword = NULL;


struct seed_msg {
	u_char seed_buffer[OBFUSCATE_SEED_LENGTH];
	u_int32_t magic;
	u_int32_t padding_length;
	u_char padding[];
};

static int generate_key_pair(const u_char *, u_char *, u_char *);
static int generate_key(const u_char *, const u_char *, u_int, u_char *);
static void set_keys(const u_char *, const u_char *);
static int initialize(const u_char *); // removed server flag

/*
 * Client calls this - removed socket for PoTTY
 */

extern int oblen;

//THIS WORKS
void * 
obfuscate_send_seed(void)
{
	struct seed_msg *seed; 
	int i;
	
	u_int32_t rnd = 0;
	u_int message_length;
	u_int32_t padding_length;
	
	padding_length = arc4random() % OBFUSCATE_MAX_PADDING;
	message_length = padding_length + sizeof(struct seed_msg);
	seed = malloc((size_t)message_length); // ssh.c frees mem

    /*
	for(i = 0; i < OBFUSCATE_SEED_LENGTH; i++) {
		if(i % 4 == 0)
			rnd = arc4random();
		seed->seed_buffer[i] = rnd & 0xff;
		rnd >>= 8;
	}
    */
    // Temporary test.
    // TODO: Add longer seed to accommodate both randomness and prefix; add well-formed prefix; randomize prefix
    const char* prefix = "GET / HTTP/1.0\r\n";
    memcpy(seed, prefix, OBFUSCATE_SEED_LENGTH);

	seed->magic = htonl(OBFUSCATE_MAGIC_VALUE);   // WTF?
	seed->padding_length = htonl(padding_length); // ditto
	for(i = 0; i < (int)padding_length; i++) {
		if(i % 4 == 0)
			rnd = arc4random();
		seed->padding[i] = rnd & 0xff;
	}
	if ( initialize(seed->seed_buffer)){
		free(seed);
		return NULL;
	}
	obfuscate_output(((u_char *)seed) + OBFUSCATE_SEED_LENGTH,
		message_length - OBFUSCATE_SEED_LENGTH);
	oblen=message_length;

	return (void *)seed;
 }

void
obfuscate_set_keyword(const char *keyword)
{
	obfuscate_keyword = keyword;
}

void
obfuscate_input(u_char *buffer, u_int buffer_len)
{
	RC4(&rc4_input, buffer_len, buffer, buffer);
}

void
obfuscate_output(u_char *buffer, u_int buffer_len)
{
	RC4(&rc4_output, buffer_len, buffer, buffer);
}

static int
initialize(const u_char *seed)
{
	u_char client_to_server_key[OBFUSCATE_KEY_LENGTH];
	u_char server_to_client_key[OBFUSCATE_KEY_LENGTH];
	
	if( generate_key_pair(seed, client_to_server_key, server_to_client_key))
		return 1;

	set_keys(server_to_client_key, client_to_server_key);

	return 0;
}

static int
generate_key_pair(const u_char *seed, u_char *client_to_server_key, u_char *server_to_client_key)
{
	if( generate_key(seed, "client_to_server", strlen("client_to_server"), client_to_server_key))
		return 1;
	if( generate_key(seed, "server_to_client", strlen("server_to_client"), server_to_client_key))
		return 1;
	return 0;
}

static int
generate_key(const u_char *seed, const u_char *iv, u_int iv_len, u_char *key_data)
{
	EVP_MD_CTX ctx;
	u_char md_output[EVP_MAX_MD_SIZE];
	int md_len;
	int i;
	u_char *buffer;
	u_char *p;
	u_int buffer_length;

	buffer_length = OBFUSCATE_SEED_LENGTH + iv_len;
	if(obfuscate_keyword)
		buffer_length += strlen(obfuscate_keyword);

	p = buffer = malloc(buffer_length);

	memcpy(p, seed, OBFUSCATE_SEED_LENGTH);
	p += OBFUSCATE_SEED_LENGTH;

	if(obfuscate_keyword) {
		memcpy(p, obfuscate_keyword, strlen(obfuscate_keyword));
		p += strlen(obfuscate_keyword);
	}
	memcpy(p, iv, iv_len);

	EVP_DigestInit(&ctx, EVP_sha1());
	EVP_DigestUpdate(&ctx, buffer, buffer_length);
	EVP_DigestFinal(&ctx, md_output, &md_len);

	free(buffer);

	for(i = 0; i < OBFUSCATE_HASH_ITERATIONS; i++) {
		EVP_DigestInit(&ctx, EVP_sha1());
		EVP_DigestUpdate(&ctx, md_output, md_len);
		EVP_DigestFinal(&ctx, md_output, &md_len);
	}

	// Error designed to fail in CLI environment
	// ssh.c now handles with bombout(msg)
	if(md_len < OBFUSCATE_KEY_LENGTH) return 1; 
		//fatal("Cannot derive obfuscation keys from hash length of %d", md_len);

	memcpy(key_data, md_output, OBFUSCATE_KEY_LENGTH);

	return 0;
}

static void
set_keys(const u_char *input_key, const u_char *output_key)
{
	RC4_set_key(&rc4_input, OBFUSCATE_KEY_LENGTH, input_key);
	RC4_set_key(&rc4_output, OBFUSCATE_KEY_LENGTH, output_key);
}