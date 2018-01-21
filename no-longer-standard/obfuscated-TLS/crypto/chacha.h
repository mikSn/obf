/*
 * Copyright 2015-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

//OBF: Adapted from OpenSSL 1.1.0g, file internal/chacha.h:

/*
 * ChaCha20_ctr32 encrypts |len| bytes from |inp| with the given key and
 * nonce and writes the result to |out|, which may be equal to |inp|.
 * The |key| is not 32 bytes of verbatim key material though, but the
 * said material collected into 8 32-bit elements array in host byte
 * order. Same approach applies to nonce: the |counter| argument is
 * pointer to concatenated nonce and counter values collected into 4
 * 32-bit elements. This, passing crypto material collected into 32-bit
 * elements as opposite to passing verbatim byte vectors, is chosen for
 * efficiency in multi-call scenarios.
 */

/*
 * You can notice that there is no key setup procedure. Because it's
 * as trivial as collecting bytes into 32-bit elements, it's reckoned
 * that below macro is sufficient.
 */
#define CHACHA_U8TOU32(p)  ( \
                ((unsigned int)(p)[0])     | ((unsigned int)(p)[1]<<8) | \
                ((unsigned int)(p)[2]<<16) | ((unsigned int)(p)[3]<<24)  )

#define CHACHA_KEY_SIZE		32
#define CHACHA_CTR_SIZE		16
#define CHACHA_BLK_SIZE		64

//OBF: Adapted from OpenSSL 1.1.0g, file chacha_enc.c

/* Adapted from the public domain code by D. Bernstein from SUPERCOP. */

typedef unsigned int u32;
typedef unsigned char u8;
typedef union {
    u32 u[16];
    u8 c[64];
} chacha_buf;

# define ROTATE(v, n) (((v) << (n)) | ((v) >> (32 - (n))))

# define U32TO8_LITTLE(p, v) do { \
                                (p)[0] = (u8)(v >>  0); \
                                (p)[1] = (u8)(v >>  8); \
                                (p)[2] = (u8)(v >> 16); \
                                (p)[3] = (u8)(v >> 24); \
                                } while(0)

/* QUARTERROUND updates a, b, c, d with a ChaCha "quarter" round. */
# define QUARTERROUND(a,b,c,d) ( \
                x[a] += x[b], x[d] = ROTATE((x[d] ^ x[a]),16), \
                x[c] += x[d], x[b] = ROTATE((x[b] ^ x[c]),12), \
                x[a] += x[b], x[d] = ROTATE((x[d] ^ x[a]), 8), \
                x[c] += x[d], x[b] = ROTATE((x[b] ^ x[c]), 7)  )

/* chacha_core performs 20 rounds of ChaCha on the input words in
 * |input| and writes the 64 output bytes to |output|. */
ITHARE_OBF_FORCEINLINE void chacha20_core(chacha_buf *output, const u32 input[16])
{
    u32 x[16];
    int i;
    const union {
        long one;
        char little;
    } is_endian = { 1 };

    memcpy(x, input, sizeof(x));

    for (i = 20; i > 0; i -= 2) {
        QUARTERROUND(0, 4, 8, 12);
        QUARTERROUND(1, 5, 9, 13);
        QUARTERROUND(2, 6, 10, 14);
        QUARTERROUND(3, 7, 11, 15);
        QUARTERROUND(0, 5, 10, 15);
        QUARTERROUND(1, 6, 11, 12);
        QUARTERROUND(2, 7, 8, 13);
        QUARTERROUND(3, 4, 9, 14);
    }

    if (is_endian.little) {
        for (i = 0; i < 16; ++i)
            output->u[i] = x[i] + input[i];
    } else {
        for (i = 0; i < 16; ++i)
            U32TO8_LITTLE(output->c + 4 * i, (x[i] + input[i]));
    }
}

inline void ChaCha20_ctr32(unsigned char *out, const unsigned char *inp,
                    size_t len, const unsigned int key[8],
                    const unsigned int counter[4])
{
    u32 input[16];
    chacha_buf buf;
    size_t todo, i;

    /* sigma constant "expand 32-byte k" in little-endian encoding */
    input[0] = ((u32)'e') | ((u32)'x'<<8) | ((u32)'p'<<16) | ((u32)'a'<<24);
    input[1] = ((u32)'n') | ((u32)'d'<<8) | ((u32)' '<<16) | ((u32)'3'<<24);
    input[2] = ((u32)'2') | ((u32)'-'<<8) | ((u32)'b'<<16) | ((u32)'y'<<24);
    input[3] = ((u32)'t') | ((u32)'e'<<8) | ((u32)' '<<16) | ((u32)'k'<<24);

    input[4] = key[0];
    input[5] = key[1];
    input[6] = key[2];
    input[7] = key[3];
    input[8] = key[4];
    input[9] = key[5];
    input[10] = key[6];
    input[11] = key[7];

    input[12] = counter[0];
    input[13] = counter[1];
    input[14] = counter[2];
    input[15] = counter[3];

    while (len > 0) {
        todo = sizeof(buf);
        if (len < todo)
            todo = len;

        chacha20_core(&buf, input);

        for (i = 0; i < todo; i++)
            out[i] = inp[i] ^ buf.c[i];
        out += todo;
        inp += todo;
        len -= todo;

        /*
         * Advance 32-bit counter. Note that as subroutine is so to
         * say nonce-agnostic, this limited counter width doesn't
         * prevent caller from implementing wider counter. It would
         * simply take two calls split on counter overflow...
         */
        input[12]++;
    }
}

