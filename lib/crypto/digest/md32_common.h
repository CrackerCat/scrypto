/* ====================================================================
 * Copyright (c) 1999-2007 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ==================================================================== */

#ifndef OPENSSL_HEADER_MD32_COMMON_H
#define OPENSSL_HEADER_MD32_COMMON_H

#include <base.h>

#include <assert.h>

#if defined(__cplusplus)
extern "C" {
#endif


/* This is a generic 32-bit "collector" for message digest algorithms. It
 * collects input character stream into chunks of 32-bit values and invokes the
 * block function that performs the actual hash calculations. To make use of
 * this mechanism, the following macros must be defined before including
 * md32_common.h.
 *
 * One of |DATA_ORDER_IS_BIG_ENDIAN| or |DATA_ORDER_IS_LITTLE_ENDIAN| must be
 * defined to specify the byte order of the input stream.
 *
 * |HASH_CBLOCK| must be defined as the integer block size, in bytes.
 *
 * |HASH_CTX| must be defined as the name of the context structure, which must
 * have at least the following members:
 *
 *     typedef struct <name>_state_st {
 *       uint32_t h[<chaining length> / sizeof(uint32_t)];
 *       uint32_t Nl, Nh;
 *       uint8_t data[HASH_CBLOCK];
 *       unsigned num;
 *       ...
 *     } <NAME>_CTX;
 *
 * <chaining length> is the output length of the hash in bytes, before
 * any truncation (e.g. 64 for SHA-224 and SHA-256, 128 for SHA-384 and
 * SHA-512).
 *
 * |HASH_UPDATE| must be defined as the name of the "Update" function to
 * generate.
 *
 * |HASH_TRANSFORM| must be defined as the  the name of the "Transform"
 * function to generate.
 *
 * |HASH_FINAL| must be defined as the name of "Final" function to generate.
 *
 * |HASH_BLOCK_DATA_ORDER| must be defined as the name of the "Block" function.
 * That function must be implemented manually. It must be capable of operating
 * on *unaligned* input data in its original (data) byte order. It must have
 * this signature:
 *
 *     void HASH_BLOCK_DATA_ORDER(uint32_t *state, const uint8_t *data,
 *                                size_t num);
 *
 * It must update the hash state |state| with |num| blocks of data from |data|,
 * where each block is |HASH_CBLOCK| bytes; i.e. |data| points to a array of
 * |HASH_CBLOCK * num| bytes. |state| points to the |h| member of a |HASH_CTX|,
 * and so will have |<chaining length> / sizeof(uint32_t)| elements.
 *
 * |HASH_MAKE_STRING(c, s)| must be defined as a block statement that converts
 * the hash state |c->h| into the output byte order, storing the result in |s|.
 */

#if !defined(DATA_ORDER_IS_BIG_ENDIAN) && !defined(DATA_ORDER_IS_LITTLE_ENDIAN)
#error "DATA_ORDER must be defined!"
#endif

#ifndef HASH_CBLOCK
#error "HASH_CBLOCK must be defined!"
#endif
#ifndef HASH_CTX
#error "HASH_CTX must be defined!"
#endif

#ifndef HASH_UPDATE
#error "HASH_UPDATE must be defined!"
#endif
#ifndef HASH_TRANSFORM
#error "HASH_TRANSFORM must be defined!"
#endif
#ifndef HASH_FINAL
#error "HASH_FINAL must be defined!"
#endif

#ifndef HASH_BLOCK_DATA_ORDER
#error "HASH_BLOCK_DATA_ORDER must be defined!"
#endif

#ifndef HASH_MAKE_STRING
#error "HASH_MAKE_STRING must be defined!"
#endif

#if defined(DATA_ORDER_IS_BIG_ENDIAN)

#if !defined(PEDANTIC) && defined(__GNUC__) && __GNUC__ >= 2 && \
    !defined(OPENSSL_NO_ASM)
#if defined(OPENSSL_X86) || defined(OPENSSL_X86_64)
/* The first macro gives a ~30-40% performance improvement in SHA-256 compiled
 * with gcc on P4. This can only be done on x86, where unaligned data fetches
 * are possible. */
#define HOST_c2l(c, l)                       \
  (void)({                                   \
    uint32_t r = *((const uint32_t *)(c));   \
    __asm__("bswapl %0" : "=r"(r) : "0"(r)); \
    (c) += 4;                                \
    (l) = r;                                 \
  })
#define HOST_l2c(l, c)                       \
  (void)({                                   \
    uint32_t r = (l);                        \
    __asm__("bswapl %0" : "=r"(r) : "0"(r)); \
    *((uint32_t *)(c)) = r;                  \
    (c) += 4;                                \
    r;                                       \
  })
#elif defined(__aarch64__) && defined(__BYTE_ORDER__)
#if defined(__ORDER_LITTLE_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define HOST_c2l(c, l)                                                 \
  (void)({                                                             \
    uint32_t r;                                                        \
    __asm__("rev %w0, %w1" : "=r"(r) : "r"(*((const uint32_t *)(c)))); \
    (c) += 4;                                                          \
    (l) = r;                                                           \
  })
#define HOST_l2c(l, c)                                      \
  (void)({                                                  \
    uint32_t r;                                             \
    __asm__("rev %w0, %w1" : "=r"(r) : "r"((uint32_t)(l))); \
    *((uint32_t *)(c)) = r;                                 \
    (c) += 4;                                               \
    r;                                                      \
  })
#elif defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define HOST_c2l(c, l) (void)((l) = *((const uint32_t *)(c)), (c) += 4)
#define HOST_l2c(l, c) (*((uint32_t *)(c)) = (l), (c) += 4, (l))
#endif /* __aarch64__ && __BYTE_ORDER__ */
#endif /* ARCH */
#endif /* !PEDANTIC && GNUC && !NO_ASM */

#ifndef HOST_c2l
#define HOST_c2l(c, l)                        \
  (void)(l = (((uint32_t)(*((c)++))) << 24),  \
         l |= (((uint32_t)(*((c)++))) << 16), \
         l |= (((uint32_t)(*((c)++))) << 8), l |= (((uint32_t)(*((c)++)))))
#endif

#ifndef HOST_l2c
#define HOST_l2c(l, c)                             \
  (void)(*((c)++) = (uint8_t)(((l) >> 24) & 0xff), \
         *((c)++) = (uint8_t)(((l) >> 16) & 0xff), \
         *((c)++) = (uint8_t)(((l) >> 8) & 0xff),  \
         *((c)++) = (uint8_t)(((l)) & 0xff))
#endif

#elif defined(DATA_ORDER_IS_LITTLE_ENDIAN)

#if defined(OPENSSL_X86) || defined(OPENSSL_X86_64)
/* See comment in DATA_ORDER_IS_BIG_ENDIAN section. */
#define HOST_c2l(c, l) (void)((l) = *((const uint32_t *)(c)), (c) += 4)
#define HOST_l2c(l, c) (void)(*((uint32_t *)(c)) = (l), (c) += 4, l)
#endif /* OPENSSL_X86 || OPENSSL_X86_64 */

#ifndef HOST_c2l
#define HOST_c2l(c, l)                                                     \
  (void)(l = (((uint32_t)(*((c)++)))), l |= (((uint32_t)(*((c)++))) << 8), \
         l |= (((uint32_t)(*((c)++))) << 16),                              \
         l |= (((uint32_t)(*((c)++))) << 24))
#endif

#ifndef HOST_l2c
#define HOST_l2c(l, c)                             \
  (void)(*((c)++) = (uint8_t)(((l)) & 0xff),       \
         *((c)++) = (uint8_t)(((l) >> 8) & 0xff),  \
         *((c)++) = (uint8_t)(((l) >> 16) & 0xff), \
         *((c)++) = (uint8_t)(((l) >> 24) & 0xff))
#endif

#endif /* DATA_ORDER */

int HASH_UPDATE(HASH_CTX *c, const void *data_, size_t len) {
  const uint8_t *data = data_;

  if (len == 0) {
    return 1;
  }

  uint32_t l = c->Nl + (((uint32_t)len) << 3);
  if (l < c->Nl) {
    /* Handle carries. */
    c->Nh++;
  }
  c->Nh += (uint32_t)(len >> 29);
  c->Nl = l;

  size_t n = c->num;
  if (n != 0) {
    if (len >= HASH_CBLOCK || len + n >= HASH_CBLOCK) {
      memcpy(c->data + n, data, HASH_CBLOCK - n);
      HASH_BLOCK_DATA_ORDER(c->h, c->data, 1);
      n = HASH_CBLOCK - n;
      data += n;
      len -= n;
      c->num = 0;
      /* Keep |c->data| zeroed when unused. */
      memset(c->data, 0, HASH_CBLOCK);
    } else {
      memcpy(c->data + n, data, len);
      c->num += (unsigned)len;
      return 1;
    }
  }

  n = len / HASH_CBLOCK;
  if (n > 0) {
    HASH_BLOCK_DATA_ORDER(c->h, data, n);
    n *= HASH_CBLOCK;
    data += n;
    len -= n;
  }

  if (len != 0) {
    c->num = (unsigned)len;
    memcpy(c->data, data, len);
  }
  return 1;
}


void HASH_TRANSFORM(HASH_CTX *c, const uint8_t *data) {
  HASH_BLOCK_DATA_ORDER(c->h, data, 1);
}


int HASH_FINAL(uint8_t *md, HASH_CTX *c) {
  /* |c->data| always has room for at least one byte. A full block would have
   * been consumed. */
  size_t n = c->num;
  assert(n < HASH_CBLOCK);
  c->data[n] = 0x80;
  n++;

  /* Fill the block with zeros if there isn't room for a 64-bit length. */
  if (n > (HASH_CBLOCK - 8)) {
    memset(c->data + n, 0, HASH_CBLOCK - n);
    n = 0;
    HASH_BLOCK_DATA_ORDER(c->h, c->data, 1);
  }
  memset(c->data + n, 0, HASH_CBLOCK - 8 - n);

  /* Append a 64-bit length to the block and process it. */
  uint8_t *p = c->data + HASH_CBLOCK - 8;
#if defined(DATA_ORDER_IS_BIG_ENDIAN)
  HOST_l2c(c->Nh, p);
  HOST_l2c(c->Nl, p);
#elif defined(DATA_ORDER_IS_LITTLE_ENDIAN)
  HOST_l2c(c->Nl, p);
  HOST_l2c(c->Nh, p);
#endif
  assert(p == c->data + HASH_CBLOCK);
  HASH_BLOCK_DATA_ORDER(c->h, c->data, 1);
  c->num = 0;
  memset(c->data, 0, HASH_CBLOCK);

  HASH_MAKE_STRING(c, md);
  return 1;
}


#if defined(__cplusplus)
} /* extern C */
#endif

#endif /* OPENSSL_HEADER_MD32_COMMON_H */