//OBF: Adapted from OpenSSL 1.1.0g, CHACHA part of file crypto/evp/e_chacha20_poly1305.c

struct EVP_CHACHA_KEY {
    union {
        double align;   /* this ensures even sizeof(EVP_CHACHA_KEY)%8==0 */
        unsigned int d[CHACHA_KEY_SIZE / 4];
    } key;
    unsigned int  counter[CHACHA_CTR_SIZE / 4];
    unsigned char buf[CHACHA_BLK_SIZE];
    unsigned int  partial_len;
};

struct EVP_CIPHER_CTX {
	EVP_CHACHA_KEY cipher_data;
};

#define data(ctx)   (&ctx->cipher_data)

inline int chacha_init_key(EVP_CIPHER_CTX *ctx,
                           const unsigned char user_key[CHACHA_KEY_SIZE],
                           const unsigned char iv[CHACHA_CTR_SIZE], int enc)
{
    EVP_CHACHA_KEY *key = data(ctx);
    unsigned int i;

    if (user_key)
        for (i = 0; i < CHACHA_KEY_SIZE; i+=4) {
            key->key.d[i/4] = CHACHA_U8TOU32(user_key+i);
        }

    if (iv)
        for (i = 0; i < CHACHA_CTR_SIZE; i+=4) {
            key->counter[i/4] = CHACHA_U8TOU32(iv+i);
        }

    key->partial_len = 0;

    return 1;
}

inline int chacha_cipher(EVP_CIPHER_CTX * ctx, unsigned char *out,
                         const unsigned char *inp, size_t len)
{
    EVP_CHACHA_KEY *key = data(ctx);
    unsigned int n, rem, ctr32;

    if ((n = key->partial_len)) {
        while (len && n < CHACHA_BLK_SIZE) {
            *out++ = *inp++ ^ key->buf[n++];
            len--;
        }
        key->partial_len = n;

        if (len == 0)
            return 1;

        if (n == CHACHA_BLK_SIZE) {
            key->partial_len = 0;
            key->counter[0]++;
            if (key->counter[0] == 0)
                key->counter[1]++;
        }
    }

    rem = (unsigned int)(len % CHACHA_BLK_SIZE);
    len -= rem;
    ctr32 = key->counter[0];
    while (len >= CHACHA_BLK_SIZE) {
        size_t blocks = len / CHACHA_BLK_SIZE;
        /*
         * 1<<28 is just a not-so-small yet not-so-large number...
         * Below condition is practically never met, but it has to
         * be checked for code correctness.
         */
        if (sizeof(size_t)>sizeof(unsigned int) && blocks>(1U<<28))
            blocks = (1U<<28);

        /*
         * As ChaCha20_ctr32 operates on 32-bit counter, caller
         * has to handle overflow. 'if' below detects the
         * overflow, which is then handled by limiting the
         * amount of blocks to the exact overflow point...
         */
        ctr32 += (unsigned int)blocks;
        if (ctr32 < blocks) {
            blocks -= ctr32;
            ctr32 = 0;
        }
        blocks *= CHACHA_BLK_SIZE;
        ChaCha20_ctr32(out, inp, blocks, key->key.d, key->counter);
        len -= blocks;
        inp += blocks;
        out += blocks;

        key->counter[0] = ctr32;
        if (ctr32 == 0) key->counter[1]++;
    }

    if (rem) {
        memset(key->buf, 0, sizeof(key->buf));
        ChaCha20_ctr32(key->buf, key->buf, CHACHA_BLK_SIZE,
                       key->key.d, key->counter);
        for (n = 0; n < rem; n++)
            out[n] = inp[n] ^ key->buf[n];
        key->partial_len = rem;
    }

    return 1;
}

#if 0
static const EVP_CIPHER chacha20 = {
    NID_chacha20,
    1,                      /* block_size */
    CHACHA_KEY_SIZE,        /* key_len */
    CHACHA_CTR_SIZE,        /* iv_len, 128-bit counter in the context */
    EVP_CIPH_CUSTOM_IV | EVP_CIPH_ALWAYS_CALL_INIT,
    chacha_init_key,
    chacha_cipher,
    NULL,
    sizeof(EVP_CHACHA_KEY),
    NULL,
    NULL,
    NULL,
    NULL
};

const EVP_CIPHER *EVP_chacha20(void)
{
    return (&chacha20);
}
#endif
