/* XMRig
 * Copyright 2010      Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler      <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466    <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee   <jayddee246@gmail.com>
 * Copyright 2016-2017 XMRig       <support@xmrig.com>
 * Copyright 2018      Sebastian Stolzenberg <https://github.com/sebastianstolzenberg>
 * Copyright 2018      BenDroid    <ben@graef.in>
 *
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __CRYPTONIGHT_X86_H__
#define __CRYPTONIGHT_X86_H__


#ifdef __GNUC__
#   include <x86intrin.h>
#include <cfenv>

#else
#   include <intrin.h>
#   define __restrict__ __restrict
#endif


#include "crypto/CryptoNight.h"
#include "crypto/soft_aes.h"


extern "C"
{
#include "crypto/c_keccak.h"
#include "crypto/c_groestl.h"
#include "crypto/c_blake256.h"
#include "crypto/c_jh.h"
#include "crypto/c_skein.h"
}

#ifdef __GNUC__
#define LIKELY(X) __builtin_expect(X, 1)
#define UNLIKELY(X) __builtin_expect(X, 0)
#else
#define LIKELY(X) X
#define UNLIKELY(X) X
#endif


#if defined(__x86_64__) || defined(_M_AMD64)
#   define EXTRACT64(X) _mm_cvtsi128_si64(X)

#   ifdef __GNUC__
static inline uint64_t __umul128(uint64_t a, uint64_t b, uint64_t* hi)
{
    unsigned __int128 r = (unsigned __int128) a * (unsigned __int128) b;
    *hi = r >> 64;
    return (uint64_t) r;
}
#   else
#define __umul128 _umul128
#   endif
#elif defined(__i386__) || defined(_M_IX86)
#   define HI32(X) \
    _mm_srli_si128((X), 4)


#   define EXTRACT64(X) \
    ((uint64_t)(uint32_t)_mm_cvtsi128_si32(X) | \
    ((uint64_t)(uint32_t)_mm_cvtsi128_si32(HI32(X)) << 32))

static inline uint64_t __umul128(uint64_t multiplier, uint64_t multiplicand, uint64_t *product_hi) {
    // multiplier   = ab = a * 2^32 + b
    // multiplicand = cd = c * 2^32 + d
    // ab * cd = a * c * 2^64 + (a * d + b * c) * 2^32 + b * d
    uint64_t a = multiplier >> 32;
    uint64_t b = multiplier & 0xFFFFFFFF;
    uint64_t c = multiplicand >> 32;
    uint64_t d = multiplicand & 0xFFFFFFFF;

    //uint64_t ac = a * c;
    uint64_t ad = a * d;
    //uint64_t bc = b * c;
    uint64_t bd = b * d;

    uint64_t adbc = ad + (b * c);
    uint64_t adbc_carry = adbc < ad ? 1 : 0;

    // multiplier * multiplicand = product_hi * 2^64 + product_lo
    uint64_t product_lo = bd + (adbc << 32);
    uint64_t product_lo_carry = product_lo < bd ? 1 : 0;
    *product_hi = (a * c) + (adbc >> 32) + (adbc_carry << 32) + product_lo_carry;

    return product_lo;
}
#endif

#ifdef _MSC_VER
#   define SET_ROUNDING_MODE() _control87(RC_DOWN, MCW_RC);
#else
#   define SET_ROUNDING_MODE() std::fesetround(FE_TOWARDZERO);
#endif

#   define SHUFFLE_PHASE_1(l, idx_, bx0, bx1, ax) \
{ \
   const __m128i chunk1 = _mm_load_si128((__m128i *)((l) + ((idx_) ^ 0x10))); \
   const __m128i chunk2 = _mm_load_si128((__m128i *)((l) + ((idx_) ^ 0x20))); \
   const __m128i chunk3 = _mm_load_si128((__m128i *)((l) + ((idx_) ^ 0x30))); \
   _mm_store_si128((__m128i *)((l) + ((idx_) ^ 0x10)), _mm_add_epi64(chunk3, bx1)); \
   _mm_store_si128((__m128i *)((l) + ((idx_) ^ 0x20)), _mm_add_epi64(chunk1, bx0)); \
   _mm_store_si128((__m128i *)((l) + ((idx_) ^ 0x30)), _mm_add_epi64(chunk2, ax)); \
}

#   define INTEGER_MATH_V2(idx, cl, cx) \
{ \
    const uint64_t sqrt_result = static_cast<uint64_t>(_mm_cvtsi128_si64(sqrt_result_xmm_##idx)); \
    const uint64_t cx0 = _mm_cvtsi128_si64(cx); \
    cl ^= static_cast<uint64_t>(_mm_cvtsi128_si64(division_result_xmm_##idx)) ^ (sqrt_result << 32); \
    const uint32_t d = static_cast<uint32_t>(cx0 + (sqrt_result << 1)) | 0x80000001UL; \
    const uint64_t cx1 = _mm_cvtsi128_si64(_mm_srli_si128(cx, 8)); \
    const uint64_t division_result = static_cast<uint32_t>(cx1 / d) + ((cx1 % d) << 32); \
    division_result_xmm_##idx = _mm_cvtsi64_si128(static_cast<int64_t>(division_result)); \
    sqrt_result_xmm_##idx = int_sqrt_v2(cx0 + division_result); \
}

#   define SHUFFLE_PHASE_2(l, idx, bx0, bx1, ax, lo, hi) \
{ \
    const __m128i chunk1 = _mm_xor_si128(_mm_load_si128((__m128i *)((l) + ((idx) ^ 0x10))), _mm_set_epi64x(lo, hi)); \
    const __m128i chunk2 = _mm_load_si128((__m128i *)((l) + ((idx) ^ 0x20))); \
    const __m128i chunk3 = _mm_load_si128((__m128i *)((l) + ((idx) ^ 0x30))); \
    hi ^= ((uint64_t*)((l) + ((idx) ^ 0x20)))[0]; \
    lo ^= ((uint64_t*)((l) + ((idx) ^ 0x20)))[1]; \
    _mm_store_si128((__m128i *)((l) + ((idx) ^ 0x10)), _mm_add_epi64(chunk3, bx1)); \
    _mm_store_si128((__m128i *)((l) + ((idx) ^ 0x20)), _mm_add_epi64(chunk1, bx0)); \
    _mm_store_si128((__m128i *)((l) + ((idx) ^ 0x30)), _mm_add_epi64(chunk2, ax)); \
}

static inline void do_blake_hash(const uint8_t *input, size_t len, uint8_t *output) {
    blake256_hash(output, input, len);
}


static inline void do_groestl_hash(const uint8_t *input, size_t len, uint8_t *output) {
    groestl(input, len * 8, output);
}


static inline void do_jh_hash(const uint8_t *input, size_t len, uint8_t *output) {
    jh_hash(32 * 8, input, 8 * len, output);
}


static inline void do_skein_hash(const uint8_t *input, size_t len, uint8_t *output) {
    xmr_skein(input, output);
}


void (* const extra_hashes[4])(const uint8_t *, size_t, uint8_t *) = {do_blake_hash, do_groestl_hash, do_jh_hash, do_skein_hash};


// This will shift and xor tmp1 into itself as 4 32-bit vals such as
// sl_xor(a1 a2 a3 a4) = a1 (a2^a1) (a3^a2^a1) (a4^a3^a2^a1)
static inline __m128i sl_xor(__m128i tmp1)
{
    __m128i tmp4;
    tmp4 = _mm_slli_si128(tmp1, 0x04);
    tmp1 = _mm_xor_si128(tmp1, tmp4);
    tmp4 = _mm_slli_si128(tmp4, 0x04);
    tmp1 = _mm_xor_si128(tmp1, tmp4);
    tmp4 = _mm_slli_si128(tmp4, 0x04);
    tmp1 = _mm_xor_si128(tmp1, tmp4);
    return tmp1;
}


template<uint8_t rcon>
static inline void aes_genkey_sub(__m128i* xout0, __m128i* xout2)
{
    __m128i xout1 = _mm_aeskeygenassist_si128(*xout2, rcon);
    xout1 = _mm_shuffle_epi32(xout1, 0xFF); // see PSHUFD, set all elems to 4th elem
    *xout0 = sl_xor(*xout0);
    *xout0 = _mm_xor_si128(*xout0, xout1);
    xout1 = _mm_aeskeygenassist_si128(*xout0, 0x00);
    xout1 = _mm_shuffle_epi32(xout1, 0xAA); // see PSHUFD, set all elems to 3rd elem
    *xout2 = sl_xor(*xout2);
    *xout2 = _mm_xor_si128(*xout2, xout1);
}


template<uint8_t rcon>
static inline void soft_aes_genkey_sub(__m128i* xout0, __m128i* xout2)
{
    __m128i xout1 = soft_aeskeygenassist<rcon>(*xout2);
    xout1 = _mm_shuffle_epi32(xout1, 0xFF); // see PSHUFD, set all elems to 4th elem
    *xout0 = sl_xor(*xout0);
    *xout0 = _mm_xor_si128(*xout0, xout1);
    xout1 = soft_aeskeygenassist<0x00>(*xout0);
    xout1 = _mm_shuffle_epi32(xout1, 0xAA); // see PSHUFD, set all elems to 3rd elem
    *xout2 = sl_xor(*xout2);
    *xout2 = _mm_xor_si128(*xout2, xout1);
}


template<bool SOFT_AES>
static inline void
aes_genkey(const __m128i* memory, __m128i* k0, __m128i* k1, __m128i* k2, __m128i* k3, __m128i* k4, __m128i* k5,
           __m128i* k6, __m128i* k7, __m128i* k8, __m128i* k9)
{
    __m128i xout0 = _mm_load_si128(memory);
    __m128i xout2 = _mm_load_si128(memory + 1);
    *k0 = xout0;
    *k1 = xout2;

    SOFT_AES ? soft_aes_genkey_sub<0x01>(&xout0, &xout2) : aes_genkey_sub<0x01>(&xout0, &xout2);
    *k2 = xout0;
    *k3 = xout2;

    SOFT_AES ? soft_aes_genkey_sub<0x02>(&xout0, &xout2) : aes_genkey_sub<0x02>(&xout0, &xout2);
    *k4 = xout0;
    *k5 = xout2;

    SOFT_AES ? soft_aes_genkey_sub<0x04>(&xout0, &xout2) : aes_genkey_sub<0x04>(&xout0, &xout2);
    *k6 = xout0;
    *k7 = xout2;

    SOFT_AES ? soft_aes_genkey_sub<0x08>(&xout0, &xout2) : aes_genkey_sub<0x08>(&xout0, &xout2);
    *k8 = xout0;
    *k9 = xout2;
}


template<bool SOFT_AES>
static inline void
aes_round(__m128i key, __m128i* x0, __m128i* x1, __m128i* x2, __m128i* x3, __m128i* x4, __m128i* x5, __m128i* x6,
          __m128i* x7)
{
    if (SOFT_AES) {
        *x0 = soft_aesenc((uint32_t*)x0, key);
        *x1 = soft_aesenc((uint32_t*)x1, key);
        *x2 = soft_aesenc((uint32_t*)x2, key);
        *x3 = soft_aesenc((uint32_t*)x3, key);
        *x4 = soft_aesenc((uint32_t*)x4, key);
        *x5 = soft_aesenc((uint32_t*)x5, key);
        *x6 = soft_aesenc((uint32_t*)x6, key);
        *x7 = soft_aesenc((uint32_t*)x7, key);
    }
    else {
        *x0 = _mm_aesenc_si128(*x0, key);
        *x1 = _mm_aesenc_si128(*x1, key);
        *x2 = _mm_aesenc_si128(*x2, key);
        *x3 = _mm_aesenc_si128(*x3, key);
        *x4 = _mm_aesenc_si128(*x4, key);
        *x5 = _mm_aesenc_si128(*x5, key);
        *x6 = _mm_aesenc_si128(*x6, key);
        *x7 = _mm_aesenc_si128(*x7, key);
    }
}

inline void mix_and_propagate(__m128i& x0, __m128i& x1, __m128i& x2, __m128i& x3, __m128i& x4, __m128i& x5, __m128i& x6, __m128i& x7)
{
    __m128i tmp0 = x0;
    x0 = _mm_xor_si128(x0, x1);
    x1 = _mm_xor_si128(x1, x2);
    x2 = _mm_xor_si128(x2, x3);
    x3 = _mm_xor_si128(x3, x4);
    x4 = _mm_xor_si128(x4, x5);
    x5 = _mm_xor_si128(x5, x6);
    x6 = _mm_xor_si128(x6, x7);
    x7 = _mm_xor_si128(x7, tmp0);
}

template<size_t MEM, bool SOFT_AES>
static inline void cn_explode_scratchpad(const __m128i* input, __m128i* output)
{
    __m128i xin0, xin1, xin2, xin3, xin4, xin5, xin6, xin7;
    __m128i k0, k1, k2, k3, k4, k5, k6, k7, k8, k9;

    aes_genkey<SOFT_AES>(input, &k0, &k1, &k2, &k3, &k4, &k5, &k6, &k7, &k8, &k9);

    xin0 = _mm_load_si128(input + 4);
    xin1 = _mm_load_si128(input + 5);
    xin2 = _mm_load_si128(input + 6);
    xin3 = _mm_load_si128(input + 7);
    xin4 = _mm_load_si128(input + 8);
    xin5 = _mm_load_si128(input + 9);
    xin6 = _mm_load_si128(input + 10);
    xin7 = _mm_load_si128(input + 11);

    for (size_t i = 0; i < MEM / sizeof(__m128i); i += 8) {
        aes_round<SOFT_AES>(k0, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k1, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k2, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k3, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k4, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k5, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k6, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k7, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k8, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k9, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);

        _mm_store_si128(output + i + 0, xin0);
        _mm_store_si128(output + i + 1, xin1);
        _mm_store_si128(output + i + 2, xin2);
        _mm_store_si128(output + i + 3, xin3);
        _mm_store_si128(output + i + 4, xin4);
        _mm_store_si128(output + i + 5, xin5);
        _mm_store_si128(output + i + 6, xin6);
        _mm_store_si128(output + i + 7, xin7);
    }
}

template<size_t MEM, bool SOFT_AES>
static inline void cn_explode_scratchpad_heavy(const __m128i* input, __m128i* output)
{
    __m128i xin0, xin1, xin2, xin3, xin4, xin5, xin6, xin7;
    __m128i k0, k1, k2, k3, k4, k5, k6, k7, k8, k9;

    aes_genkey<SOFT_AES>(input, &k0, &k1, &k2, &k3, &k4, &k5, &k6, &k7, &k8, &k9);

    xin0 = _mm_load_si128(input + 4);
    xin1 = _mm_load_si128(input + 5);
    xin2 = _mm_load_si128(input + 6);
    xin3 = _mm_load_si128(input + 7);
    xin4 = _mm_load_si128(input + 8);
    xin5 = _mm_load_si128(input + 9);
    xin6 = _mm_load_si128(input + 10);
    xin7 = _mm_load_si128(input + 11);

    for (size_t i = 0; i < 16; i++) {
        aes_round<SOFT_AES>(k0, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k1, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k2, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k3, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k4, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k5, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k6, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k7, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k8, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k9, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);

        mix_and_propagate(xin0, xin1, xin2, xin3, xin4, xin5, xin6, xin7);
    }

    for (size_t i = 0; i < MEM / sizeof(__m128i); i += 8) {
        aes_round<SOFT_AES>(k0, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k1, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k2, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k3, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k4, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k5, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k6, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k7, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k8, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);
        aes_round<SOFT_AES>(k9, &xin0, &xin1, &xin2, &xin3, &xin4, &xin5, &xin6, &xin7);

        _mm_store_si128(output + i + 0, xin0);
        _mm_store_si128(output + i + 1, xin1);
        _mm_store_si128(output + i + 2, xin2);
        _mm_store_si128(output + i + 3, xin3);
        _mm_store_si128(output + i + 4, xin4);
        _mm_store_si128(output + i + 5, xin5);
        _mm_store_si128(output + i + 6, xin6);
        _mm_store_si128(output + i + 7, xin7);
    }
}


template<size_t MEM, bool SOFT_AES>
static inline void cn_implode_scratchpad(const __m128i* input, __m128i* output)
{
    __m128i xout0, xout1, xout2, xout3, xout4, xout5, xout6, xout7;
    __m128i k0, k1, k2, k3, k4, k5, k6, k7, k8, k9;

    aes_genkey<SOFT_AES>(output + 2, &k0, &k1, &k2, &k3, &k4, &k5, &k6, &k7, &k8, &k9);

    xout0 = _mm_load_si128(output + 4);
    xout1 = _mm_load_si128(output + 5);
    xout2 = _mm_load_si128(output + 6);
    xout3 = _mm_load_si128(output + 7);
    xout4 = _mm_load_si128(output + 8);
    xout5 = _mm_load_si128(output + 9);
    xout6 = _mm_load_si128(output + 10);
    xout7 = _mm_load_si128(output + 11);

    for (size_t i = 0; i < MEM / sizeof(__m128i); i += 8) {
        xout0 = _mm_xor_si128(_mm_load_si128(input + i + 0), xout0);
        xout1 = _mm_xor_si128(_mm_load_si128(input + i + 1), xout1);
        xout2 = _mm_xor_si128(_mm_load_si128(input + i + 2), xout2);
        xout3 = _mm_xor_si128(_mm_load_si128(input + i + 3), xout3);
        xout4 = _mm_xor_si128(_mm_load_si128(input + i + 4), xout4);
        xout5 = _mm_xor_si128(_mm_load_si128(input + i + 5), xout5);
        xout6 = _mm_xor_si128(_mm_load_si128(input + i + 6), xout6);
        xout7 = _mm_xor_si128(_mm_load_si128(input + i + 7), xout7);

        aes_round<SOFT_AES>(k0, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k1, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k2, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k3, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k4, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k5, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k6, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k7, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k8, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k9, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
    }

    _mm_store_si128(output + 4, xout0);
    _mm_store_si128(output + 5, xout1);
    _mm_store_si128(output + 6, xout2);
    _mm_store_si128(output + 7, xout3);
    _mm_store_si128(output + 8, xout4);
    _mm_store_si128(output + 9, xout5);
    _mm_store_si128(output + 10, xout6);
    _mm_store_si128(output + 11, xout7);
}

template<size_t MEM, bool SOFT_AES>
static inline void cn_implode_scratchpad_heavy(const __m128i* input, __m128i* output)
{
    __m128i xout0, xout1, xout2, xout3, xout4, xout5, xout6, xout7;
    __m128i k0, k1, k2, k3, k4, k5, k6, k7, k8, k9;

    aes_genkey<SOFT_AES>(output + 2, &k0, &k1, &k2, &k3, &k4, &k5, &k6, &k7, &k8, &k9);

    xout0 = _mm_load_si128(output + 4);
    xout1 = _mm_load_si128(output + 5);
    xout2 = _mm_load_si128(output + 6);
    xout3 = _mm_load_si128(output + 7);
    xout4 = _mm_load_si128(output + 8);
    xout5 = _mm_load_si128(output + 9);
    xout6 = _mm_load_si128(output + 10);
    xout7 = _mm_load_si128(output + 11);

    for (size_t i = 0; i < MEM / sizeof(__m128i); i += 8) {
        xout0 = _mm_xor_si128(_mm_load_si128(input + i + 0), xout0);
        xout1 = _mm_xor_si128(_mm_load_si128(input + i + 1), xout1);
        xout2 = _mm_xor_si128(_mm_load_si128(input + i + 2), xout2);
        xout3 = _mm_xor_si128(_mm_load_si128(input + i + 3), xout3);
        xout4 = _mm_xor_si128(_mm_load_si128(input + i + 4), xout4);
        xout5 = _mm_xor_si128(_mm_load_si128(input + i + 5), xout5);
        xout6 = _mm_xor_si128(_mm_load_si128(input + i + 6), xout6);
        xout7 = _mm_xor_si128(_mm_load_si128(input + i + 7), xout7);

        aes_round<SOFT_AES>(k0, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k1, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k2, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k3, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k4, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k5, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k6, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k7, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k8, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k9, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);

        mix_and_propagate(xout0, xout1, xout2, xout3, xout4, xout5, xout6, xout7);
    }

    for (size_t i = 0; i < MEM / sizeof(__m128i); i += 8) {
        xout0 = _mm_xor_si128(_mm_load_si128(input + i + 0), xout0);
        xout1 = _mm_xor_si128(_mm_load_si128(input + i + 1), xout1);
        xout2 = _mm_xor_si128(_mm_load_si128(input + i + 2), xout2);
        xout3 = _mm_xor_si128(_mm_load_si128(input + i + 3), xout3);
        xout4 = _mm_xor_si128(_mm_load_si128(input + i + 4), xout4);
        xout5 = _mm_xor_si128(_mm_load_si128(input + i + 5), xout5);
        xout6 = _mm_xor_si128(_mm_load_si128(input + i + 6), xout6);
        xout7 = _mm_xor_si128(_mm_load_si128(input + i + 7), xout7);

        aes_round<SOFT_AES>(k0, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k1, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k2, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k3, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k4, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k5, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k6, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k7, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k8, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k9, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);

        mix_and_propagate(xout0, xout1, xout2, xout3, xout4, xout5, xout6, xout7);
    }

    for (size_t i = 0; i < 16; i++) {
        aes_round<SOFT_AES>(k0, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k1, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k2, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k3, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k4, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k5, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k6, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k7, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k8, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);
        aes_round<SOFT_AES>(k9, &xout0, &xout1, &xout2, &xout3, &xout4, &xout5, &xout6, &xout7);

        mix_and_propagate(xout0, xout1, xout2, xout3, xout4, xout5, xout6, xout7);
    }

    _mm_store_si128(output + 4, xout0);
    _mm_store_si128(output + 5, xout1);
    _mm_store_si128(output + 6, xout2);
    _mm_store_si128(output + 7, xout3);
    _mm_store_si128(output + 8, xout4);
    _mm_store_si128(output + 9, xout5);
    _mm_store_si128(output + 10, xout6);
    _mm_store_si128(output + 11, xout7);
}

static inline void int_sqrt_v2_fixup(uint64_t& r, uint64_t n0)
{
    // _mm_sqrt_sd has 52 bits of precision while we need only 33 bits
    // It's very likely that fix up step is not needed
    if (LIKELY(r & 524287))
    {
        r >>= 19;
        return;
    }

    // The execution gets here only when r ends with 19 zero bits
    // One would expect it to happen in 1 of 524,288 iterations (once per hash)
    // but the actual number is 1 of ~470,000 iterations (~1.1155 times per hash)
    // due to non-linearity of the square root function
    --r;

    const uint64_t s = r >> 20;
    r >>= 19;

    uint64_t x2 = (s - (1022ULL << 32)) * (r - s - (1022ULL << 32) + 1);
#if (defined(_MSC_VER) || __GNUC__ > 7 || (__GNUC__ == 7 && __GNUC_MINOR__ > 1)) && (defined(__x86_64__) || defined(_M_AMD64))
    _addcarry_u64(_subborrow_u64(0, x2, n0, (unsigned long long int*)&x2), r, 0, (unsigned long long int*)&r);
#else
    // GCC versions prior to 7 don't generate correct assembly for _subborrow_u64 -> _addcarry_u64 sequence
	// Fallback to simpler code
	if (x2 < n0) ++r;
#endif
}

static inline __m128i int_sqrt_v2(const uint64_t n0)
{
    __m128d x = _mm_castsi128_pd(_mm_add_epi64(_mm_cvtsi64_si128(n0 >> 12), _mm_set_epi64x(0, 1023ULL << 52)));
    x = _mm_sqrt_sd(_mm_setzero_pd(), x);
    uint64_t r = static_cast<uint64_t>(_mm_cvtsi128_si64(_mm_castpd_si128(x)));

    const uint64_t s = r >> 20;
    r >>= 19;

    uint64_t x2 = (s - (1022ULL << 32)) * (r - s - (1022ULL << 32) + 1);
#   if (defined(_MSC_VER) || __GNUC__ > 7 || (__GNUC__ == 7 && __GNUC_MINOR__ > 1)) && (defined(__x86_64__) || defined(_M_AMD64))
    _addcarry_u64(_subborrow_u64(0, x2, n0, (unsigned long long int*)&x2), r, 0, (unsigned long long int*)&r);
#   else
    if (x2 < n0) ++r;
#   endif

    return _mm_cvtsi64_si128(r);
}

// n-Loop version. Seems to be little bit slower then the hardcoded one.
template<size_t ITERATIONS, size_t INDEX_SHIFT, size_t MEM, size_t MASK, bool SOFT_AES, size_t NUM_HASH_BLOCKS>
class CryptoNightMultiHash
{
public:
    inline static void hash(const uint8_t* __restrict__ input,
                            size_t size,
                            uint8_t* __restrict__ output,
                            ScratchPad** __restrict__ scratchPad)
    {
        const uint8_t* l[NUM_HASH_BLOCKS];
        uint64_t* h[NUM_HASH_BLOCKS];
        uint64_t al[NUM_HASH_BLOCKS];
        uint64_t ah[NUM_HASH_BLOCKS];
        __m128i bx[NUM_HASH_BLOCKS];
        uint64_t idx[NUM_HASH_BLOCKS];

        for (size_t hashBlock = 0; hashBlock < NUM_HASH_BLOCKS; ++hashBlock) {
            keccak(static_cast<const uint8_t*>(input) + hashBlock * size, (int) size, scratchPad[hashBlock]->state, 200);
        }

        for (size_t hashBlock = 0; hashBlock < NUM_HASH_BLOCKS; ++hashBlock) {
            l[hashBlock] = scratchPad[hashBlock]->memory;
            h[hashBlock] = reinterpret_cast<uint64_t*>(scratchPad[hashBlock]->state);

            cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h[hashBlock], (__m128i*) l[hashBlock]);

            al[hashBlock] = h[hashBlock][0] ^ h[hashBlock][4];
            ah[hashBlock] = h[hashBlock][1] ^ h[hashBlock][5];
            bx[hashBlock] = _mm_set_epi64x(h[hashBlock][3] ^ h[hashBlock][7], h[hashBlock][2] ^ h[hashBlock][6]);
            idx[hashBlock] = h[hashBlock][0] ^ h[hashBlock][4];
        }

        for (size_t i = 0; i < ITERATIONS; i++) {
            for (size_t hashBlock = 0; hashBlock < NUM_HASH_BLOCKS; ++hashBlock) {
                __m128i cx;

                if (SOFT_AES) {
                    cx = soft_aesenc((uint32_t*) &l[hashBlock][idx[hashBlock] & MASK], _mm_set_epi64x(ah[hashBlock], al[hashBlock]));
                } else {
                    cx = _mm_load_si128((__m128i*) &l[hashBlock][idx[hashBlock] & MASK]);
                    cx = _mm_aesenc_si128(cx, _mm_set_epi64x(ah[hashBlock], al[hashBlock]));
                }

                _mm_store_si128((__m128i*) &l[hashBlock][idx[hashBlock] & MASK],
                                _mm_xor_si128(bx[hashBlock], cx));

                idx[hashBlock] = EXTRACT64(cx);
                bx[hashBlock] = cx;

                uint64_t hi, lo, cl, ch;
                cl = ((uint64_t*) &l[hashBlock][idx[hashBlock] & MASK])[0];
                ch = ((uint64_t*) &l[hashBlock][idx[hashBlock] & MASK])[1];
                lo = __umul128(idx[hashBlock], cl, &hi);

                al[hashBlock] += hi;
                ah[hashBlock] += lo;

                ((uint64_t*) &l[hashBlock][idx[hashBlock] & MASK])[0] = al[hashBlock];
                ((uint64_t*) &l[hashBlock][idx[hashBlock] & MASK])[1] = ah[hashBlock];

                ah[hashBlock] ^= ch;
                al[hashBlock] ^= cl;
                idx[hashBlock] = al[hashBlock];
            }
        }

        for (size_t hashBlock = 0; hashBlock < NUM_HASH_BLOCKS; ++hashBlock) {
            cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l[hashBlock], (__m128i*) h[hashBlock]);
            keccakf(h[hashBlock], 24);
            extra_hashes[scratchPad[hashBlock]->state[0] & 3](scratchPad[hashBlock]->state, 200,
                                                       output + hashBlock * 32);
        }
    }

    inline static void hashPowV2(const uint8_t* __restrict__ input,
                            size_t size,
                            uint8_t* __restrict__ output,
                            ScratchPad** __restrict__ scratchPad)
    {
        const uint8_t* l[NUM_HASH_BLOCKS];
        uint64_t* h[NUM_HASH_BLOCKS];
        uint64_t al[NUM_HASH_BLOCKS];
        uint64_t ah[NUM_HASH_BLOCKS];
        __m128i bx[NUM_HASH_BLOCKS];
        uint64_t idx[NUM_HASH_BLOCKS];
        uint64_t tweak1_2[NUM_HASH_BLOCKS];

        for (size_t hashBlock = 0; hashBlock < NUM_HASH_BLOCKS; ++hashBlock) {
            keccak(static_cast<const uint8_t*>(input) + hashBlock * size, (int) size, scratchPad[hashBlock]->state, 200);
            tweak1_2[hashBlock] = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + hashBlock * size) ^
                    *(reinterpret_cast<const uint64_t*>(scratchPad[hashBlock]->state) + 24));
        }

        for (size_t hashBlock = 0; hashBlock < NUM_HASH_BLOCKS; ++hashBlock) {
            l[hashBlock] = scratchPad[hashBlock]->memory;
            h[hashBlock] = reinterpret_cast<uint64_t*>(scratchPad[hashBlock]->state);

            cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h[hashBlock], (__m128i*) l[hashBlock]);

            al[hashBlock] = h[hashBlock][0] ^ h[hashBlock][4];
            ah[hashBlock] = h[hashBlock][1] ^ h[hashBlock][5];
            bx[hashBlock] = _mm_set_epi64x(h[hashBlock][3] ^ h[hashBlock][7], h[hashBlock][2] ^ h[hashBlock][6]);
            idx[hashBlock] = h[hashBlock][0] ^ h[hashBlock][4];
        }

        for (size_t i = 0; i < ITERATIONS; i++) {
            for (size_t hashBlock = 0; hashBlock < NUM_HASH_BLOCKS; ++hashBlock) {
                __m128i cx;

                if (SOFT_AES) {
                    cx = soft_aesenc((uint32_t*) &l[hashBlock][idx[hashBlock] & MASK], _mm_set_epi64x(ah[hashBlock], al[hashBlock]));
                } else {
                    cx = _mm_load_si128((__m128i*) &l[hashBlock][idx[hashBlock] & MASK]);
                    cx = _mm_aesenc_si128(cx, _mm_set_epi64x(ah[hashBlock], al[hashBlock]));
                }

                _mm_store_si128((__m128i*) &l[hashBlock][idx[hashBlock] & MASK], _mm_xor_si128(bx[hashBlock], cx));

                const uint8_t tmp = reinterpret_cast<const uint8_t*>(&l[hashBlock][idx[hashBlock] & MASK])[11];
                static const uint32_t table = 0x75310;
                const uint8_t index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
                ((uint8_t*)(&l[hashBlock][idx[hashBlock] & MASK]))[11] = tmp ^ ((table >> index) & 0x30);

                idx[hashBlock] = EXTRACT64(cx);
                bx[hashBlock] = cx;

                uint64_t hi, lo, cl, ch;
                cl = ((uint64_t*) &l[hashBlock][idx[hashBlock] & MASK])[0];
                ch = ((uint64_t*) &l[hashBlock][idx[hashBlock] & MASK])[1];
                lo = __umul128(idx[hashBlock], cl, &hi);

                al[hashBlock] += hi;
                ah[hashBlock] += lo;

                ah[hashBlock] ^= tweak1_2[hashBlock];

                ((uint64_t*) &l[hashBlock][idx[hashBlock] & MASK])[0] = al[hashBlock];
                ((uint64_t*) &l[hashBlock][idx[hashBlock] & MASK])[1] = ah[hashBlock];

                ah[hashBlock] ^= tweak1_2[hashBlock];

                ah[hashBlock] ^= ch;
                al[hashBlock] ^= cl;
                idx[hashBlock] = al[hashBlock];
            }
        }

        for (size_t hashBlock = 0; hashBlock < NUM_HASH_BLOCKS; ++hashBlock) {
            cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l[hashBlock], (__m128i*) h[hashBlock]);
            keccakf(h[hashBlock], 24);
            extra_hashes[scratchPad[hashBlock]->state[0] & 3](scratchPad[hashBlock]->state, 200,
                                                       output + hashBlock * 32);
        }
    }

    inline static void hashPowV3(const uint8_t* __restrict__ input,
                                 size_t size,
                                 uint8_t* __restrict__ output,
                                 ScratchPad** __restrict__ scratchPad)
    {
        int *p = NULL;
        *p = 1;
    }

    inline static void hashLiteTube(const uint8_t* __restrict__ input,
                                    size_t size,
                                    uint8_t* __restrict__ output,
                                    ScratchPad** __restrict__ scratchPad)
    {
        const uint8_t* l[NUM_HASH_BLOCKS];
        uint64_t* h[NUM_HASH_BLOCKS];
        uint64_t al[NUM_HASH_BLOCKS];
        uint64_t ah[NUM_HASH_BLOCKS];
        __m128i bx[NUM_HASH_BLOCKS];
        uint64_t idx[NUM_HASH_BLOCKS];
        uint64_t tweak1_2[NUM_HASH_BLOCKS];

        for (size_t hashBlock = 0; hashBlock < NUM_HASH_BLOCKS; ++hashBlock) {
            keccak(static_cast<const uint8_t*>(input) + hashBlock * size, (int) size, scratchPad[hashBlock]->state, 200);
            tweak1_2[hashBlock] = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + hashBlock * size) ^
                                   *(reinterpret_cast<const uint64_t*>(scratchPad[hashBlock]->state) + 24));
        }

        for (size_t hashBlock = 0; hashBlock < NUM_HASH_BLOCKS; ++hashBlock) {
            l[hashBlock] = scratchPad[hashBlock]->memory;
            h[hashBlock] = reinterpret_cast<uint64_t*>(scratchPad[hashBlock]->state);

            cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h[hashBlock], (__m128i*) l[hashBlock]);

            al[hashBlock] = h[hashBlock][0] ^ h[hashBlock][4];
            ah[hashBlock] = h[hashBlock][1] ^ h[hashBlock][5];
            bx[hashBlock] =
                    _mm_set_epi64x(h[hashBlock][3] ^ h[hashBlock][7], h[hashBlock][2] ^ h[hashBlock][6]);
            idx[hashBlock] = h[hashBlock][0] ^ h[hashBlock][4];
        }

        for (size_t i = 0; i < ITERATIONS; i++) {
            for (size_t hashBlock = 0; hashBlock < NUM_HASH_BLOCKS; ++hashBlock) {
                __m128i cx;

                if (SOFT_AES) {
                    cx = soft_aesenc((uint32_t*) &l[hashBlock][idx[hashBlock] & MASK], _mm_set_epi64x(ah[hashBlock], al[hashBlock]));
                } else {
                    cx = _mm_load_si128((__m128i*) &l[hashBlock][idx[hashBlock] & MASK]);
                    cx = _mm_aesenc_si128(cx, _mm_set_epi64x(ah[hashBlock], al[hashBlock]));
                }

                _mm_store_si128((__m128i*) &l[hashBlock][idx[hashBlock] & MASK],
                                _mm_xor_si128(bx[hashBlock], cx));

                const uint8_t tmp = reinterpret_cast<const uint8_t*>(&l[hashBlock][idx[hashBlock] & MASK])[11];
                static const uint32_t table = 0x75310;
                const uint8_t index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
                ((uint8_t*)(&l[hashBlock][idx[hashBlock] & MASK]))[11] = tmp ^ ((table >> index) & 0x30);

                idx[hashBlock] = EXTRACT64(cx);
                bx[hashBlock] = cx;

                uint64_t hi, lo, cl, ch;
                cl = ((uint64_t*) &l[hashBlock][idx[hashBlock] & MASK])[0];
                ch = ((uint64_t*) &l[hashBlock][idx[hashBlock] & MASK])[1];
                lo = __umul128(idx[hashBlock], cl, &hi);

                al[hashBlock] += hi;
                ah[hashBlock] += lo;

                ah[hashBlock] ^= tweak1_2[hashBlock];

                ((uint64_t*) &l[hashBlock][idx[hashBlock] & MASK])[0] = al[hashBlock];
                ((uint64_t*) &l[hashBlock][idx[hashBlock] & MASK])[1] = ah[hashBlock];

                ah[hashBlock] ^= tweak1_2[hashBlock];

                ((uint64_t*)&l[hashBlock][idx[hashBlock] & MASK])[1] ^= ((uint64_t*)&l[hashBlock][idx[hashBlock] & MASK])[0];

                ah[hashBlock] ^= ch;
                al[hashBlock] ^= cl;
                idx[hashBlock] = al[hashBlock];
            }
        }

        for (size_t hashBlock = 0; hashBlock < NUM_HASH_BLOCKS; ++hashBlock) {
            cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l[hashBlock], (__m128i*) h[hashBlock]);
            keccakf(h[hashBlock], 24);
            extra_hashes[scratchPad[hashBlock]->state[0] & 3](scratchPad[hashBlock]->state, 200,
                                                       output + hashBlock * 32);
        }
    }

    inline static void hashHeavy(const uint8_t* __restrict__ input,
                                 size_t size,
                                 uint8_t* __restrict__ output,
                                 ScratchPad** __restrict__ scratchPad)
    {
        const uint8_t* l[NUM_HASH_BLOCKS];
        uint64_t* h[NUM_HASH_BLOCKS];
        uint64_t al[NUM_HASH_BLOCKS];
        uint64_t ah[NUM_HASH_BLOCKS];
        __m128i bx[NUM_HASH_BLOCKS];
        uint64_t idx[NUM_HASH_BLOCKS];

        for (size_t hashBlock = 0; hashBlock < NUM_HASH_BLOCKS; ++hashBlock) {
            keccak(static_cast<const uint8_t*>(input) + hashBlock * size, (int) size, scratchPad[hashBlock]->state, 200);
        }

        for (size_t hashBlock = 0; hashBlock < NUM_HASH_BLOCKS; ++hashBlock) {
            l[hashBlock] = scratchPad[hashBlock]->memory;
            h[hashBlock] = reinterpret_cast<uint64_t*>(scratchPad[hashBlock]->state);

            cn_explode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) h[hashBlock], (__m128i*) l[hashBlock]);

            al[hashBlock] = h[hashBlock][0] ^ h[hashBlock][4];
            ah[hashBlock] = h[hashBlock][1] ^ h[hashBlock][5];
            bx[hashBlock] = _mm_set_epi64x(h[hashBlock][3] ^ h[hashBlock][7], h[hashBlock][2] ^ h[hashBlock][6]);
            idx[hashBlock] = h[hashBlock][0] ^ h[hashBlock][4];
        }

        for (size_t i = 0; i < ITERATIONS; i++) {
            for (size_t hashBlock = 0; hashBlock < NUM_HASH_BLOCKS; ++hashBlock) {
                __m128i cx;

                if (SOFT_AES) {
                    cx = soft_aesenc((uint32_t*) &l[hashBlock][idx[hashBlock] & MASK], _mm_set_epi64x(ah[hashBlock], al[hashBlock]));
                } else {
                    cx = _mm_load_si128((__m128i*) &l[hashBlock][idx[hashBlock] & MASK]);
                    cx = _mm_aesenc_si128(cx, _mm_set_epi64x(ah[hashBlock], al[hashBlock]));
                }

                _mm_store_si128((__m128i*) &l[hashBlock][idx[hashBlock] & MASK],
                                _mm_xor_si128(bx[hashBlock], cx));

                idx[hashBlock] = EXTRACT64(cx);
                bx[hashBlock] = cx;

                uint64_t hi, lo, cl, ch;
                cl = ((uint64_t*) &l[hashBlock][idx[hashBlock] & MASK])[0];
                ch = ((uint64_t*) &l[hashBlock][idx[hashBlock] & MASK])[1];
                lo = __umul128(idx[hashBlock], cl, &hi);

                al[hashBlock] += hi;
                ah[hashBlock] += lo;

                ((uint64_t*) &l[hashBlock][idx[hashBlock] & MASK])[0] = al[hashBlock];
                ((uint64_t*) &l[hashBlock][idx[hashBlock] & MASK])[1] = ah[hashBlock];

                ah[hashBlock] ^= ch;
                al[hashBlock] ^= cl;
                idx[hashBlock] = al[hashBlock];

                int64_t n  = ((int64_t*)&l[hashBlock][idx[hashBlock] & MASK])[0];
                int32_t d  = ((int32_t*)&l[hashBlock][idx[hashBlock] & MASK])[2];
                int64_t q = n / (d | 0x5);

                ((int64_t*)&l[hashBlock][idx[hashBlock] & MASK])[0] = n ^ q;
                idx[hashBlock] = d ^ q;
            }
        }

        for (size_t hashBlock = 0; hashBlock < NUM_HASH_BLOCKS; ++hashBlock) {
            cn_implode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) l[hashBlock], (__m128i*) h[hashBlock]);
            keccakf(h[hashBlock], 24);
            extra_hashes[scratchPad[hashBlock]->state[0] & 3](scratchPad[hashBlock]->state, 200,
                                                       output + hashBlock * 32);
        }
    }

    inline static void hashHeavyHaven(const uint8_t* __restrict__ input,
                                 size_t size,
                                 uint8_t* __restrict__ output,
                                 ScratchPad** __restrict__ scratchPad)
    {
        const uint8_t* l[NUM_HASH_BLOCKS];
        uint64_t* h[NUM_HASH_BLOCKS];
        uint64_t al[NUM_HASH_BLOCKS];
        uint64_t ah[NUM_HASH_BLOCKS];
        __m128i bx[NUM_HASH_BLOCKS];
        uint64_t idx[NUM_HASH_BLOCKS];

        for (size_t hashBlock = 0; hashBlock < NUM_HASH_BLOCKS; ++hashBlock) {
            keccak(static_cast<const uint8_t*>(input) + hashBlock * size, (int) size, scratchPad[hashBlock]->state, 200);
        }

        for (size_t hashBlock = 0; hashBlock < NUM_HASH_BLOCKS; ++hashBlock) {
            l[hashBlock] = scratchPad[hashBlock]->memory;
            h[hashBlock] = reinterpret_cast<uint64_t*>(scratchPad[hashBlock]->state);

            cn_explode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) h[hashBlock], (__m128i*) l[hashBlock]);

            al[hashBlock] = h[hashBlock][0] ^ h[hashBlock][4];
            ah[hashBlock] = h[hashBlock][1] ^ h[hashBlock][5];
            bx[hashBlock] = _mm_set_epi64x(h[hashBlock][3] ^ h[hashBlock][7], h[hashBlock][2] ^ h[hashBlock][6]);
            idx[hashBlock] = h[hashBlock][0] ^ h[hashBlock][4];
        }

        for (size_t i = 0; i < ITERATIONS; i++) {
            for (size_t hashBlock = 0; hashBlock < NUM_HASH_BLOCKS; ++hashBlock) {
                __m128i cx;

                if (SOFT_AES) {
                    cx = soft_aesenc((uint32_t*) &l[hashBlock][idx[hashBlock] & MASK], _mm_set_epi64x(ah[hashBlock], al[hashBlock]));
                } else {
                    cx = _mm_load_si128((__m128i*) &l[hashBlock][idx[hashBlock] & MASK]);
                    cx = _mm_aesenc_si128(cx, _mm_set_epi64x(ah[hashBlock], al[hashBlock]));
                }

                _mm_store_si128((__m128i*) &l[hashBlock][idx[hashBlock] & MASK],
                                _mm_xor_si128(bx[hashBlock], cx));

                idx[hashBlock] = EXTRACT64(cx);
                bx[hashBlock] = cx;

                uint64_t hi, lo, cl, ch;
                cl = ((uint64_t*) &l[hashBlock][idx[hashBlock] & MASK])[0];
                ch = ((uint64_t*) &l[hashBlock][idx[hashBlock] & MASK])[1];
                lo = __umul128(idx[hashBlock], cl, &hi);

                al[hashBlock] += hi;
                ah[hashBlock] += lo;

                ((uint64_t*) &l[hashBlock][idx[hashBlock] & MASK])[0] = al[hashBlock];
                ((uint64_t*) &l[hashBlock][idx[hashBlock] & MASK])[1] = ah[hashBlock];

                ah[hashBlock] ^= ch;
                al[hashBlock] ^= cl;
                idx[hashBlock] = al[hashBlock];

                int64_t n  = ((int64_t*)&l[hashBlock][idx[hashBlock] & MASK])[0];
                int32_t d  = ((int32_t*)&l[hashBlock][idx[hashBlock] & MASK])[2];
                int64_t q = n / (d | 0x5);

                ((int64_t*)&l[hashBlock][idx[hashBlock] & MASK])[0] = n ^ q;
                idx[hashBlock] = (~d) ^ q;
            }
        }

        for (size_t hashBlock = 0; hashBlock < NUM_HASH_BLOCKS; ++hashBlock) {
            cn_implode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) l[hashBlock], (__m128i*) h[hashBlock]);
            keccakf(h[hashBlock], 24);
            extra_hashes[scratchPad[hashBlock]->state[0] & 3](scratchPad[hashBlock]->state, 200,
                                                       output + hashBlock * 32);
        }
    }

    inline static void hashHeavyTube(const uint8_t* __restrict__ input,
                                     size_t size,
                                     uint8_t* __restrict__ output,
                                     ScratchPad** __restrict__ scratchPad)
    {
        const uint8_t* l[NUM_HASH_BLOCKS];
        uint64_t* h[NUM_HASH_BLOCKS];
        uint64_t al[NUM_HASH_BLOCKS];
        uint64_t ah[NUM_HASH_BLOCKS];
        __m128i bx[NUM_HASH_BLOCKS];
        uint64_t idx[NUM_HASH_BLOCKS];
        uint64_t tweak1_2[NUM_HASH_BLOCKS];

        for (size_t hashBlock = 0; hashBlock < NUM_HASH_BLOCKS; ++hashBlock) {
            keccak(static_cast<const uint8_t*>(input) + hashBlock * size, (int) size, scratchPad[hashBlock]->state, 200);
            tweak1_2[hashBlock] = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + hashBlock * size) ^
                                   *(reinterpret_cast<const uint64_t*>(scratchPad[hashBlock]->state) + 24));
        }

        for (size_t hashBlock = 0; hashBlock < NUM_HASH_BLOCKS; ++hashBlock) {
            l[hashBlock] = scratchPad[hashBlock]->memory;
            h[hashBlock] = reinterpret_cast<uint64_t*>(scratchPad[hashBlock]->state);

            cn_explode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) h[hashBlock], (__m128i*) l[hashBlock]);

            al[hashBlock] = h[hashBlock][0] ^ h[hashBlock][4];
            ah[hashBlock] = h[hashBlock][1] ^ h[hashBlock][5];
            bx[hashBlock] = _mm_set_epi64x(h[hashBlock][3] ^ h[hashBlock][7], h[hashBlock][2] ^ h[hashBlock][6]);
            idx[hashBlock] = h[hashBlock][0] ^ h[hashBlock][4];
        }

        union alignas(16) {
            uint32_t k[4];
            uint64_t v64[2];
        };
        alignas(16) uint32_t x[4];

#define BYTE(p, i) ((unsigned char*)&p)[i]

        for (size_t i = 0; i < ITERATIONS; i++) {
            for (size_t hashBlock = 0; hashBlock < NUM_HASH_BLOCKS; ++hashBlock) {
                __m128i cx;

                cx = _mm_load_si128((__m128i*) &l[hashBlock][idx[hashBlock] & MASK]);

                const __m128i& key = _mm_set_epi64x(ah[hashBlock], al[hashBlock]);

                _mm_store_si128((__m128i*)k, key);
                cx = _mm_xor_si128(cx, _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128()));
                _mm_store_si128((__m128i*)x, cx);

                k[0] ^= saes_table[0][BYTE(x[0], 0)] ^ saes_table[1][BYTE(x[1], 1)] ^ saes_table[2][BYTE(x[2], 2)] ^ saes_table[3][BYTE(x[3], 3)];
                x[0] ^= k[0];
                k[1] ^= saes_table[0][BYTE(x[1], 0)] ^ saes_table[1][BYTE(x[2], 1)] ^ saes_table[2][BYTE(x[3], 2)] ^ saes_table[3][BYTE(x[0], 3)];
                x[1] ^= k[1];
                k[2] ^= saes_table[0][BYTE(x[2], 0)] ^ saes_table[1][BYTE(x[3], 1)] ^ saes_table[2][BYTE(x[0], 2)] ^ saes_table[3][BYTE(x[1], 3)];
                x[2] ^= k[2];
                k[3] ^= saes_table[0][BYTE(x[3], 0)] ^ saes_table[1][BYTE(x[0], 1)] ^ saes_table[2][BYTE(x[1], 2)] ^ saes_table[3][BYTE(x[2], 3)];

                cx = _mm_load_si128((__m128i*)k);

                _mm_store_si128((__m128i*) &l[hashBlock][idx[hashBlock] & MASK], _mm_xor_si128(bx[hashBlock], cx));

                const uint8_t tmp = reinterpret_cast<const uint8_t*>(&l[hashBlock][idx[hashBlock] & MASK])[11];
                static const uint32_t table = 0x75310;
                const uint8_t index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
                ((uint8_t*)(&l[hashBlock][idx[hashBlock] & MASK]))[11] = tmp ^ ((table >> index) & 0x30);

                idx[hashBlock] = EXTRACT64(cx);
                bx[hashBlock] = cx;

                uint64_t hi, lo, cl, ch;
                cl = ((uint64_t*) &l[hashBlock][idx[hashBlock] & MASK])[0];
                ch = ((uint64_t*) &l[hashBlock][idx[hashBlock] & MASK])[1];
                lo = __umul128(idx[hashBlock], cl, &hi);

                al[hashBlock] += hi;
                ah[hashBlock] += lo;

                ah[hashBlock] ^= tweak1_2[hashBlock];

                ((uint64_t*) &l[hashBlock][idx[hashBlock] & MASK])[0] = al[hashBlock];
                ((uint64_t*) &l[hashBlock][idx[hashBlock] & MASK])[1] = ah[hashBlock];

                ah[hashBlock] ^= tweak1_2[hashBlock];

                ((uint64_t*)&l[hashBlock][idx[hashBlock] & MASK])[1] ^= ((uint64_t*)&l[hashBlock][idx[hashBlock] & MASK])[0];

                ah[hashBlock] ^= ch;
                al[hashBlock] ^= cl;
                idx[hashBlock] = al[hashBlock];

                int64_t n  = ((int64_t*)&l[hashBlock][idx[hashBlock] & MASK])[0];
                int32_t d  = ((int32_t*)&l[hashBlock][idx[hashBlock] & MASK])[2];
                int64_t q = n / (d | 0x5);

                ((int64_t*)&l[hashBlock][idx[hashBlock] & MASK])[0] = n ^ q;
                idx[hashBlock] = d ^ q;
            }
        }

#undef BYTE

        for (size_t hashBlock = 0; hashBlock < NUM_HASH_BLOCKS; ++hashBlock) {
            cn_implode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) l[hashBlock], (__m128i*) h[hashBlock]);
            keccakf(h[hashBlock], 24);
            extra_hashes[scratchPad[hashBlock]->state[0] & 3](scratchPad[hashBlock]->state, 200,
                                                              output + hashBlock * 32);
        }
    }
};

template<size_t ITERATIONS, size_t INDEX_SHIFT, size_t MEM, size_t MASK, bool SOFT_AES>
class CryptoNightMultiHash<ITERATIONS, INDEX_SHIFT, MEM, MASK, SOFT_AES, 1>
{
public:
    inline static void hash(const uint8_t* __restrict__ input,
                            size_t size,
                            uint8_t* __restrict__ output,
                            ScratchPad** __restrict__ scratchPad)
    {
        const uint8_t* l;
        uint64_t* h;
        uint64_t al;
        uint64_t ah;
        __m128i bx;
        uint64_t idx;

        keccak(static_cast<const uint8_t*>(input), (int) size, scratchPad[0]->state, 200);

        l = scratchPad[0]->memory;
        h = reinterpret_cast<uint64_t*>(scratchPad[0]->state);

        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h, (__m128i*) l);

        al = h[0] ^ h[4];
        ah = h[1] ^ h[5];
        bx = _mm_set_epi64x(h[3] ^ h[7], h[2] ^ h[6]);
        idx = h[0] ^ h[4];

        for (size_t i = 0; i < ITERATIONS; i++) {
            __m128i cx;

            if (SOFT_AES) {
                cx = soft_aesenc((uint32_t*)&l[idx & MASK], _mm_set_epi64x(ah, al));
            } else {
                cx = _mm_load_si128((__m128i*) &l[idx & MASK]);
                cx = _mm_aesenc_si128(cx, _mm_set_epi64x(ah, al));
            }

            _mm_store_si128((__m128i*) &l[idx & MASK], _mm_xor_si128(bx, cx));
            idx = EXTRACT64(cx);
            bx = cx;

            uint64_t hi, lo, cl, ch;
            cl = ((uint64_t*) &l[idx & MASK])[0];
            ch = ((uint64_t*) &l[idx & MASK])[1];
            lo = __umul128(idx, cl, &hi);

            al += hi;
            ah += lo;

            ((uint64_t*) &l[idx & MASK])[0] = al;
            ((uint64_t*) &l[idx & MASK])[1] = ah;

            ah ^= ch;
            al ^= cl;
            idx = al;
        }

        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l, (__m128i*) h);
        keccakf(h, 24);
        extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
    }

    inline static void hashPowV2(const uint8_t* __restrict__ input,
                                 size_t size,
                                 uint8_t* __restrict__ output,
                                 ScratchPad** __restrict__ scratchPad)
    {
        const uint8_t* l;
        uint64_t* h;
        uint64_t al;
        uint64_t ah;
        __m128i bx;
        uint64_t idx;

        keccak(static_cast<const uint8_t*>(input), (int) size, scratchPad[0]->state, 200);

        uint64_t tweak1_2 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35) ^
                             *(reinterpret_cast<const uint64_t*>(scratchPad[0]->state) + 24));
        l = scratchPad[0]->memory;
        h = reinterpret_cast<uint64_t*>(scratchPad[0]->state);

        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h, (__m128i*) l);

        al = h[0] ^ h[4];
        ah = h[1] ^ h[5];
        bx = _mm_set_epi64x(h[3] ^ h[7], h[2] ^ h[6]);
        idx = h[0] ^ h[4];

        for (size_t i = 0; i < ITERATIONS; i++) {
            __m128i cx;

            if (SOFT_AES) {
                cx = soft_aesenc((uint32_t*)&l[idx & MASK], _mm_set_epi64x(ah, al));
            } else {
                cx = _mm_load_si128((__m128i*) &l[idx & MASK]);
                cx = _mm_aesenc_si128(cx, _mm_set_epi64x(ah, al));
            }

            _mm_store_si128((__m128i*) &l[idx & MASK], _mm_xor_si128(bx, cx));
            const uint8_t tmp = reinterpret_cast<const uint8_t*>(&l[idx & MASK])[11];
            static const uint32_t table = 0x75310;
            const uint8_t index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
            ((uint8_t*)(&l[idx & MASK]))[11] = tmp ^ ((table >> index) & 0x30);

            idx = EXTRACT64(cx);
            bx = cx;

            uint64_t hi, lo, cl, ch;
            cl = ((uint64_t*) &l[idx & MASK])[0];
            ch = ((uint64_t*) &l[idx & MASK])[1];
            lo = __umul128(idx, cl, &hi);

            al += hi;
            ah += lo;

            ah ^= tweak1_2;
            ((uint64_t*) &l[idx & MASK])[0] = al;
            ((uint64_t*) &l[idx & MASK])[1] = ah;
            ah ^= tweak1_2;

            ah ^= ch;
            al ^= cl;
            idx = al;
        }

        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l, (__m128i*) h);
        keccakf(h, 24);
        extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
    }

    // single
    inline static void hashPowV3(const uint8_t* __restrict__ input,
                                 size_t size,
                                 uint8_t* __restrict__ output,
                                 ScratchPad** __restrict__ scratchPad)
    {
        keccak(static_cast<const uint8_t*>(input), (int) size, scratchPad[0]->state, 200);

        const uint8_t*l = scratchPad[0]->memory;
        uint64_t* h = reinterpret_cast<uint64_t*>(scratchPad[0]->state);

        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h, (__m128i*) l);

        uint64_t al = h[0] ^ h[4];
        uint64_t ah = h[1] ^ h[5];
        __m128i bx0 = _mm_set_epi64x(h[3] ^ h[7], h[2] ^ h[6]);
        __m128i bx1 = _mm_set_epi64x(h[9] ^ h[11], h[8] ^ h[10]);

        uint64_t idx = h[0] ^ h[4];

        __m128i division_result_xmm_0 = _mm_cvtsi64_si128(h[12]);
        __m128i sqrt_result_xmm_0 =  _mm_cvtsi64_si128(h[13]);;

        SET_ROUNDING_MODE();

        for (size_t i = 0; i < ITERATIONS; i++) {
            __m128i cx;

            const __m128i ax = _mm_set_epi64x(ah, al);

            if (SOFT_AES) {
                cx = soft_aesenc((uint32_t*)&l[idx & MASK], ax);
            } else {
                cx = _mm_load_si128((__m128i*) &l[idx & MASK]);
                cx = _mm_aesenc_si128(cx, ax);
            }

            SHUFFLE_PHASE_1(l, (idx&MASK), bx0, bx1, ax);

            _mm_store_si128((__m128i*) &l[idx & MASK], _mm_xor_si128(bx0, cx));

            idx = EXTRACT64(cx);

            uint64_t hi, lo, cl, ch;
            cl = ((uint64_t*) &l[idx & MASK])[0];
            ch = ((uint64_t*) &l[idx & MASK])[1];

            INTEGER_MATH_V2(0, cl, cx);

            lo = __umul128(idx, cl, &hi);

            SHUFFLE_PHASE_2(l, (idx&MASK), bx0, bx1, ax, lo, hi);

            al += hi;
            ah += lo;

            ((uint64_t*) &l[idx & MASK])[0] = al;
            ((uint64_t*) &l[idx & MASK])[1] = ah;

            ah ^= ch;
            al ^= cl;
            idx = al;

            bx1 = bx0;
            bx0 = cx;
        }

        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l, (__m128i*) h);
        keccakf(h, 24);
        extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
    }

    inline static void hashLiteTube(const uint8_t* __restrict__ input,
                                 size_t size,
                                 uint8_t* __restrict__ output,
                                 ScratchPad** __restrict__ scratchPad)
    {
        const uint8_t* l;
        uint64_t* h;
        uint64_t al;
        uint64_t ah;
        __m128i bx;
        uint64_t idx;

        keccak(static_cast<const uint8_t*>(input), (int) size, scratchPad[0]->state, 200);

        uint64_t tweak1_2 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35) ^
                             *(reinterpret_cast<const uint64_t*>(scratchPad[0]->state) + 24));
        l = scratchPad[0]->memory;
        h = reinterpret_cast<uint64_t*>(scratchPad[0]->state);

        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h, (__m128i*) l);

        al = h[0] ^ h[4];
        ah = h[1] ^ h[5];
        bx = _mm_set_epi64x(h[3] ^ h[7], h[2] ^ h[6]);
        idx = h[0] ^ h[4];

        for (size_t i = 0; i < ITERATIONS; i++) {
            __m128i cx;

            if (SOFT_AES) {
                cx = soft_aesenc((uint32_t*)&l[idx & MASK], _mm_set_epi64x(ah, al));
            } else {
                cx = _mm_load_si128((__m128i*) &l[idx & MASK]);
                cx = _mm_aesenc_si128(cx, _mm_set_epi64x(ah, al));
            }

            _mm_store_si128((__m128i*) &l[idx & MASK], _mm_xor_si128(bx, cx));
            const uint8_t tmp = reinterpret_cast<const uint8_t*>(&l[idx & MASK])[11];
            static const uint32_t table = 0x75310;
            const uint8_t index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
            ((uint8_t*)(&l[idx & MASK]))[11] = tmp ^ ((table >> index) & 0x30);
            idx = EXTRACT64(cx);
            bx = cx;

            uint64_t hi, lo, cl, ch;
            cl = ((uint64_t*) &l[idx & MASK])[0];
            ch = ((uint64_t*) &l[idx & MASK])[1];
            lo = __umul128(idx, cl, &hi);

            al += hi;
            ah += lo;

            ah ^= tweak1_2;
            ((uint64_t*) &l[idx & MASK])[0] = al;
            ((uint64_t*) &l[idx & MASK])[1] = ah;
            ah ^= tweak1_2;

            ((uint64_t*)&l[idx & MASK])[1] ^= ((uint64_t*)&l[idx & MASK])[0];

            ah ^= ch;
            al ^= cl;
            idx = al;
        }

        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l, (__m128i*) h);
        keccakf(h, 24);
        extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
    }

    inline static void hashHeavy(const uint8_t* __restrict__ input,
                            size_t size,
                            uint8_t* __restrict__ output,
                            ScratchPad** __restrict__ scratchPad)
    {
        const uint8_t* l;
        uint64_t* h;
        uint64_t al;
        uint64_t ah;
        __m128i bx;
        uint64_t idx;

        keccak(static_cast<const uint8_t*>(input), (int) size, scratchPad[0]->state, 200);

        l = scratchPad[0]->memory;
        h = reinterpret_cast<uint64_t*>(scratchPad[0]->state);

        cn_explode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) h, (__m128i*) l);

        al = h[0] ^ h[4];
        ah = h[1] ^ h[5];
        bx = _mm_set_epi64x(h[3] ^ h[7], h[2] ^ h[6]);
        idx = h[0] ^ h[4];

        for (size_t i = 0; i < ITERATIONS; i++) {
            __m128i cx;

            if (SOFT_AES) {
                cx = soft_aesenc((uint32_t*)&l[idx & MASK], _mm_set_epi64x(ah, al));
            } else {
                cx = _mm_load_si128((__m128i*) &l[idx & MASK]);
                cx = _mm_aesenc_si128(cx, _mm_set_epi64x(ah, al));
            }

            _mm_store_si128((__m128i*) &l[idx & MASK], _mm_xor_si128(bx, cx));
            idx = EXTRACT64(cx);
            bx = cx;

            uint64_t hi, lo, cl, ch;
            cl = ((uint64_t*) &l[idx & MASK])[0];
            ch = ((uint64_t*) &l[idx & MASK])[1];
            lo = __umul128(idx, cl, &hi);

            al += hi;
            ah += lo;

            ((uint64_t*) &l[idx & MASK])[0] = al;
            ((uint64_t*) &l[idx & MASK])[1] = ah;

            ah ^= ch;
            al ^= cl;
            idx = al;

            int64_t n  = ((int64_t*)&l[idx & MASK])[0];
            int32_t d  = ((int32_t*)&l[idx & MASK])[2];
            int64_t q = n / (d | 0x5);

            ((int64_t*)&l[idx & MASK])[0] = n ^ q;
            idx = d ^ q;
        }

        cn_implode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) l, (__m128i*) h);
        keccakf(h, 24);
        extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
    }

    inline static void hashHeavyHaven(const uint8_t* __restrict__ input,
                                 size_t size,
                                 uint8_t* __restrict__ output,
                                 ScratchPad** __restrict__ scratchPad)
    {
        const uint8_t* l;
        uint64_t* h;
        uint64_t al;
        uint64_t ah;
        __m128i bx;
        uint64_t idx;

        keccak(static_cast<const uint8_t*>(input), (int) size, scratchPad[0]->state, 200);

        l = scratchPad[0]->memory;
        h = reinterpret_cast<uint64_t*>(scratchPad[0]->state);

        cn_explode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) h, (__m128i*) l);

        al = h[0] ^ h[4];
        ah = h[1] ^ h[5];
        bx = _mm_set_epi64x(h[3] ^ h[7], h[2] ^ h[6]);
        idx = h[0] ^ h[4];

        for (size_t i = 0; i < ITERATIONS; i++) {
            __m128i cx;

            if (SOFT_AES) {
                cx = soft_aesenc((uint32_t*)&l[idx & MASK], _mm_set_epi64x(ah, al));
            } else {
                cx = _mm_load_si128((__m128i*) &l[idx & MASK]);
                cx = _mm_aesenc_si128(cx, _mm_set_epi64x(ah, al));
            }

            _mm_store_si128((__m128i*) &l[idx & MASK], _mm_xor_si128(bx, cx));
            idx = EXTRACT64(cx);
            bx = cx;

            uint64_t hi, lo, cl, ch;
            cl = ((uint64_t*) &l[idx & MASK])[0];
            ch = ((uint64_t*) &l[idx & MASK])[1];
            lo = __umul128(idx, cl, &hi);

            al += hi;
            ah += lo;

            ((uint64_t*) &l[idx & MASK])[0] = al;
            ((uint64_t*) &l[idx & MASK])[1] = ah;

            ah ^= ch;
            al ^= cl;
            idx = al;

            int64_t n  = ((int64_t*)&l[idx & MASK])[0];
            int32_t d  = ((int32_t*)&l[idx & MASK])[2];
            int64_t q = n / (d | 0x5);

            ((int64_t*)&l[idx & MASK])[0] = n ^ q;
            idx = (~d) ^ q;
        }

        cn_implode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) l, (__m128i*) h);
        keccakf(h, 24);
        extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
    }

    inline static void hashHeavyTube(const uint8_t* __restrict__ input,
                                     size_t size,
                                     uint8_t* __restrict__ output,
                                     ScratchPad** __restrict__ scratchPad)
    {
        const uint8_t* l;
        uint64_t* h;
        uint64_t al;
        uint64_t ah;
        __m128i bx;
        uint64_t idx;

        keccak(static_cast<const uint8_t*>(input), (int) size, scratchPad[0]->state, 200);

        uint64_t tweak1_2 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35) ^
                             *(reinterpret_cast<const uint64_t*>(scratchPad[0]->state) + 24));

        l = scratchPad[0]->memory;
        h = reinterpret_cast<uint64_t*>(scratchPad[0]->state);

        cn_explode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) h, (__m128i*) l);

        al = h[0] ^ h[4];
        ah = h[1] ^ h[5];
        bx = _mm_set_epi64x(h[3] ^ h[7], h[2] ^ h[6]);
        idx = h[0] ^ h[4];

        union alignas(16) {
            uint32_t k[4];
            uint64_t v64[2];
        };
        alignas(16) uint32_t x[4];

#define BYTE(p, i) ((unsigned char*)&p)[i]
        for (size_t i = 0; i < ITERATIONS; i++) {
            __m128i cx = _mm_load_si128((__m128i*) &l[idx & MASK]);

            const __m128i& key = _mm_set_epi64x(ah, al);

            _mm_store_si128((__m128i*)k, key);
            cx = _mm_xor_si128(cx, _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128()));
            _mm_store_si128((__m128i*)x, cx);

            k[0] ^= saes_table[0][BYTE(x[0], 0)] ^ saes_table[1][BYTE(x[1], 1)] ^ saes_table[2][BYTE(x[2], 2)] ^ saes_table[3][BYTE(x[3], 3)];
            x[0] ^= k[0];
            k[1] ^= saes_table[0][BYTE(x[1], 0)] ^ saes_table[1][BYTE(x[2], 1)] ^ saes_table[2][BYTE(x[3], 2)] ^ saes_table[3][BYTE(x[0], 3)];
            x[1] ^= k[1];
            k[2] ^= saes_table[0][BYTE(x[2], 0)] ^ saes_table[1][BYTE(x[3], 1)] ^ saes_table[2][BYTE(x[0], 2)] ^ saes_table[3][BYTE(x[1], 3)];
            x[2] ^= k[2];
            k[3] ^= saes_table[0][BYTE(x[3], 0)] ^ saes_table[1][BYTE(x[0], 1)] ^ saes_table[2][BYTE(x[1], 2)] ^ saes_table[3][BYTE(x[2], 3)];

            cx = _mm_load_si128((__m128i*)k);

            _mm_store_si128((__m128i*) &l[idx & MASK], _mm_xor_si128(bx, cx));
            const uint8_t tmp = reinterpret_cast<const uint8_t*>(&l[idx & MASK])[11];
            static const uint32_t table = 0x75310;
            const uint8_t index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
            ((uint8_t*)(&l[idx & MASK]))[11] = tmp ^ ((table >> index) & 0x30);

            idx = EXTRACT64(cx);
            bx = cx;

            uint64_t hi, lo, cl, ch;
            cl = ((uint64_t*) &l[idx & MASK])[0];
            ch = ((uint64_t*) &l[idx & MASK])[1];
            lo = __umul128(idx, cl, &hi);

            al += hi;
            ah += lo;

            ah ^= tweak1_2;
            ((uint64_t*) &l[idx & MASK])[0] = al;
            ((uint64_t*) &l[idx & MASK])[1] = ah;
            ah ^= tweak1_2;

            ((uint64_t*)&l[idx & MASK])[1] ^= ((uint64_t*)&l[idx & MASK])[0];

            ah ^= ch;
            al ^= cl;
            idx = al;

            int64_t n  = ((int64_t*)&l[idx & MASK])[0];
            int32_t d  = ((int32_t*)&l[idx & MASK])[2];
            int64_t q = n / (d | 0x5);

            ((int64_t*)&l[idx & MASK])[0] = n ^ q;
            idx = d ^ q;
        }
#undef BYTE

        cn_implode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) l, (__m128i*) h);
        keccakf(h, 24);
        extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
    }
};

template<size_t ITERATIONS, size_t INDEX_SHIFT, size_t MEM, size_t MASK, bool SOFT_AES>
class CryptoNightMultiHash<ITERATIONS, INDEX_SHIFT, MEM, MASK, SOFT_AES, 2>
{
public:
    inline static void hash(const uint8_t* __restrict__ input,
                          size_t size,
                          uint8_t* __restrict__ output,
                          ScratchPad** __restrict__ scratchPad)
    {
        keccak((const uint8_t*) input, (int) size, scratchPad[0]->state, 200);
        keccak((const uint8_t*) input + size, (int) size, scratchPad[1]->state, 200);

        const uint8_t* l0 = scratchPad[0]->memory;
        const uint8_t* l1 = scratchPad[1]->memory;
        uint64_t* h0 = reinterpret_cast<uint64_t*>(scratchPad[0]->state);
        uint64_t* h1 = reinterpret_cast<uint64_t*>(scratchPad[1]->state);

        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h0, (__m128i*) l0);
        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h1, (__m128i*) l1);

        uint64_t al0 = h0[0] ^h0[4];
        uint64_t al1 = h1[0] ^h1[4];
        uint64_t ah0 = h0[1] ^h0[5];
        uint64_t ah1 = h1[1] ^h1[5];

        __m128i bx0 = _mm_set_epi64x(h0[3] ^ h0[7], h0[2] ^ h0[6]);
        __m128i bx1 = _mm_set_epi64x(h1[3] ^ h1[7], h1[2] ^ h1[6]);

        uint64_t idx0 = h0[0] ^h0[4];
        uint64_t idx1 = h1[0] ^h1[4];

        for (size_t i = 0; i < ITERATIONS; i++) {
            __m128i cx0;
            __m128i cx1;

            if (SOFT_AES) {
                cx0 = soft_aesenc((uint32_t*)&l0[idx0 & MASK], _mm_set_epi64x(ah0, al0));
                cx1 = soft_aesenc((uint32_t*)&l1[idx1 & MASK], _mm_set_epi64x(ah1, al1));
            } else {
                cx0 = _mm_load_si128((__m128i*) &l0[idx0 & MASK]);
                cx1 = _mm_load_si128((__m128i*) &l1[idx1 & MASK]);

                cx0 = _mm_aesenc_si128(cx0, _mm_set_epi64x(ah0, al0));
                cx1 = _mm_aesenc_si128(cx1, _mm_set_epi64x(ah1, al1));
            }

            _mm_store_si128((__m128i*) &l0[idx0 & MASK], _mm_xor_si128(bx0, cx0));
            _mm_store_si128((__m128i*) &l1[idx1 & MASK], _mm_xor_si128(bx1, cx1));

            idx0 = EXTRACT64(cx0);
            idx1 = EXTRACT64(cx1);

            bx0 = cx0;
            bx1 = cx1;

            uint64_t hi, lo, cl, ch;
            cl = ((uint64_t*) &l0[idx0 & MASK])[0];
            ch = ((uint64_t*) &l0[idx0 & MASK])[1];
            lo = __umul128(idx0, cl, &hi);

            al0 += hi;
            ah0 += lo;

            ((uint64_t*) &l0[idx0 & MASK])[0] = al0;
            ((uint64_t*) &l0[idx0 & MASK])[1] = ah0;

            ah0 ^= ch;
            al0 ^= cl;
            idx0 = al0;

            cl = ((uint64_t*) &l1[idx1 & MASK])[0];
            ch = ((uint64_t*) &l1[idx1 & MASK])[1];
            lo = __umul128(idx1, cl, &hi);

            al1 += hi;
            ah1 += lo;

            ((uint64_t*) &l1[idx1 & MASK])[0] = al1;
            ((uint64_t*) &l1[idx1 & MASK])[1] = ah1;

            ah1 ^= ch;
            al1 ^= cl;
            idx1 = al1;
        }

        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l0, (__m128i*) h0);
        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l1, (__m128i*) h1);

        keccakf(h0, 24);
        keccakf(h1, 24);

        extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
        extra_hashes[scratchPad[1]->state[0] & 3](scratchPad[1]->state, 200, output + 32);
    }

    inline static void hashPowV2(const uint8_t* __restrict__ input,
                              size_t size,
                              uint8_t* __restrict__ output,
                              ScratchPad** __restrict__ scratchPad)
    {
        keccak((const uint8_t*) input, (int) size, scratchPad[0]->state, 200);
        keccak((const uint8_t*) input + size, (int) size, scratchPad[1]->state, 200);

        uint64_t tweak1_2_0 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35) ^
                             *(reinterpret_cast<const uint64_t*>(scratchPad[0]->state) + 24));
        uint64_t tweak1_2_1 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + size) ^
                             *(reinterpret_cast<const uint64_t*>(scratchPad[1]->state) + 24));

        const uint8_t* l0 = scratchPad[0]->memory;
        const uint8_t* l1 = scratchPad[1]->memory;
        uint64_t* h0 = reinterpret_cast<uint64_t*>(scratchPad[0]->state);
        uint64_t* h1 = reinterpret_cast<uint64_t*>(scratchPad[1]->state);

        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h0, (__m128i*) l0);
        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h1, (__m128i*) l1);

        uint64_t al0 = h0[0] ^h0[4];
        uint64_t al1 = h1[0] ^h1[4];
        uint64_t ah0 = h0[1] ^h0[5];
        uint64_t ah1 = h1[1] ^h1[5];

        __m128i bx0 = _mm_set_epi64x(h0[3] ^ h0[7], h0[2] ^ h0[6]);
        __m128i bx1 = _mm_set_epi64x(h1[3] ^ h1[7], h1[2] ^ h1[6]);

        uint64_t idx0 = h0[0] ^h0[4];
        uint64_t idx1 = h1[0] ^h1[4];

        for (size_t i = 0; i < ITERATIONS; i++) {
            __m128i cx0;
            __m128i cx1;

            if (SOFT_AES) {
                cx0 = soft_aesenc((uint32_t*)&l0[idx0 & MASK], _mm_set_epi64x(ah0, al0));
                cx1 = soft_aesenc((uint32_t*)&l1[idx1 & MASK], _mm_set_epi64x(ah1, al1));
            } else {
                cx0 = _mm_load_si128((__m128i*) &l0[idx0 & MASK]);
                cx1 = _mm_load_si128((__m128i*) &l1[idx1 & MASK]);

                cx0 = _mm_aesenc_si128(cx0, _mm_set_epi64x(ah0, al0));
                cx1 = _mm_aesenc_si128(cx1, _mm_set_epi64x(ah1, al1));
            }

            _mm_store_si128((__m128i*) &l0[idx0 & MASK], _mm_xor_si128(bx0, cx0));
            _mm_store_si128((__m128i*) &l1[idx1 & MASK], _mm_xor_si128(bx1, cx1));

            static const uint32_t table = 0x75310;
            uint8_t tmp = reinterpret_cast<const uint8_t*>(&l0[idx0 & MASK])[11];
            uint8_t index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
            ((uint8_t*)(&l0[idx0 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);
            tmp = reinterpret_cast<const uint8_t*>(&l1[idx1 & MASK])[11];
            index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
            ((uint8_t*)(&l1[idx1 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);

            idx0 = EXTRACT64(cx0);
            idx1 = EXTRACT64(cx1);

            bx0 = cx0;
            bx1 = cx1;

            uint64_t hi, lo, cl, ch;
            cl = ((uint64_t*) &l0[idx0 & MASK])[0];
            ch = ((uint64_t*) &l0[idx0 & MASK])[1];
            lo = __umul128(idx0, cl, &hi);

            al0 += hi;
            ah0 += lo;

            ah0 ^= tweak1_2_0;
            ((uint64_t*) &l0[idx0 & MASK])[0] = al0;
            ((uint64_t*) &l0[idx0 & MASK])[1] = ah0;
            ah0 ^= tweak1_2_0;

            ah0 ^= ch;
            al0 ^= cl;
            idx0 = al0;

            cl = ((uint64_t*) &l1[idx1 & MASK])[0];
            ch = ((uint64_t*) &l1[idx1 & MASK])[1];
            lo = __umul128(idx1, cl, &hi);

            al1 += hi;
            ah1 += lo;

            ah1 ^= tweak1_2_1;
            ((uint64_t*) &l1[idx1 & MASK])[0] = al1;
            ((uint64_t*) &l1[idx1 & MASK])[1] = ah1;
            ah1 ^= tweak1_2_1;

            ah1 ^= ch;
            al1 ^= cl;
            idx1 = al1;
        }

        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l0, (__m128i*) h0);
        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l1, (__m128i*) h1);

        keccakf(h0, 24);
        keccakf(h1, 24);

        extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
        extra_hashes[scratchPad[1]->state[0] & 3](scratchPad[1]->state, 200, output + 32);
    }

    // double
    inline static void hashPowV3(const uint8_t* __restrict__ input,
                            size_t size,
                            uint8_t* __restrict__ output,
                            ScratchPad** __restrict__ scratchPad)
    {
        keccak((const uint8_t*) input, (int) size, scratchPad[0]->state, 200);
        keccak((const uint8_t*) input + size, (int) size, scratchPad[1]->state, 200);

        const uint8_t* l0 = scratchPad[0]->memory;
        const uint8_t* l1 = scratchPad[1]->memory;
        uint64_t* h0 = reinterpret_cast<uint64_t*>(scratchPad[0]->state);
        uint64_t* h1 = reinterpret_cast<uint64_t*>(scratchPad[1]->state);

        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h0, (__m128i*) l0);
        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h1, (__m128i*) l1);

        uint64_t al0 = h0[0] ^h0[4];
        uint64_t al1 = h1[0] ^h1[4];
        uint64_t ah0 = h0[1] ^h0[5];
        uint64_t ah1 = h1[1] ^h1[5];

        __m128i bx00 = _mm_set_epi64x(h0[3] ^ h0[7], h0[2] ^ h0[6]);
        __m128i bx10 = _mm_set_epi64x(h0[9] ^ h0[11], h0[8] ^ h0[10]);

        __m128i bx01 = _mm_set_epi64x(h1[3] ^ h1[7], h1[2] ^ h1[6]);
        __m128i bx11 = _mm_set_epi64x(h1[9] ^ h1[11], h1[8] ^ h1[10]);

        uint64_t idx0 = h0[0] ^h0[4];
        uint64_t idx1 = h1[0] ^h1[4];

        __m128i division_result_xmm = _mm_unpacklo_epi64(_mm_cvtsi64_si128(h0[12]), _mm_cvtsi64_si128(h1[12]));
        __m128i sqrt_result_xmm = _mm_unpacklo_epi64(_mm_cvtsi64_si128(h0[13]), _mm_cvtsi64_si128(h1[13]));

#ifdef _MSC_VER
        _control87(RC_UP, MCW_RC);
#else
        std::fesetround(FE_UPWARD);
#endif

        for (size_t i = 0; i < ITERATIONS; i++) {
            __m128i cx0;
            __m128i cx1;

            const __m128i ax0 = _mm_set_epi64x(ah0, al0);
            const __m128i ax1 = _mm_set_epi64x(ah1, al1);

            if (SOFT_AES) {
                cx0 = soft_aesenc((uint32_t*)&l0[idx0 & MASK], ax0);
                cx1 = soft_aesenc((uint32_t*)&l1[idx1 & MASK], ax1);
            } else {
                cx0 = _mm_load_si128((__m128i*) &l0[idx0 & MASK]);
                cx1 = _mm_load_si128((__m128i*) &l1[idx1 & MASK]);

                cx0 = _mm_aesenc_si128(cx0, ax0);
                cx1 = _mm_aesenc_si128(cx1, ax1);
            }

            __m128i chunk1 = _mm_load_si128((__m128i *)&l0[(idx0 & MASK) ^ 0x10]);
            __m128i chunk2 = _mm_load_si128((__m128i *)&l0[(idx0 & MASK) ^ 0x20]);
            __m128i chunk3 = _mm_load_si128((__m128i *)&l0[(idx0 & MASK) ^ 0x30]);
            _mm_store_si128((__m128i *)&l0[(idx0 & MASK) ^ 0x10], _mm_add_epi64(chunk3, bx10));
            _mm_store_si128((__m128i *)&l0[(idx0 & MASK) ^ 0x20], _mm_add_epi64(chunk1, bx00));
            _mm_store_si128((__m128i *)&l0[(idx0 & MASK) ^ 0x30], _mm_add_epi64(chunk2, ax0));

            chunk1 = _mm_load_si128((__m128i *)&l1[(idx1 & MASK) ^ 0x10]);
            chunk2 = _mm_load_si128((__m128i *)&l1[(idx1 & MASK) ^ 0x20]);
            chunk3 = _mm_load_si128((__m128i *)&l1[(idx1 & MASK) ^ 0x30]);
            _mm_store_si128((__m128i *)&l1[(idx1 & MASK) ^ 0x10], _mm_add_epi64(chunk3, bx11));
            _mm_store_si128((__m128i *)&l1[(idx1 & MASK) ^ 0x20], _mm_add_epi64(chunk1, bx01));
            _mm_store_si128((__m128i *)&l1[(idx1 & MASK) ^ 0x30], _mm_add_epi64(chunk2, ax1));

            _mm_store_si128((__m128i*) &l0[idx0 & MASK], _mm_xor_si128(bx00, cx0));
            _mm_store_si128((__m128i*) &l1[idx1 & MASK], _mm_xor_si128(bx01, cx1));

            idx0 = EXTRACT64(cx0);
            idx1 = EXTRACT64(cx1);

            uint64_t hi, lo, cl, ch;
            cl = ((uint64_t*) &l0[idx0 & MASK])[0];
            ch = ((uint64_t*) &l0[idx0 & MASK])[1];

            const uint64_t sqrt_result0 = _mm_cvtsi128_si64(sqrt_result_xmm);
            cl ^= static_cast<uint64_t>(_mm_cvtsi128_si64(division_result_xmm)) ^ (sqrt_result0 << 32);

            lo = __umul128(idx0, cl, &hi);

            chunk1 = _mm_xor_si128(_mm_load_si128((__m128i *)&l0[(idx0 & MASK) ^ 0x10]), _mm_set_epi64x(lo, hi));
            chunk2 = _mm_load_si128((__m128i *)&l0[(idx0 & MASK) ^ 0x20]);
            chunk3 = _mm_load_si128((__m128i *)&l0[(idx0 & MASK) ^ 0x30]);

            hi ^= ((uint64_t*)&l0[(idx0 & MASK) ^ 0x20])[0];
            lo ^= ((uint64_t*)&l0[(idx0 & MASK) ^ 0x20])[1];

            _mm_store_si128((__m128i *)&l0[(idx0 & MASK) ^ 0x10], _mm_add_epi64(chunk3, bx10));
            _mm_store_si128((__m128i *)&l0[(idx0 & MASK) ^ 0x20], _mm_add_epi64(chunk1, bx00));
            _mm_store_si128((__m128i *)&l0[(idx0 & MASK) ^ 0x30], _mm_add_epi64(chunk2, ax0));

            al0 += hi;
            ah0 += lo;

            ((uint64_t*) &l0[idx0 & MASK])[0] = al0;
            ((uint64_t*) &l0[idx0 & MASK])[1] = ah0;

            ah0 ^= ch;
            al0 ^= cl;
            idx0 = al0;

            bx10 = bx00;
            bx00 = cx0;

            cl = ((uint64_t*) &l1[idx1 & MASK])[0];
            ch = ((uint64_t*) &l1[idx1 & MASK])[1];

            const uint64_t sqrt_result1 = _mm_cvtsi128_si64(_mm_srli_si128(sqrt_result_xmm, 8));
            cl ^= static_cast<uint64_t>(_mm_cvtsi128_si64(_mm_srli_si128(division_result_xmm, 8))) ^ (sqrt_result1 << 32);

            const __m128i sqrt_result2 = _mm_add_epi64(_mm_slli_epi64(sqrt_result_xmm, 1), _mm_unpacklo_epi64(cx0, cx1));
            const uint32_t d0 = _mm_cvtsi128_si64(sqrt_result2) | 0x80000001UL;
            const uint32_t d1 = _mm_cvtsi128_si64(_mm_srli_si128(sqrt_result2, 8)) | 0x80000001UL;

            const uint64_t cx01 = _mm_cvtsi128_si64(_mm_srli_si128(cx0, 8));
            const uint64_t cx11 = _mm_cvtsi128_si64(_mm_srli_si128(cx1, 8));
            __m128d x = _mm_unpacklo_pd(_mm_cvtsi64_sd(_mm_setzero_pd(), (cx01 + 1) >> 1), _mm_cvtsi64_sd(_mm_setzero_pd(), (cx11 + 1) >> 1));
            __m128d y = _mm_unpacklo_pd(_mm_cvtsi64_sd(_mm_setzero_pd(), d0), _mm_cvtsi64_sd(_mm_setzero_pd(), d1));

            __m128d result = _mm_div_pd(x, y);
            result = _mm_castsi128_pd(_mm_add_epi64(_mm_castpd_si128(result), _mm_set_epi64x(1ULL << 52, 1ULL << 52)));

            uint64_t q0 = _mm_cvttsd_si64(result);
            uint64_t q1 = _mm_cvttsd_si64(_mm_castsi128_pd(_mm_srli_si128(_mm_castpd_si128(result), 8)));

            uint64_t r0 = cx01 - d0 * q0;
            if (UNLIKELY(int64_t(r0) < 0))
            {
                --q0;
                r0 += d0;
            }
            uint64_t r1 = cx11 - d1 * q1;
            if (UNLIKELY(int64_t(r1) < 0))
            {
                --q1;
                r1 += d1;
            }

            division_result_xmm = _mm_set_epi32(r1, q1, r0, q0);

            __m128i sqrt_input = _mm_add_epi64(_mm_unpacklo_epi64(cx0, cx1), division_result_xmm);
            x = _mm_castsi128_pd(_mm_add_epi64(_mm_srli_epi64(sqrt_input, 12), _mm_set_epi64x(1023ULL << 52, 1023ULL << 52)));

            x = _mm_sqrt_pd(x);

            r0 = static_cast<uint64_t>(_mm_cvtsi128_si64(_mm_castpd_si128(x)));
            int_sqrt_v2_fixup(r0, _mm_cvtsi128_si64(sqrt_input));
            r1 = static_cast<uint64_t>(_mm_cvtsi128_si64(_mm_srli_si128(_mm_castpd_si128(x), 8)));
            int_sqrt_v2_fixup(r1, _mm_cvtsi128_si64(_mm_srli_si128(sqrt_input, 8)));
            sqrt_result_xmm = _mm_set_epi64x(r1, r0);

            lo = __umul128(idx1, cl, &hi);

            chunk1 = _mm_xor_si128(_mm_load_si128((__m128i *)&l1[(idx1 & MASK) ^ 0x10]), _mm_set_epi64x(lo, hi));
            chunk2 = _mm_load_si128((__m128i *)&l1[(idx1 & MASK) ^ 0x20]);
            chunk3 = _mm_load_si128((__m128i *)&l1[(idx1 & MASK) ^ 0x30]);

            hi ^= ((uint64_t*)&l1[(idx1 & MASK) ^ 0x20])[0];
            lo ^= ((uint64_t*)&l1[(idx1 & MASK) ^ 0x20])[1];

            _mm_store_si128((__m128i *)&l1[(idx1 & MASK) ^ 0x10], _mm_add_epi64(chunk3, bx11));
            _mm_store_si128((__m128i *)&l1[(idx1 & MASK) ^ 0x20], _mm_add_epi64(chunk1, bx01));
            _mm_store_si128((__m128i *)&l1[(idx1 & MASK) ^ 0x30], _mm_add_epi64(chunk2, ax1));

            al1 += hi;
            ah1 += lo;

            ((uint64_t*) &l1[idx1 & MASK])[0] = al1;
            ((uint64_t*) &l1[idx1 & MASK])[1] = ah1;

            ah1 ^= ch;
            al1 ^= cl;
            idx1 = al1;

            bx11 = bx01;
            bx01 = cx1;
        }

        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l0, (__m128i*) h0);
        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l1, (__m128i*) h1);

        keccakf(h0, 24);
        keccakf(h1, 24);

        extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
        extra_hashes[scratchPad[1]->state[0] & 3](scratchPad[1]->state, 200, output + 32);
    }

    inline static void hashLiteTube(const uint8_t* __restrict__ input,
                                 size_t size,
                                 uint8_t* __restrict__ output,
                                 ScratchPad** __restrict__ scratchPad)
    {
        keccak((const uint8_t*) input, (int) size, scratchPad[0]->state, 200);
        keccak((const uint8_t*) input + size, (int) size, scratchPad[1]->state, 200);

        uint64_t tweak1_2_0 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35) ^
                               *(reinterpret_cast<const uint64_t*>(scratchPad[0]->state) + 24));
        uint64_t tweak1_2_1 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + size) ^
                               *(reinterpret_cast<const uint64_t*>(scratchPad[1]->state) + 24));

        const uint8_t* l0 = scratchPad[0]->memory;
        const uint8_t* l1 = scratchPad[1]->memory;
        uint64_t* h0 = reinterpret_cast<uint64_t*>(scratchPad[0]->state);
        uint64_t* h1 = reinterpret_cast<uint64_t*>(scratchPad[1]->state);

        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h0, (__m128i*) l0);
        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h1, (__m128i*) l1);

        uint64_t al0 = h0[0] ^h0[4];
        uint64_t al1 = h1[0] ^h1[4];
        uint64_t ah0 = h0[1] ^h0[5];
        uint64_t ah1 = h1[1] ^h1[5];

        __m128i bx0 = _mm_set_epi64x(h0[3] ^ h0[7], h0[2] ^ h0[6]);
        __m128i bx1 = _mm_set_epi64x(h1[3] ^ h1[7], h1[2] ^ h1[6]);

        uint64_t idx0 = h0[0] ^h0[4];
        uint64_t idx1 = h1[0] ^h1[4];

        for (size_t i = 0; i < ITERATIONS; i++) {
            __m128i cx0;
            __m128i cx1;

            if (SOFT_AES) {
                cx0 = soft_aesenc((uint32_t*)&l0[idx0 & MASK], _mm_set_epi64x(ah0, al0));
                cx1 = soft_aesenc((uint32_t*)&l1[idx1 & MASK], _mm_set_epi64x(ah1, al1));
            } else {
                cx0 = _mm_load_si128((__m128i*) &l0[idx0 & MASK]);
                cx1 = _mm_load_si128((__m128i*) &l1[idx1 & MASK]);

                cx0 = _mm_aesenc_si128(cx0, _mm_set_epi64x(ah0, al0));
                cx1 = _mm_aesenc_si128(cx1, _mm_set_epi64x(ah1, al1));
            }

            _mm_store_si128((__m128i*) &l0[idx0 & MASK], _mm_xor_si128(bx0, cx0));
            _mm_store_si128((__m128i*) &l1[idx1 & MASK], _mm_xor_si128(bx1, cx1));

            static const uint32_t table = 0x75310;
            uint8_t tmp = reinterpret_cast<const uint8_t*>(&l0[idx0 & MASK])[11];
            uint8_t index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
            ((uint8_t*)(&l0[idx0 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);
            tmp = reinterpret_cast<const uint8_t*>(&l1[idx1 & MASK])[11];
            index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
            ((uint8_t*)(&l1[idx1 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);

            idx0 = EXTRACT64(cx0);
            idx1 = EXTRACT64(cx1);

            bx0 = cx0;
            bx1 = cx1;

            uint64_t hi, lo, cl, ch;
            cl = ((uint64_t*) &l0[idx0 & MASK])[0];
            ch = ((uint64_t*) &l0[idx0 & MASK])[1];
            lo = __umul128(idx0, cl, &hi);

            al0 += hi;
            ah0 += lo;

            ah0 ^= tweak1_2_0;
            ((uint64_t*) &l0[idx0 & MASK])[0] = al0;
            ((uint64_t*) &l0[idx0 & MASK])[1] = ah0;
            ah0 ^= tweak1_2_0;

            ((uint64_t*)&l0[idx0 & MASK])[1] ^= ((uint64_t*)&l0[idx0 & MASK])[0];

            ah0 ^= ch;
            al0 ^= cl;
            idx0 = al0;

            cl = ((uint64_t*) &l1[idx1 & MASK])[0];
            ch = ((uint64_t*) &l1[idx1 & MASK])[1];
            lo = __umul128(idx1, cl, &hi);

            al1 += hi;
            ah1 += lo;

            ah1 ^= tweak1_2_1;
            ((uint64_t*) &l1[idx1 & MASK])[0] = al1;
            ((uint64_t*) &l1[idx1 & MASK])[1] = ah1;
            ah1 ^= tweak1_2_1;

            ((uint64_t*)&l1[idx1 & MASK])[1] ^= ((uint64_t*)&l1[idx1 & MASK])[0];

            ah1 ^= ch;
            al1 ^= cl;
            idx1 = al1;
        }

        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l0, (__m128i*) h0);
        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l1, (__m128i*) h1);

        keccakf(h0, 24);
        keccakf(h1, 24);

        extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
        extra_hashes[scratchPad[1]->state[0] & 3](scratchPad[1]->state, 200, output + 32);
    }

    inline static void hashHeavy(const uint8_t* __restrict__ input,
                            size_t size,
                            uint8_t* __restrict__ output,
                            ScratchPad** __restrict__ scratchPad)
    {
        keccak((const uint8_t*) input, (int) size, scratchPad[0]->state, 200);
        keccak((const uint8_t*) input + size, (int) size, scratchPad[1]->state, 200);

        const uint8_t* l0 = scratchPad[0]->memory;
        const uint8_t* l1 = scratchPad[1]->memory;
        uint64_t* h0 = reinterpret_cast<uint64_t*>(scratchPad[0]->state);
        uint64_t* h1 = reinterpret_cast<uint64_t*>(scratchPad[1]->state);

        cn_explode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) h0, (__m128i*) l0);
        cn_explode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) h1, (__m128i*) l1);

        uint64_t al0 = h0[0] ^h0[4];
        uint64_t al1 = h1[0] ^h1[4];
        uint64_t ah0 = h0[1] ^h0[5];
        uint64_t ah1 = h1[1] ^h1[5];

        __m128i bx0 = _mm_set_epi64x(h0[3] ^ h0[7], h0[2] ^ h0[6]);
        __m128i bx1 = _mm_set_epi64x(h1[3] ^ h1[7], h1[2] ^ h1[6]);

        uint64_t idx0 = h0[0] ^h0[4];
        uint64_t idx1 = h1[0] ^h1[4];

        for (size_t i = 0; i < ITERATIONS; i++) {
            __m128i cx0;
            __m128i cx1;

            if (SOFT_AES) {
                cx0 = soft_aesenc((uint32_t*)&l0[idx0 & MASK], _mm_set_epi64x(ah0, al0));
                cx1 = soft_aesenc((uint32_t*)&l1[idx1 & MASK], _mm_set_epi64x(ah1, al1));
            } else {
                cx0 = _mm_load_si128((__m128i*) &l0[idx0 & MASK]);
                cx1 = _mm_load_si128((__m128i*) &l1[idx1 & MASK]);

                cx0 = _mm_aesenc_si128(cx0, _mm_set_epi64x(ah0, al0));
                cx1 = _mm_aesenc_si128(cx1, _mm_set_epi64x(ah1, al1));
            }

            _mm_store_si128((__m128i*) &l0[idx0 & MASK], _mm_xor_si128(bx0, cx0));
            _mm_store_si128((__m128i*) &l1[idx1 & MASK], _mm_xor_si128(bx1, cx1));

            idx0 = EXTRACT64(cx0);
            idx1 = EXTRACT64(cx1);

            bx0 = cx0;
            bx1 = cx1;

            uint64_t hi, lo, cl, ch;
            cl = ((uint64_t*) &l0[idx0 & MASK])[0];
            ch = ((uint64_t*) &l0[idx0 & MASK])[1];
            lo = __umul128(idx0, cl, &hi);

            al0 += hi;
            ah0 += lo;

            ((uint64_t*) &l0[idx0 & MASK])[0] = al0;
            ((uint64_t*) &l0[idx0 & MASK])[1] = ah0;

            ah0 ^= ch;
            al0 ^= cl;
            idx0 = al0;

            int64_t n  = ((int64_t*)&l0[idx0 & MASK])[0];
            int32_t d  = ((int32_t*)&l0[idx0 & MASK])[2];
            int64_t q = n / (d | 0x5);

            ((int64_t*)&l0[idx0 & MASK])[0] = n ^ q;
            idx0 = d ^ q;


            cl = ((uint64_t*) &l1[idx1 & MASK])[0];
            ch = ((uint64_t*) &l1[idx1 & MASK])[1];
            lo = __umul128(idx1, cl, &hi);

            al1 += hi;
            ah1 += lo;

            ((uint64_t*) &l1[idx1 & MASK])[0] = al1;
            ((uint64_t*) &l1[idx1 & MASK])[1] = ah1;

            ah1 ^= ch;
            al1 ^= cl;
            idx1 = al1;

            n  = ((int64_t*)&l1[idx1 & MASK])[0];
            d  = ((int32_t*)&l1[idx1 & MASK])[2];
            q = n / (d | 0x5);

            ((int64_t*)&l1[idx1 & MASK])[0] = n ^ q;
            idx1 = d ^ q;
        }

        cn_implode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) l0, (__m128i*) h0);
        cn_implode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) l1, (__m128i*) h1);

        keccakf(h0, 24);
        keccakf(h1, 24);

        extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
        extra_hashes[scratchPad[1]->state[0] & 3](scratchPad[1]->state, 200, output + 32);
    }

    inline static void hashHeavyHaven(const uint8_t* __restrict__ input,
                                 size_t size,
                                 uint8_t* __restrict__ output,
                                 ScratchPad** __restrict__ scratchPad)
    {
        keccak((const uint8_t*) input, (int) size, scratchPad[0]->state, 200);
        keccak((const uint8_t*) input + size, (int) size, scratchPad[1]->state, 200);

        const uint8_t* l0 = scratchPad[0]->memory;
        const uint8_t* l1 = scratchPad[1]->memory;
        uint64_t* h0 = reinterpret_cast<uint64_t*>(scratchPad[0]->state);
        uint64_t* h1 = reinterpret_cast<uint64_t*>(scratchPad[1]->state);

        cn_explode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) h0, (__m128i*) l0);
        cn_explode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) h1, (__m128i*) l1);

        uint64_t al0 = h0[0] ^h0[4];
        uint64_t al1 = h1[0] ^h1[4];
        uint64_t ah0 = h0[1] ^h0[5];
        uint64_t ah1 = h1[1] ^h1[5];

        __m128i bx0 = _mm_set_epi64x(h0[3] ^ h0[7], h0[2] ^ h0[6]);
        __m128i bx1 = _mm_set_epi64x(h1[3] ^ h1[7], h1[2] ^ h1[6]);

        uint64_t idx0 = h0[0] ^h0[4];
        uint64_t idx1 = h1[0] ^h1[4];

        for (size_t i = 0; i < ITERATIONS; i++) {
            __m128i cx0;
            __m128i cx1;

            if (SOFT_AES) {
                cx0 = soft_aesenc((uint32_t*)&l0[idx0 & MASK], _mm_set_epi64x(ah0, al0));
                cx1 = soft_aesenc((uint32_t*)&l1[idx1 & MASK], _mm_set_epi64x(ah1, al1));
            } else {
                cx0 = _mm_load_si128((__m128i*) &l0[idx0 & MASK]);
                cx1 = _mm_load_si128((__m128i*) &l1[idx1 & MASK]);

                cx0 = _mm_aesenc_si128(cx0, _mm_set_epi64x(ah0, al0));
                cx1 = _mm_aesenc_si128(cx1, _mm_set_epi64x(ah1, al1));
            }

            _mm_store_si128((__m128i*) &l0[idx0 & MASK], _mm_xor_si128(bx0, cx0));
            _mm_store_si128((__m128i*) &l1[idx1 & MASK], _mm_xor_si128(bx1, cx1));

            idx0 = EXTRACT64(cx0);
            idx1 = EXTRACT64(cx1);

            bx0 = cx0;
            bx1 = cx1;

            uint64_t hi, lo, cl, ch;
            cl = ((uint64_t*) &l0[idx0 & MASK])[0];
            ch = ((uint64_t*) &l0[idx0 & MASK])[1];
            lo = __umul128(idx0, cl, &hi);

            al0 += hi;
            ah0 += lo;

            ((uint64_t*) &l0[idx0 & MASK])[0] = al0;
            ((uint64_t*) &l0[idx0 & MASK])[1] = ah0;

            ah0 ^= ch;
            al0 ^= cl;
            idx0 = al0;

            int64_t n  = ((int64_t*)&l0[idx0 & MASK])[0];
            int32_t d  = ((int32_t*)&l0[idx0 & MASK])[2];
            int64_t q = n / (d | 0x5);

            ((int64_t*)&l0[idx0 & MASK])[0] = n ^ q;
            idx0 = (~d) ^ q;


            cl = ((uint64_t*) &l1[idx1 & MASK])[0];
            ch = ((uint64_t*) &l1[idx1 & MASK])[1];
            lo = __umul128(idx1, cl, &hi);

            al1 += hi;
            ah1 += lo;

            ((uint64_t*) &l1[idx1 & MASK])[0] = al1;
            ((uint64_t*) &l1[idx1 & MASK])[1] = ah1;

            ah1 ^= ch;
            al1 ^= cl;
            idx1 = al1;

            n  = ((int64_t*)&l1[idx1 & MASK])[0];
            d  = ((int32_t*)&l1[idx1 & MASK])[2];
            q = n / (d | 0x5);

            ((int64_t*)&l1[idx1 & MASK])[0] = n ^ q;
            idx1 = (~d) ^ q;
        }

        cn_implode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) l0, (__m128i*) h0);
        cn_implode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) l1, (__m128i*) h1);

        keccakf(h0, 24);
        keccakf(h1, 24);

        extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
        extra_hashes[scratchPad[1]->state[0] & 3](scratchPad[1]->state, 200, output + 32);
    }

    inline static void hashHeavyTube(const uint8_t* __restrict__ input,
                                     size_t size,
                                     uint8_t* __restrict__ output,
                                     ScratchPad** __restrict__ scratchPad)
    {
        keccak((const uint8_t*) input, (int) size, scratchPad[0]->state, 200);
        keccak((const uint8_t*) input + size, (int) size, scratchPad[1]->state, 200);

        uint64_t tweak1_2_0 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35) ^
                               *(reinterpret_cast<const uint64_t*>(scratchPad[0]->state) + 24));
        uint64_t tweak1_2_1 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + size) ^
                               *(reinterpret_cast<const uint64_t*>(scratchPad[1]->state) + 24));

        const uint8_t* l0 = scratchPad[0]->memory;
        const uint8_t* l1 = scratchPad[1]->memory;
        uint64_t* h0 = reinterpret_cast<uint64_t*>(scratchPad[0]->state);
        uint64_t* h1 = reinterpret_cast<uint64_t*>(scratchPad[1]->state);

        cn_explode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) h0, (__m128i*) l0);
        cn_explode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) h1, (__m128i*) l1);

        uint64_t al0 = h0[0] ^h0[4];
        uint64_t al1 = h1[0] ^h1[4];
        uint64_t ah0 = h0[1] ^h0[5];
        uint64_t ah1 = h1[1] ^h1[5];

        __m128i bx0 = _mm_set_epi64x(h0[3] ^ h0[7], h0[2] ^ h0[6]);
        __m128i bx1 = _mm_set_epi64x(h1[3] ^ h1[7], h1[2] ^ h1[6]);

        uint64_t idx0 = h0[0] ^h0[4];
        uint64_t idx1 = h1[0] ^h1[4];

        union alignas(16) {
            uint32_t k[4];
            uint64_t v64[2];
        };
        alignas(16) uint32_t x[4];

#define BYTE(p, i) ((unsigned char*)&p)[i]
        for (size_t i = 0; i < ITERATIONS; i++) {
            __m128i cx0 = _mm_load_si128((__m128i*) &l0[idx0 & MASK]);
            __m128i cx1 = _mm_load_si128((__m128i*) &l1[idx1 & MASK]);

            const __m128i& key0 = _mm_set_epi64x(ah0, al0);

            _mm_store_si128((__m128i*)k, key0);
            cx0 = _mm_xor_si128(cx0, _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128()));
            _mm_store_si128((__m128i*)x, cx0);

            k[0] ^= saes_table[0][BYTE(x[0], 0)] ^ saes_table[1][BYTE(x[1], 1)] ^ saes_table[2][BYTE(x[2], 2)] ^ saes_table[3][BYTE(x[3], 3)];
            x[0] ^= k[0];
            k[1] ^= saes_table[0][BYTE(x[1], 0)] ^ saes_table[1][BYTE(x[2], 1)] ^ saes_table[2][BYTE(x[3], 2)] ^ saes_table[3][BYTE(x[0], 3)];
            x[1] ^= k[1];
            k[2] ^= saes_table[0][BYTE(x[2], 0)] ^ saes_table[1][BYTE(x[3], 1)] ^ saes_table[2][BYTE(x[0], 2)] ^ saes_table[3][BYTE(x[1], 3)];
            x[2] ^= k[2];
            k[3] ^= saes_table[0][BYTE(x[3], 0)] ^ saes_table[1][BYTE(x[0], 1)] ^ saes_table[2][BYTE(x[1], 2)] ^ saes_table[3][BYTE(x[2], 3)];

            cx0 = _mm_load_si128((__m128i*)k);

            const __m128i& key1 = _mm_set_epi64x(ah1, al1);

            _mm_store_si128((__m128i*)k, key1);
            cx1 = _mm_xor_si128(cx1, _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128()));
            _mm_store_si128((__m128i*)x, cx1);

            k[0] ^= saes_table[0][BYTE(x[0], 0)] ^ saes_table[1][BYTE(x[1], 1)] ^ saes_table[2][BYTE(x[2], 2)] ^ saes_table[3][BYTE(x[3], 3)];
            x[0] ^= k[0];
            k[1] ^= saes_table[0][BYTE(x[1], 0)] ^ saes_table[1][BYTE(x[2], 1)] ^ saes_table[2][BYTE(x[3], 2)] ^ saes_table[3][BYTE(x[0], 3)];
            x[1] ^= k[1];
            k[2] ^= saes_table[0][BYTE(x[2], 0)] ^ saes_table[1][BYTE(x[3], 1)] ^ saes_table[2][BYTE(x[0], 2)] ^ saes_table[3][BYTE(x[1], 3)];
            x[2] ^= k[2];
            k[3] ^= saes_table[0][BYTE(x[3], 0)] ^ saes_table[1][BYTE(x[0], 1)] ^ saes_table[2][BYTE(x[1], 2)] ^ saes_table[3][BYTE(x[2], 3)];

            cx1 = _mm_load_si128((__m128i*)k);

            _mm_store_si128((__m128i*) &l0[idx0 & MASK], _mm_xor_si128(bx0, cx0));
            _mm_store_si128((__m128i*) &l1[idx1 & MASK], _mm_xor_si128(bx1, cx1));

            static const uint32_t table = 0x75310;
            uint8_t tmp = reinterpret_cast<const uint8_t*>(&l0[idx0 & MASK])[11];
            uint8_t index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
            ((uint8_t*)(&l0[idx0 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);
            tmp = reinterpret_cast<const uint8_t*>(&l1[idx1 & MASK])[11];
            index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
            ((uint8_t*)(&l1[idx1 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);

            idx0 = EXTRACT64(cx0);
            idx1 = EXTRACT64(cx1);

            bx0 = cx0;
            bx1 = cx1;

            uint64_t hi, lo, cl, ch;
            cl = ((uint64_t*) &l0[idx0 & MASK])[0];
            ch = ((uint64_t*) &l0[idx0 & MASK])[1];
            lo = __umul128(idx0, cl, &hi);

            al0 += hi;
            ah0 += lo;

            ah0 ^= tweak1_2_0;
            ((uint64_t*) &l0[idx0 & MASK])[0] = al0;
            ((uint64_t*) &l0[idx0 & MASK])[1] = ah0;
            ah0 ^= tweak1_2_0;

            ((uint64_t*)&l0[idx0 & MASK])[1] ^= ((uint64_t*)&l0[idx0 & MASK])[0];

            ah0 ^= ch;
            al0 ^= cl;
            idx0 = al0;

            int64_t n  = ((int64_t*)&l0[idx0 & MASK])[0];
            int32_t d  = ((int32_t*)&l0[idx0 & MASK])[2];
            int64_t q = n / (d | 0x5);

            ((int64_t*)&l0[idx0 & MASK])[0] = n ^ q;
            idx0 = d ^ q;


            cl = ((uint64_t*) &l1[idx1 & MASK])[0];
            ch = ((uint64_t*) &l1[idx1 & MASK])[1];
            lo = __umul128(idx1, cl, &hi);

            al1 += hi;
            ah1 += lo;

            ah1 ^= tweak1_2_1;
            ((uint64_t*) &l1[idx1 & MASK])[0] = al1;
            ((uint64_t*) &l1[idx1 & MASK])[1] = ah1;
            ah1 ^= tweak1_2_1;

            ((uint64_t*)&l1[idx1 & MASK])[1] ^= ((uint64_t*)&l1[idx1 & MASK])[0];

            ah1 ^= ch;
            al1 ^= cl;
            idx1 = al1;

            n  = ((int64_t*)&l1[idx1 & MASK])[0];
            d  = ((int32_t*)&l1[idx1 & MASK])[2];
            q = n / (d | 0x5);

            ((int64_t*)&l1[idx1 & MASK])[0] = n ^ q;
            idx1 = d ^ q;
        }
#undef BYTE

        cn_implode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) l0, (__m128i*) h0);
        cn_implode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) l1, (__m128i*) h1);

        keccakf(h0, 24);
        keccakf(h1, 24);

        extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
        extra_hashes[scratchPad[1]->state[0] & 3](scratchPad[1]->state, 200, output + 32);
    }
};

template<size_t ITERATIONS, size_t INDEX_SHIFT, size_t MEM, size_t MASK, bool SOFT_AES>
class CryptoNightMultiHash<ITERATIONS, INDEX_SHIFT, MEM, MASK, SOFT_AES, 3>
{
public:
    inline static void hash(const uint8_t* __restrict__ input,
                            size_t size,
                            uint8_t* __restrict__ output,
                            ScratchPad** __restrict__ scratchPad)
    {
        keccak((const uint8_t*) input, (int) size, scratchPad[0]->state, 200);
        keccak((const uint8_t*) input + size, (int) size, scratchPad[1]->state, 200);
        keccak((const uint8_t*) input + 2 * size, (int) size, scratchPad[2]->state, 200);

        const uint8_t* l0 = scratchPad[0]->memory;
        const uint8_t* l1 = scratchPad[1]->memory;
        const uint8_t* l2 = scratchPad[2]->memory;
        uint64_t* h0 = reinterpret_cast<uint64_t*>(scratchPad[0]->state);
        uint64_t* h1 = reinterpret_cast<uint64_t*>(scratchPad[1]->state);
        uint64_t* h2 = reinterpret_cast<uint64_t*>(scratchPad[2]->state);

        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h0, (__m128i*) l0);
        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h1, (__m128i*) l1);
        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h2, (__m128i*) l2);

        uint64_t al0 = h0[0] ^h0[4];
        uint64_t al1 = h1[0] ^h1[4];
        uint64_t al2 = h2[0] ^h2[4];
        uint64_t ah0 = h0[1] ^h0[5];
        uint64_t ah1 = h1[1] ^h1[5];
        uint64_t ah2 = h2[1] ^h2[5];

        __m128i bx0 = _mm_set_epi64x(h0[3] ^ h0[7], h0[2] ^ h0[6]);
        __m128i bx1 = _mm_set_epi64x(h1[3] ^ h1[7], h1[2] ^ h1[6]);
        __m128i bx2 = _mm_set_epi64x(h2[3] ^ h2[7], h2[2] ^ h2[6]);

        uint64_t idx0 = h0[0] ^h0[4];
        uint64_t idx1 = h1[0] ^h1[4];
        uint64_t idx2 = h2[0] ^h2[4];

        for (size_t i = 0; i < ITERATIONS; i++) {
            __m128i cx0;
            __m128i cx1;
            __m128i cx2;

            if (SOFT_AES) {
                cx0 = soft_aesenc((uint32_t*)&l0[idx0 & MASK], _mm_set_epi64x(ah0, al0));
                cx1 = soft_aesenc((uint32_t*)&l1[idx1 & MASK], _mm_set_epi64x(ah1, al1));
                cx2 = soft_aesenc((uint32_t*)&l2[idx2 & MASK], _mm_set_epi64x(ah2, al2));
            } else {
                cx0 = _mm_load_si128((__m128i*) &l0[idx0 & MASK]);
                cx1 = _mm_load_si128((__m128i*) &l1[idx1 & MASK]);
                cx2 = _mm_load_si128((__m128i*) &l2[idx2 & MASK]);

                cx0 = _mm_aesenc_si128(cx0, _mm_set_epi64x(ah0, al0));
                cx1 = _mm_aesenc_si128(cx1, _mm_set_epi64x(ah1, al1));
                cx2 = _mm_aesenc_si128(cx2, _mm_set_epi64x(ah2, al2));
            }

            _mm_store_si128((__m128i*) &l0[idx0 & MASK], _mm_xor_si128(bx0, cx0));
            _mm_store_si128((__m128i*) &l1[idx1 & MASK], _mm_xor_si128(bx1, cx1));
            _mm_store_si128((__m128i*) &l2[idx2 & MASK], _mm_xor_si128(bx2, cx2));

            idx0 = EXTRACT64(cx0);
            idx1 = EXTRACT64(cx1);
            idx2 = EXTRACT64(cx2);

            bx0 = cx0;
            bx1 = cx1;
            bx2 = cx2;


            uint64_t hi, lo, cl, ch;
            cl = ((uint64_t*) &l0[idx0 & MASK])[0];
            ch = ((uint64_t*) &l0[idx0 & MASK])[1];
            lo = __umul128(idx0, cl, &hi);

            al0 += hi;
            ah0 += lo;

            ((uint64_t*) &l0[idx0 & MASK])[0] = al0;
            ((uint64_t*) &l0[idx0 & MASK])[1] = ah0;

            ah0 ^= ch;
            al0 ^= cl;
            idx0 = al0;


            cl = ((uint64_t*) &l1[idx1 & MASK])[0];
            ch = ((uint64_t*) &l1[idx1 & MASK])[1];
            lo = __umul128(idx1, cl, &hi);

            al1 += hi;
            ah1 += lo;

            ((uint64_t*) &l1[idx1 & MASK])[0] = al1;
            ((uint64_t*) &l1[idx1 & MASK])[1] = ah1;

            ah1 ^= ch;
            al1 ^= cl;
            idx1 = al1;


            cl = ((uint64_t*) &l2[idx2 & MASK])[0];
            ch = ((uint64_t*) &l2[idx2 & MASK])[1];
            lo = __umul128(idx2, cl, &hi);

            al2 += hi;
            ah2 += lo;

            ((uint64_t*) &l2[idx2 & MASK])[0] = al2;
            ((uint64_t*) &l2[idx2 & MASK])[1] = ah2;

            ah2 ^= ch;
            al2 ^= cl;
            idx2 = al2;
        }

        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l0, (__m128i*) h0);
        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l1, (__m128i*) h1);
        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l2, (__m128i*) h2);

        keccakf(h0, 24);
        keccakf(h1, 24);
        keccakf(h2, 24);

        extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
        extra_hashes[scratchPad[1]->state[0] & 3](scratchPad[1]->state, 200, output + 32);
        extra_hashes[scratchPad[2]->state[0] & 3](scratchPad[2]->state, 200, output + 64);
    }

  inline static void hashPowV2(const uint8_t* __restrict__ input,
                          size_t size,
                          uint8_t* __restrict__ output,
                          ScratchPad** __restrict__ scratchPad)
  {
      keccak((const uint8_t*) input, (int) size, scratchPad[0]->state, 200);
      keccak((const uint8_t*) input + size, (int) size, scratchPad[1]->state, 200);
      keccak((const uint8_t*) input + 2 * size, (int) size, scratchPad[2]->state, 200);

      uint64_t tweak1_2_0 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35) ^
                             *(reinterpret_cast<const uint64_t*>(scratchPad[0]->state) + 24));
      uint64_t tweak1_2_1 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + size) ^
                             *(reinterpret_cast<const uint64_t*>(scratchPad[1]->state) + 24));
      uint64_t tweak1_2_2 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + 2 * size) ^
                             *(reinterpret_cast<const uint64_t*>(scratchPad[2]->state) + 24));

      const uint8_t* l0 = scratchPad[0]->memory;
      const uint8_t* l1 = scratchPad[1]->memory;
      const uint8_t* l2 = scratchPad[2]->memory;
      uint64_t* h0 = reinterpret_cast<uint64_t*>(scratchPad[0]->state);
      uint64_t* h1 = reinterpret_cast<uint64_t*>(scratchPad[1]->state);
      uint64_t* h2 = reinterpret_cast<uint64_t*>(scratchPad[2]->state);

      cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h0, (__m128i*) l0);
      cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h1, (__m128i*) l1);
      cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h2, (__m128i*) l2);

      uint64_t al0 = h0[0] ^h0[4];
      uint64_t al1 = h1[0] ^h1[4];
      uint64_t al2 = h2[0] ^h2[4];
      uint64_t ah0 = h0[1] ^h0[5];
      uint64_t ah1 = h1[1] ^h1[5];
      uint64_t ah2 = h2[1] ^h2[5];

      __m128i bx0 = _mm_set_epi64x(h0[3] ^ h0[7], h0[2] ^ h0[6]);
      __m128i bx1 = _mm_set_epi64x(h1[3] ^ h1[7], h1[2] ^ h1[6]);
      __m128i bx2 = _mm_set_epi64x(h2[3] ^ h2[7], h2[2] ^ h2[6]);

      uint64_t idx0 = h0[0] ^h0[4];
      uint64_t idx1 = h1[0] ^h1[4];
      uint64_t idx2 = h2[0] ^h2[4];

      for (size_t i = 0; i < ITERATIONS; i++) {
          __m128i cx0;
          __m128i cx1;
          __m128i cx2;

          if (SOFT_AES) {
              cx0 = soft_aesenc((uint32_t*)&l0[idx0 & MASK], _mm_set_epi64x(ah0, al0));
              cx1 = soft_aesenc((uint32_t*)&l1[idx1 & MASK], _mm_set_epi64x(ah1, al1));
              cx2 = soft_aesenc((uint32_t*)&l2[idx2 & MASK], _mm_set_epi64x(ah2, al2));
          } else {
              cx0 = _mm_load_si128((__m128i*) &l0[idx0 & MASK]);
              cx1 = _mm_load_si128((__m128i*) &l1[idx1 & MASK]);
              cx2 = _mm_load_si128((__m128i*) &l2[idx2 & MASK]);

              cx0 = _mm_aesenc_si128(cx0, _mm_set_epi64x(ah0, al0));
              cx1 = _mm_aesenc_si128(cx1, _mm_set_epi64x(ah1, al1));
              cx2 = _mm_aesenc_si128(cx2, _mm_set_epi64x(ah2, al2));
          }

          _mm_store_si128((__m128i*) &l0[idx0 & MASK], _mm_xor_si128(bx0, cx0));
          _mm_store_si128((__m128i*) &l1[idx1 & MASK], _mm_xor_si128(bx1, cx1));
          _mm_store_si128((__m128i*) &l2[idx2 & MASK], _mm_xor_si128(bx2, cx2));

          static const uint32_t table = 0x75310;
          uint8_t tmp = reinterpret_cast<const uint8_t*>(&l0[idx0 & MASK])[11];
          uint8_t index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
          ((uint8_t*)(&l0[idx0 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);
          tmp = reinterpret_cast<const uint8_t*>(&l1[idx1 & MASK])[11];
          index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
          ((uint8_t*)(&l1[idx1 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);
          tmp = reinterpret_cast<const uint8_t*>(&l2[idx2 & MASK])[11];
          index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
          ((uint8_t*)(&l2[idx2 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);

          idx0 = EXTRACT64(cx0);
          idx1 = EXTRACT64(cx1);
          idx2 = EXTRACT64(cx2);

          bx0 = cx0;
          bx1 = cx1;
          bx2 = cx2;


          uint64_t hi, lo, cl, ch;
          cl = ((uint64_t*) &l0[idx0 & MASK])[0];
          ch = ((uint64_t*) &l0[idx0 & MASK])[1];
          lo = __umul128(idx0, cl, &hi);

          al0 += hi;
          ah0 += lo;

          ah0 ^= tweak1_2_0;
          ((uint64_t*) &l0[idx0 & MASK])[0] = al0;
          ((uint64_t*) &l0[idx0 & MASK])[1] = ah0;
          ah0 ^= tweak1_2_0;

          ah0 ^= ch;
          al0 ^= cl;
          idx0 = al0;


          cl = ((uint64_t*) &l1[idx1 & MASK])[0];
          ch = ((uint64_t*) &l1[idx1 & MASK])[1];
          lo = __umul128(idx1, cl, &hi);

          al1 += hi;
          ah1 += lo;

          ah1 ^= tweak1_2_1;
          ((uint64_t*) &l1[idx1 & MASK])[0] = al1;
          ((uint64_t*) &l1[idx1 & MASK])[1] = ah1;
          ah1 ^= tweak1_2_1;

          ah1 ^= ch;
          al1 ^= cl;
          idx1 = al1;


          cl = ((uint64_t*) &l2[idx2 & MASK])[0];
          ch = ((uint64_t*) &l2[idx2 & MASK])[1];
          lo = __umul128(idx2, cl, &hi);

          al2 += hi;
          ah2 += lo;

          ah2 ^= tweak1_2_2;
          ((uint64_t*) &l2[idx2 & MASK])[0] = al2;
          ((uint64_t*) &l2[idx2 & MASK])[1] = ah2;
          ah2 ^= tweak1_2_2;

          ah2 ^= ch;
          al2 ^= cl;
          idx2 = al2;
      }

      cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l0, (__m128i*) h0);
      cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l1, (__m128i*) h1);
      cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l2, (__m128i*) h2);

      keccakf(h0, 24);
      keccakf(h1, 24);
      keccakf(h2, 24);

      extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
      extra_hashes[scratchPad[1]->state[0] & 3](scratchPad[1]->state, 200, output + 32);
      extra_hashes[scratchPad[2]->state[0] & 3](scratchPad[2]->state, 200, output + 64);
  }

    inline static void hashLiteTube(const uint8_t* __restrict__ input,
                                 size_t size,
                                 uint8_t* __restrict__ output,
                                 ScratchPad** __restrict__ scratchPad)
    {
        keccak((const uint8_t*) input, (int) size, scratchPad[0]->state, 200);
        keccak((const uint8_t*) input + size, (int) size, scratchPad[1]->state, 200);
        keccak((const uint8_t*) input + 2 * size, (int) size, scratchPad[2]->state, 200);

        uint64_t tweak1_2_0 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35) ^
                               *(reinterpret_cast<const uint64_t*>(scratchPad[0]->state) + 24));
        uint64_t tweak1_2_1 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + size) ^
                               *(reinterpret_cast<const uint64_t*>(scratchPad[1]->state) + 24));
        uint64_t tweak1_2_2 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + 2 * size) ^
                               *(reinterpret_cast<const uint64_t*>(scratchPad[2]->state) + 24));

        const uint8_t* l0 = scratchPad[0]->memory;
        const uint8_t* l1 = scratchPad[1]->memory;
        const uint8_t* l2 = scratchPad[2]->memory;
        uint64_t* h0 = reinterpret_cast<uint64_t*>(scratchPad[0]->state);
        uint64_t* h1 = reinterpret_cast<uint64_t*>(scratchPad[1]->state);
        uint64_t* h2 = reinterpret_cast<uint64_t*>(scratchPad[2]->state);

        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h0, (__m128i*) l0);
        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h1, (__m128i*) l1);
        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h2, (__m128i*) l2);

        uint64_t al0 = h0[0] ^h0[4];
        uint64_t al1 = h1[0] ^h1[4];
        uint64_t al2 = h2[0] ^h2[4];
        uint64_t ah0 = h0[1] ^h0[5];
        uint64_t ah1 = h1[1] ^h1[5];
        uint64_t ah2 = h2[1] ^h2[5];

        __m128i bx0 = _mm_set_epi64x(h0[3] ^ h0[7], h0[2] ^ h0[6]);
        __m128i bx1 = _mm_set_epi64x(h1[3] ^ h1[7], h1[2] ^ h1[6]);
        __m128i bx2 = _mm_set_epi64x(h2[3] ^ h2[7], h2[2] ^ h2[6]);

        uint64_t idx0 = h0[0] ^h0[4];
        uint64_t idx1 = h1[0] ^h1[4];
        uint64_t idx2 = h2[0] ^h2[4];

        for (size_t i = 0; i < ITERATIONS; i++) {
            __m128i cx0;
            __m128i cx1;
            __m128i cx2;

            if (SOFT_AES) {
                cx0 = soft_aesenc((uint32_t*)&l0[idx0 & MASK], _mm_set_epi64x(ah0, al0));
                cx1 = soft_aesenc((uint32_t*)&l1[idx1 & MASK], _mm_set_epi64x(ah1, al1));
                cx2 = soft_aesenc((uint32_t*)&l2[idx2 & MASK], _mm_set_epi64x(ah2, al2));
            } else {
                cx0 = _mm_load_si128((__m128i*) &l0[idx0 & MASK]);
                cx1 = _mm_load_si128((__m128i*) &l1[idx1 & MASK]);
                cx2 = _mm_load_si128((__m128i*) &l2[idx2 & MASK]);

                cx0 = _mm_aesenc_si128(cx0, _mm_set_epi64x(ah0, al0));
                cx1 = _mm_aesenc_si128(cx1, _mm_set_epi64x(ah1, al1));
                cx2 = _mm_aesenc_si128(cx2, _mm_set_epi64x(ah2, al2));
            }

            _mm_store_si128((__m128i*) &l0[idx0 & MASK], _mm_xor_si128(bx0, cx0));
            _mm_store_si128((__m128i*) &l1[idx1 & MASK], _mm_xor_si128(bx1, cx1));
            _mm_store_si128((__m128i*) &l2[idx2 & MASK], _mm_xor_si128(bx2, cx2));

            static const uint32_t table = 0x75310;
            uint8_t tmp = reinterpret_cast<const uint8_t*>(&l0[idx0 & MASK])[11];
            uint8_t index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
            ((uint8_t*)(&l0[idx0 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);
            tmp = reinterpret_cast<const uint8_t*>(&l1[idx1 & MASK])[11];
            index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
            ((uint8_t*)(&l1[idx1 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);
            tmp = reinterpret_cast<const uint8_t*>(&l2[idx2 & MASK])[11];
            index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
            ((uint8_t*)(&l2[idx2 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);

            idx0 = EXTRACT64(cx0);
            idx1 = EXTRACT64(cx1);
            idx2 = EXTRACT64(cx2);

            bx0 = cx0;
            bx1 = cx1;
            bx2 = cx2;


            uint64_t hi, lo, cl, ch;
            cl = ((uint64_t*) &l0[idx0 & MASK])[0];
            ch = ((uint64_t*) &l0[idx0 & MASK])[1];
            lo = __umul128(idx0, cl, &hi);

            al0 += hi;
            ah0 += lo;

            ah0 ^= tweak1_2_0;
            ((uint64_t*) &l0[idx0 & MASK])[0] = al0;
            ((uint64_t*) &l0[idx0 & MASK])[1] = ah0;
            ah0 ^= tweak1_2_0;

            ((uint64_t*)&l0[idx0 & MASK])[1] ^= ((uint64_t*)&l0[idx0 & MASK])[0];

            ah0 ^= ch;
            al0 ^= cl;
            idx0 = al0;


            cl = ((uint64_t*) &l1[idx1 & MASK])[0];
            ch = ((uint64_t*) &l1[idx1 & MASK])[1];
            lo = __umul128(idx1, cl, &hi);

            al1 += hi;
            ah1 += lo;

            ah1 ^= tweak1_2_1;
            ((uint64_t*) &l1[idx1 & MASK])[0] = al1;
            ((uint64_t*) &l1[idx1 & MASK])[1] = ah1;
            ah1 ^= tweak1_2_1;

            ((uint64_t*)&l1[idx1 & MASK])[1] ^= ((uint64_t*)&l1[idx1 & MASK])[0];

            ah1 ^= ch;
            al1 ^= cl;
            idx1 = al1;


            cl = ((uint64_t*) &l2[idx2 & MASK])[0];
            ch = ((uint64_t*) &l2[idx2 & MASK])[1];
            lo = __umul128(idx2, cl, &hi);

            al2 += hi;
            ah2 += lo;

            ah2 ^= tweak1_2_2;
            ((uint64_t*) &l2[idx2 & MASK])[0] = al2;
            ((uint64_t*) &l2[idx2 & MASK])[1] = ah2;
            ah2 ^= tweak1_2_2;

            ((uint64_t*)&l2[idx2 & MASK])[1] ^= ((uint64_t*)&l2[idx2 & MASK])[0];

            ah2 ^= ch;
            al2 ^= cl;
            idx2 = al2;
        }

        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l0, (__m128i*) h0);
        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l1, (__m128i*) h1);
        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l2, (__m128i*) h2);

        keccakf(h0, 24);
        keccakf(h1, 24);
        keccakf(h2, 24);

        extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
        extra_hashes[scratchPad[1]->state[0] & 3](scratchPad[1]->state, 200, output + 32);
        extra_hashes[scratchPad[2]->state[0] & 3](scratchPad[2]->state, 200, output + 64);
    }

    inline static void hashHeavy(const uint8_t* __restrict__ input,
                            size_t size,
                            uint8_t* __restrict__ output,
                            ScratchPad** __restrict__ scratchPad)
    {
        keccak((const uint8_t*) input, (int) size, scratchPad[0]->state, 200);
        keccak((const uint8_t*) input + size, (int) size, scratchPad[1]->state, 200);
        keccak((const uint8_t*) input + 2 * size, (int) size, scratchPad[2]->state, 200);

        const uint8_t* l0 = scratchPad[0]->memory;
        const uint8_t* l1 = scratchPad[1]->memory;
        const uint8_t* l2 = scratchPad[2]->memory;
        uint64_t* h0 = reinterpret_cast<uint64_t*>(scratchPad[0]->state);
        uint64_t* h1 = reinterpret_cast<uint64_t*>(scratchPad[1]->state);
        uint64_t* h2 = reinterpret_cast<uint64_t*>(scratchPad[2]->state);

        cn_explode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) h0, (__m128i*) l0);
        cn_explode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) h1, (__m128i*) l1);
        cn_explode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) h2, (__m128i*) l2);

        uint64_t al0 = h0[0] ^h0[4];
        uint64_t al1 = h1[0] ^h1[4];
        uint64_t al2 = h2[0] ^h2[4];
        uint64_t ah0 = h0[1] ^h0[5];
        uint64_t ah1 = h1[1] ^h1[5];
        uint64_t ah2 = h2[1] ^h2[5];

        __m128i bx0 = _mm_set_epi64x(h0[3] ^ h0[7], h0[2] ^ h0[6]);
        __m128i bx1 = _mm_set_epi64x(h1[3] ^ h1[7], h1[2] ^ h1[6]);
        __m128i bx2 = _mm_set_epi64x(h2[3] ^ h2[7], h2[2] ^ h2[6]);

        uint64_t idx0 = h0[0] ^h0[4];
        uint64_t idx1 = h1[0] ^h1[4];
        uint64_t idx2 = h2[0] ^h2[4];

        for (size_t i = 0; i < ITERATIONS; i++) {
            __m128i cx0;
            __m128i cx1;
            __m128i cx2;

            if (SOFT_AES) {
                cx0 = soft_aesenc((uint32_t*)&l0[idx0 & MASK], _mm_set_epi64x(ah0, al0));
                cx1 = soft_aesenc((uint32_t*)&l1[idx1 & MASK], _mm_set_epi64x(ah1, al1));
                cx2 = soft_aesenc((uint32_t*)&l2[idx2 & MASK], _mm_set_epi64x(ah2, al2));
            } else {
                cx0 = _mm_load_si128((__m128i*) &l0[idx0 & MASK]);
                cx1 = _mm_load_si128((__m128i*) &l1[idx1 & MASK]);
                cx2 = _mm_load_si128((__m128i*) &l2[idx2 & MASK]);

                cx0 = _mm_aesenc_si128(cx0, _mm_set_epi64x(ah0, al0));
                cx1 = _mm_aesenc_si128(cx1, _mm_set_epi64x(ah1, al1));
                cx2 = _mm_aesenc_si128(cx2, _mm_set_epi64x(ah2, al2));
            }

            _mm_store_si128((__m128i*) &l0[idx0 & MASK], _mm_xor_si128(bx0, cx0));
            _mm_store_si128((__m128i*) &l1[idx1 & MASK], _mm_xor_si128(bx1, cx1));
            _mm_store_si128((__m128i*) &l2[idx2 & MASK], _mm_xor_si128(bx2, cx2));

            idx0 = EXTRACT64(cx0);
            idx1 = EXTRACT64(cx1);
            idx2 = EXTRACT64(cx2);

            bx0 = cx0;
            bx1 = cx1;
            bx2 = cx2;


            uint64_t hi, lo, cl, ch;
            cl = ((uint64_t*) &l0[idx0 & MASK])[0];
            ch = ((uint64_t*) &l0[idx0 & MASK])[1];
            lo = __umul128(idx0, cl, &hi);

            al0 += hi;
            ah0 += lo;

            ((uint64_t*) &l0[idx0 & MASK])[0] = al0;
            ((uint64_t*) &l0[idx0 & MASK])[1] = ah0;

            ah0 ^= ch;
            al0 ^= cl;
            idx0 = al0;

            int64_t n  = ((int64_t*)&l0[idx0 & MASK])[0];
            int32_t d  = ((int32_t*)&l0[idx0 & MASK])[2];
            int64_t q = n / (d | 0x5);

            ((int64_t*)&l0[idx0 & MASK])[0] = n ^ q;
            idx0 = d ^ q;


            cl = ((uint64_t*) &l1[idx1 & MASK])[0];
            ch = ((uint64_t*) &l1[idx1 & MASK])[1];
            lo = __umul128(idx1, cl, &hi);

            al1 += hi;
            ah1 += lo;

            ((uint64_t*) &l1[idx1 & MASK])[0] = al1;
            ((uint64_t*) &l1[idx1 & MASK])[1] = ah1;

            ah1 ^= ch;
            al1 ^= cl;
            idx1 = al1;

            n  = ((int64_t*)&l1[idx1 & MASK])[0];
            d  = ((int32_t*)&l1[idx1 & MASK])[2];
            q = n / (d | 0x5);

            ((int64_t*)&l1[idx1 & MASK])[0] = n ^ q;
            idx1 = d ^ q;


            cl = ((uint64_t*) &l2[idx2 & MASK])[0];
            ch = ((uint64_t*) &l2[idx2 & MASK])[1];
            lo = __umul128(idx2, cl, &hi);

            al2 += hi;
            ah2 += lo;

            ((uint64_t*) &l2[idx2 & MASK])[0] = al2;
            ((uint64_t*) &l2[idx2 & MASK])[1] = ah2;

            ah2 ^= ch;
            al2 ^= cl;
            idx2 = al2;

            n  = ((int64_t*)&l2[idx2 & MASK])[0];
            d  = ((int32_t*)&l2[idx2 & MASK])[2];
            q = n / (d | 0x5);

            ((int64_t*)&l2[idx2 & MASK])[0] = n ^ q;
            idx2 = d ^ q;
        }

        cn_implode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) l0, (__m128i*) h0);
        cn_implode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) l1, (__m128i*) h1);
        cn_implode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) l2, (__m128i*) h2);

        keccakf(h0, 24);
        keccakf(h1, 24);
        keccakf(h2, 24);

        extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
        extra_hashes[scratchPad[1]->state[0] & 3](scratchPad[1]->state, 200, output + 32);
        extra_hashes[scratchPad[2]->state[0] & 3](scratchPad[2]->state, 200, output + 64);
    }

    inline static void hashHeavyHaven(const uint8_t* __restrict__ input,
                                 size_t size,
                                 uint8_t* __restrict__ output,
                                 ScratchPad** __restrict__ scratchPad)
    {
        keccak((const uint8_t*) input, (int) size, scratchPad[0]->state, 200);
        keccak((const uint8_t*) input + size, (int) size, scratchPad[1]->state, 200);
        keccak((const uint8_t*) input + 2 * size, (int) size, scratchPad[2]->state, 200);

        const uint8_t* l0 = scratchPad[0]->memory;
        const uint8_t* l1 = scratchPad[1]->memory;
        const uint8_t* l2 = scratchPad[2]->memory;
        uint64_t* h0 = reinterpret_cast<uint64_t*>(scratchPad[0]->state);
        uint64_t* h1 = reinterpret_cast<uint64_t*>(scratchPad[1]->state);
        uint64_t* h2 = reinterpret_cast<uint64_t*>(scratchPad[2]->state);

        cn_explode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) h0, (__m128i*) l0);
        cn_explode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) h1, (__m128i*) l1);
        cn_explode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) h2, (__m128i*) l2);

        uint64_t al0 = h0[0] ^h0[4];
        uint64_t al1 = h1[0] ^h1[4];
        uint64_t al2 = h2[0] ^h2[4];
        uint64_t ah0 = h0[1] ^h0[5];
        uint64_t ah1 = h1[1] ^h1[5];
        uint64_t ah2 = h2[1] ^h2[5];

        __m128i bx0 = _mm_set_epi64x(h0[3] ^ h0[7], h0[2] ^ h0[6]);
        __m128i bx1 = _mm_set_epi64x(h1[3] ^ h1[7], h1[2] ^ h1[6]);
        __m128i bx2 = _mm_set_epi64x(h2[3] ^ h2[7], h2[2] ^ h2[6]);

        uint64_t idx0 = h0[0] ^h0[4];
        uint64_t idx1 = h1[0] ^h1[4];
        uint64_t idx2 = h2[0] ^h2[4];

        for (size_t i = 0; i < ITERATIONS; i++) {
            __m128i cx0;
            __m128i cx1;
            __m128i cx2;

            if (SOFT_AES) {
                cx0 = soft_aesenc((uint32_t*)&l0[idx0 & MASK], _mm_set_epi64x(ah0, al0));
                cx1 = soft_aesenc((uint32_t*)&l1[idx1 & MASK], _mm_set_epi64x(ah1, al1));
                cx2 = soft_aesenc((uint32_t*)&l2[idx2 & MASK], _mm_set_epi64x(ah2, al2));
            } else {
                cx0 = _mm_load_si128((__m128i*) &l0[idx0 & MASK]);
                cx1 = _mm_load_si128((__m128i*) &l1[idx1 & MASK]);
                cx2 = _mm_load_si128((__m128i*) &l2[idx2 & MASK]);

                cx0 = _mm_aesenc_si128(cx0, _mm_set_epi64x(ah0, al0));
                cx1 = _mm_aesenc_si128(cx1, _mm_set_epi64x(ah1, al1));
                cx2 = _mm_aesenc_si128(cx2, _mm_set_epi64x(ah2, al2));
            }

            _mm_store_si128((__m128i*) &l0[idx0 & MASK], _mm_xor_si128(bx0, cx0));
            _mm_store_si128((__m128i*) &l1[idx1 & MASK], _mm_xor_si128(bx1, cx1));
            _mm_store_si128((__m128i*) &l2[idx2 & MASK], _mm_xor_si128(bx2, cx2));

            idx0 = EXTRACT64(cx0);
            idx1 = EXTRACT64(cx1);
            idx2 = EXTRACT64(cx2);

            bx0 = cx0;
            bx1 = cx1;
            bx2 = cx2;


            uint64_t hi, lo, cl, ch;
            cl = ((uint64_t*) &l0[idx0 & MASK])[0];
            ch = ((uint64_t*) &l0[idx0 & MASK])[1];
            lo = __umul128(idx0, cl, &hi);

            al0 += hi;
            ah0 += lo;

            ((uint64_t*) &l0[idx0 & MASK])[0] = al0;
            ((uint64_t*) &l0[idx0 & MASK])[1] = ah0;

            ah0 ^= ch;
            al0 ^= cl;
            idx0 = al0;

            int64_t n  = ((int64_t*)&l0[idx0 & MASK])[0];
            int32_t d  = ((int32_t*)&l0[idx0 & MASK])[2];
            int64_t q = n / (d | 0x5);

            ((int64_t*)&l0[idx0 & MASK])[0] = n ^ q;
            idx0 = (~d) ^ q;


            cl = ((uint64_t*) &l1[idx1 & MASK])[0];
            ch = ((uint64_t*) &l1[idx1 & MASK])[1];
            lo = __umul128(idx1, cl, &hi);

            al1 += hi;
            ah1 += lo;

            ((uint64_t*) &l1[idx1 & MASK])[0] = al1;
            ((uint64_t*) &l1[idx1 & MASK])[1] = ah1;

            ah1 ^= ch;
            al1 ^= cl;
            idx1 = al1;

            n  = ((int64_t*)&l1[idx1 & MASK])[0];
            d  = ((int32_t*)&l1[idx1 & MASK])[2];
            q = n / (d | 0x5);

            ((int64_t*)&l1[idx1 & MASK])[0] = n ^ q;
            idx1 = (~d) ^ q;


            cl = ((uint64_t*) &l2[idx2 & MASK])[0];
            ch = ((uint64_t*) &l2[idx2 & MASK])[1];
            lo = __umul128(idx2, cl, &hi);

            al2 += hi;
            ah2 += lo;

            ((uint64_t*) &l2[idx2 & MASK])[0] = al2;
            ((uint64_t*) &l2[idx2 & MASK])[1] = ah2;

            ah2 ^= ch;
            al2 ^= cl;
            idx2 = al2;

            n  = ((int64_t*)&l2[idx2 & MASK])[0];
            d  = ((int32_t*)&l2[idx2 & MASK])[2];
            q = n / (d | 0x5);

            ((int64_t*)&l2[idx2 & MASK])[0] = n ^ q;
            idx2 = (~d) ^ q;
        }

        cn_implode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) l0, (__m128i*) h0);
        cn_implode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) l1, (__m128i*) h1);
        cn_implode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) l2, (__m128i*) h2);

        keccakf(h0, 24);
        keccakf(h1, 24);
        keccakf(h2, 24);

        extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
        extra_hashes[scratchPad[1]->state[0] & 3](scratchPad[1]->state, 200, output + 32);
        extra_hashes[scratchPad[2]->state[0] & 3](scratchPad[2]->state, 200, output + 64);
    }

    inline static void hashHeavyTube(const uint8_t* __restrict__ input,
                                     size_t size,
                                     uint8_t* __restrict__ output,
                                     ScratchPad** __restrict__ scratchPad)
    {
        keccak((const uint8_t*) input, (int) size, scratchPad[0]->state, 200);
        keccak((const uint8_t*) input + size, (int) size, scratchPad[1]->state, 200);
        keccak((const uint8_t*) input + 2 * size, (int) size, scratchPad[2]->state, 200);

        uint64_t tweak1_2_0 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35) ^
                               *(reinterpret_cast<const uint64_t*>(scratchPad[0]->state) + 24));
        uint64_t tweak1_2_1 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + size) ^
                               *(reinterpret_cast<const uint64_t*>(scratchPad[1]->state) + 24));
        uint64_t tweak1_2_2 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + 2 * size) ^
                               *(reinterpret_cast<const uint64_t*>(scratchPad[2]->state) + 24));

        const uint8_t* l0 = scratchPad[0]->memory;
        const uint8_t* l1 = scratchPad[1]->memory;
        const uint8_t* l2 = scratchPad[2]->memory;
        uint64_t* h0 = reinterpret_cast<uint64_t*>(scratchPad[0]->state);
        uint64_t* h1 = reinterpret_cast<uint64_t*>(scratchPad[1]->state);
        uint64_t* h2 = reinterpret_cast<uint64_t*>(scratchPad[2]->state);

        cn_explode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) h0, (__m128i*) l0);
        cn_explode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) h1, (__m128i*) l1);
        cn_explode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) h2, (__m128i*) l2);

        uint64_t al0 = h0[0] ^h0[4];
        uint64_t al1 = h1[0] ^h1[4];
        uint64_t al2 = h2[0] ^h2[4];
        uint64_t ah0 = h0[1] ^h0[5];
        uint64_t ah1 = h1[1] ^h1[5];
        uint64_t ah2 = h2[1] ^h2[5];

        __m128i bx0 = _mm_set_epi64x(h0[3] ^ h0[7], h0[2] ^ h0[6]);
        __m128i bx1 = _mm_set_epi64x(h1[3] ^ h1[7], h1[2] ^ h1[6]);
        __m128i bx2 = _mm_set_epi64x(h2[3] ^ h2[7], h2[2] ^ h2[6]);

        uint64_t idx0 = h0[0] ^h0[4];
        uint64_t idx1 = h1[0] ^h1[4];
        uint64_t idx2 = h2[0] ^h2[4];

        union alignas(16) {
            uint32_t k[4];
            uint64_t v64[2];
        };
        alignas(16) uint32_t x[4];

#define BYTE(p, i) ((unsigned char*)&p)[i]
        for (size_t i = 0; i < ITERATIONS; i++) {
            __m128i cx0 = _mm_load_si128((__m128i*) &l0[idx0 & MASK]);
            __m128i cx1 = _mm_load_si128((__m128i*) &l1[idx1 & MASK]);
            __m128i cx2 = _mm_load_si128((__m128i*) &l2[idx2 & MASK]);

            const __m128i& key0 = _mm_set_epi64x(ah0, al0);

            _mm_store_si128((__m128i*)k, key0);
            cx0 = _mm_xor_si128(cx0, _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128()));
            _mm_store_si128((__m128i*)x, cx0);

            k[0] ^= saes_table[0][BYTE(x[0], 0)] ^ saes_table[1][BYTE(x[1], 1)] ^ saes_table[2][BYTE(x[2], 2)] ^ saes_table[3][BYTE(x[3], 3)];
            x[0] ^= k[0];
            k[1] ^= saes_table[0][BYTE(x[1], 0)] ^ saes_table[1][BYTE(x[2], 1)] ^ saes_table[2][BYTE(x[3], 2)] ^ saes_table[3][BYTE(x[0], 3)];
            x[1] ^= k[1];
            k[2] ^= saes_table[0][BYTE(x[2], 0)] ^ saes_table[1][BYTE(x[3], 1)] ^ saes_table[2][BYTE(x[0], 2)] ^ saes_table[3][BYTE(x[1], 3)];
            x[2] ^= k[2];
            k[3] ^= saes_table[0][BYTE(x[3], 0)] ^ saes_table[1][BYTE(x[0], 1)] ^ saes_table[2][BYTE(x[1], 2)] ^ saes_table[3][BYTE(x[2], 3)];

            cx0 = _mm_load_si128((__m128i*)k);

            const __m128i& key1 = _mm_set_epi64x(ah1, al1);

            _mm_store_si128((__m128i*)k, key1);
            cx1 = _mm_xor_si128(cx1, _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128()));
            _mm_store_si128((__m128i*)x, cx1);

            k[0] ^= saes_table[0][BYTE(x[0], 0)] ^ saes_table[1][BYTE(x[1], 1)] ^ saes_table[2][BYTE(x[2], 2)] ^ saes_table[3][BYTE(x[3], 3)];
            x[0] ^= k[0];
            k[1] ^= saes_table[0][BYTE(x[1], 0)] ^ saes_table[1][BYTE(x[2], 1)] ^ saes_table[2][BYTE(x[3], 2)] ^ saes_table[3][BYTE(x[0], 3)];
            x[1] ^= k[1];
            k[2] ^= saes_table[0][BYTE(x[2], 0)] ^ saes_table[1][BYTE(x[3], 1)] ^ saes_table[2][BYTE(x[0], 2)] ^ saes_table[3][BYTE(x[1], 3)];
            x[2] ^= k[2];
            k[3] ^= saes_table[0][BYTE(x[3], 0)] ^ saes_table[1][BYTE(x[0], 1)] ^ saes_table[2][BYTE(x[1], 2)] ^ saes_table[3][BYTE(x[2], 3)];

            cx1 = _mm_load_si128((__m128i*)k);

            const __m128i& key2 = _mm_set_epi64x(ah2, al2);

            _mm_store_si128((__m128i*)k, key2);
            cx2 = _mm_xor_si128(cx2, _mm_cmpeq_epi32(_mm_setzero_si128(), _mm_setzero_si128()));
            _mm_store_si128((__m128i*)x, cx2);

            k[0] ^= saes_table[0][BYTE(x[0], 0)] ^ saes_table[1][BYTE(x[1], 1)] ^ saes_table[2][BYTE(x[2], 2)] ^ saes_table[3][BYTE(x[3], 3)];
            x[0] ^= k[0];
            k[1] ^= saes_table[0][BYTE(x[1], 0)] ^ saes_table[1][BYTE(x[2], 1)] ^ saes_table[2][BYTE(x[3], 2)] ^ saes_table[3][BYTE(x[0], 3)];
            x[1] ^= k[1];
            k[2] ^= saes_table[0][BYTE(x[2], 0)] ^ saes_table[1][BYTE(x[3], 1)] ^ saes_table[2][BYTE(x[0], 2)] ^ saes_table[3][BYTE(x[1], 3)];
            x[2] ^= k[2];
            k[3] ^= saes_table[0][BYTE(x[3], 0)] ^ saes_table[1][BYTE(x[0], 1)] ^ saes_table[2][BYTE(x[1], 2)] ^ saes_table[3][BYTE(x[2], 3)];

            cx2 = _mm_load_si128((__m128i*)k);

            _mm_store_si128((__m128i*) &l0[idx0 & MASK], _mm_xor_si128(bx0, cx0));
            _mm_store_si128((__m128i*) &l1[idx1 & MASK], _mm_xor_si128(bx1, cx1));
            _mm_store_si128((__m128i*) &l2[idx2 & MASK], _mm_xor_si128(bx2, cx2));

            static const uint32_t table = 0x75310;
            uint8_t tmp = reinterpret_cast<const uint8_t*>(&l0[idx0 & MASK])[11];
            uint8_t index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
            ((uint8_t*)(&l0[idx0 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);
            tmp = reinterpret_cast<const uint8_t*>(&l1[idx1 & MASK])[11];
            index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
            ((uint8_t*)(&l1[idx1 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);
            tmp = reinterpret_cast<const uint8_t*>(&l2[idx2 & MASK])[11];
            index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
            ((uint8_t*)(&l2[idx2 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);

            idx0 = EXTRACT64(cx0);
            idx1 = EXTRACT64(cx1);
            idx2 = EXTRACT64(cx2);

            bx0 = cx0;
            bx1 = cx1;
            bx2 = cx2;

            uint64_t hi, lo, cl, ch;
            cl = ((uint64_t*) &l0[idx0 & MASK])[0];
            ch = ((uint64_t*) &l0[idx0 & MASK])[1];
            lo = __umul128(idx0, cl, &hi);

            al0 += hi;
            ah0 += lo;

            ah0 ^= tweak1_2_0;
            ((uint64_t*) &l0[idx0 & MASK])[0] = al0;
            ((uint64_t*) &l0[idx0 & MASK])[1] = ah0;
            ah0 ^= tweak1_2_0;

            ((uint64_t*)&l0[idx0 & MASK])[1] ^= ((uint64_t*)&l0[idx0 & MASK])[0];

            ah0 ^= ch;
            al0 ^= cl;
            idx0 = al0;

            int64_t n  = ((int64_t*)&l0[idx0 & MASK])[0];
            int32_t d  = ((int32_t*)&l0[idx0 & MASK])[2];
            int64_t q = n / (d | 0x5);

            ((int64_t*)&l0[idx0 & MASK])[0] = n ^ q;
            idx0 = d ^ q;


            cl = ((uint64_t*) &l1[idx1 & MASK])[0];
            ch = ((uint64_t*) &l1[idx1 & MASK])[1];
            lo = __umul128(idx1, cl, &hi);

            al1 += hi;
            ah1 += lo;

            ah1 ^= tweak1_2_1;
            ((uint64_t*) &l1[idx1 & MASK])[0] = al1;
            ((uint64_t*) &l1[idx1 & MASK])[1] = ah1;
            ah1 ^= tweak1_2_1;

            ((uint64_t*)&l1[idx1 & MASK])[1] ^= ((uint64_t*)&l1[idx1 & MASK])[0];

            ah1 ^= ch;
            al1 ^= cl;
            idx1 = al1;

            n  = ((int64_t*)&l1[idx1 & MASK])[0];
            d  = ((int32_t*)&l1[idx1 & MASK])[2];
            q = n / (d | 0x5);

            ((int64_t*)&l1[idx1 & MASK])[0] = n ^ q;
            idx1 = d ^ q;


            cl = ((uint64_t*) &l2[idx2 & MASK])[0];
            ch = ((uint64_t*) &l2[idx2 & MASK])[1];
            lo = __umul128(idx2, cl, &hi);

            al2 += hi;
            ah2 += lo;

            ah2 ^= tweak1_2_2;
            ((uint64_t*) &l2[idx2 & MASK])[0] = al2;
            ((uint64_t*) &l2[idx2 & MASK])[1] = ah2;
            ah2 ^= tweak1_2_2;

            ((uint64_t*)&l2[idx2 & MASK])[1] ^= ((uint64_t*)&l2[idx2 & MASK])[0];

            ah2 ^= ch;
            al2 ^= cl;
            idx2 = al2;

            n  = ((int64_t*)&l2[idx2 & MASK])[0];
            d  = ((int32_t*)&l2[idx2 & MASK])[2];
            q = n / (d | 0x5);

            ((int64_t*)&l2[idx2 & MASK])[0] = n ^ q;
            idx2 = d ^ q;
        }
#undef BYTE

        cn_implode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) l0, (__m128i*) h0);
        cn_implode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) l1, (__m128i*) h1);
        cn_implode_scratchpad_heavy<MEM, SOFT_AES>((__m128i*) l2, (__m128i*) h2);

        keccakf(h0, 24);
        keccakf(h1, 24);
        keccakf(h2, 24);

        extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
        extra_hashes[scratchPad[1]->state[0] & 3](scratchPad[1]->state, 200, output + 32);
        extra_hashes[scratchPad[2]->state[0] & 3](scratchPad[2]->state, 200, output + 64);
    }
};

template<size_t ITERATIONS, size_t INDEX_SHIFT, size_t MEM, size_t MASK, bool SOFT_AES>
class CryptoNightMultiHash<ITERATIONS, INDEX_SHIFT, MEM, MASK, SOFT_AES, 4>
{
public:
    inline static void hash(const uint8_t* __restrict__ input,
                            size_t size,
                            uint8_t* __restrict__ output,
                            ScratchPad** __restrict__ scratchPad)
    {
        keccak((const uint8_t*) input, (int) size, scratchPad[0]->state, 200);
        keccak((const uint8_t*) input + size, (int) size, scratchPad[1]->state, 200);
        keccak((const uint8_t*) input + 2 * size, (int) size, scratchPad[2]->state, 200);
        keccak((const uint8_t*) input + 3 * size, (int) size, scratchPad[3]->state, 200);

        const uint8_t* l0 = scratchPad[0]->memory;
        const uint8_t* l1 = scratchPad[1]->memory;
        const uint8_t* l2 = scratchPad[2]->memory;
        const uint8_t* l3 = scratchPad[3]->memory;
        uint64_t* h0 = reinterpret_cast<uint64_t*>(scratchPad[0]->state);
        uint64_t* h1 = reinterpret_cast<uint64_t*>(scratchPad[1]->state);
        uint64_t* h2 = reinterpret_cast<uint64_t*>(scratchPad[2]->state);
        uint64_t* h3 = reinterpret_cast<uint64_t*>(scratchPad[3]->state);

        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h0, (__m128i*) l0);
        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h1, (__m128i*) l1);
        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h2, (__m128i*) l2);
        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h3, (__m128i*) l3);

        uint64_t al0 = h0[0] ^h0[4];
        uint64_t al1 = h1[0] ^h1[4];
        uint64_t al2 = h2[0] ^h2[4];
        uint64_t al3 = h3[0] ^h3[4];
        uint64_t ah0 = h0[1] ^h0[5];
        uint64_t ah1 = h1[1] ^h1[5];
        uint64_t ah2 = h2[1] ^h2[5];
        uint64_t ah3 = h3[1] ^h3[5];

        __m128i bx0 = _mm_set_epi64x(h0[3] ^ h0[7], h0[2] ^ h0[6]);
        __m128i bx1 = _mm_set_epi64x(h1[3] ^ h1[7], h1[2] ^ h1[6]);
        __m128i bx2 = _mm_set_epi64x(h2[3] ^ h2[7], h2[2] ^ h2[6]);
        __m128i bx3 = _mm_set_epi64x(h3[3] ^ h3[7], h3[2] ^ h3[6]);

        uint64_t idx0 = h0[0] ^h0[4];
        uint64_t idx1 = h1[0] ^h1[4];
        uint64_t idx2 = h2[0] ^h2[4];
        uint64_t idx3 = h3[0] ^h3[4];

        for (size_t i = 0; i < ITERATIONS; i++) {
            __m128i cx0;
            __m128i cx1;
            __m128i cx2;
            __m128i cx3;

            if (SOFT_AES) {
                cx0 = soft_aesenc((uint32_t*)&l0[idx0 & MASK], _mm_set_epi64x(ah0, al0));
                cx1 = soft_aesenc((uint32_t*)&l1[idx1 & MASK], _mm_set_epi64x(ah1, al1));
                cx2 = soft_aesenc((uint32_t*)&l2[idx2 & MASK], _mm_set_epi64x(ah2, al2));
                cx3 = soft_aesenc((uint32_t*)&l3[idx3 & MASK], _mm_set_epi64x(ah3, al3));
            } else {
                cx0 = _mm_load_si128((__m128i*) &l0[idx0 & MASK]);
                cx1 = _mm_load_si128((__m128i*) &l1[idx1 & MASK]);
                cx2 = _mm_load_si128((__m128i*) &l2[idx2 & MASK]);
                cx3 = _mm_load_si128((__m128i*) &l3[idx3 & MASK]);

                cx0 = _mm_aesenc_si128(cx0, _mm_set_epi64x(ah0, al0));
                cx1 = _mm_aesenc_si128(cx1, _mm_set_epi64x(ah1, al1));
                cx2 = _mm_aesenc_si128(cx2, _mm_set_epi64x(ah2, al2));
                cx3 = _mm_aesenc_si128(cx3, _mm_set_epi64x(ah3, al3));
            }

            _mm_store_si128((__m128i*) &l0[idx0 & MASK], _mm_xor_si128(bx0, cx0));
            _mm_store_si128((__m128i*) &l1[idx1 & MASK], _mm_xor_si128(bx1, cx1));
            _mm_store_si128((__m128i*) &l2[idx2 & MASK], _mm_xor_si128(bx2, cx2));
            _mm_store_si128((__m128i*) &l3[idx3 & MASK], _mm_xor_si128(bx3, cx3));

            idx0 = EXTRACT64(cx0);
            idx1 = EXTRACT64(cx1);
            idx2 = EXTRACT64(cx2);
            idx3 = EXTRACT64(cx3);

            bx0 = cx0;
            bx1 = cx1;
            bx2 = cx2;
            bx3 = cx3;


            uint64_t hi, lo, cl, ch;
            cl = ((uint64_t*) &l0[idx0 & MASK])[0];
            ch = ((uint64_t*) &l0[idx0 & MASK])[1];
            lo = __umul128(idx0, cl, &hi);

            al0 += hi;
            ah0 += lo;

            ((uint64_t*) &l0[idx0 & MASK])[0] = al0;
            ((uint64_t*) &l0[idx0 & MASK])[1] = ah0;

            ah0 ^= ch;
            al0 ^= cl;
            idx0 = al0;


            cl = ((uint64_t*) &l1[idx1 & MASK])[0];
            ch = ((uint64_t*) &l1[idx1 & MASK])[1];
            lo = __umul128(idx1, cl, &hi);

            al1 += hi;
            ah1 += lo;

            ((uint64_t*) &l1[idx1 & MASK])[0] = al1;
            ((uint64_t*) &l1[idx1 & MASK])[1] = ah1;

            ah1 ^= ch;
            al1 ^= cl;
            idx1 = al1;


            cl = ((uint64_t*) &l2[idx2 & MASK])[0];
            ch = ((uint64_t*) &l2[idx2 & MASK])[1];
            lo = __umul128(idx2, cl, &hi);

            al2 += hi;
            ah2 += lo;

            ((uint64_t*) &l2[idx2 & MASK])[0] = al2;
            ((uint64_t*) &l2[idx2 & MASK])[1] = ah2;

            ah2 ^= ch;
            al2 ^= cl;
            idx2 = al2;


            cl = ((uint64_t*) &l3[idx3 & MASK])[0];
            ch = ((uint64_t*) &l3[idx3 & MASK])[1];
            lo = __umul128(idx3, cl, &hi);

            al3 += hi;
            ah3 += lo;

            ((uint64_t*) &l3[idx3 & MASK])[0] = al3;
            ((uint64_t*) &l3[idx3 & MASK])[1] = ah3;

            ah3 ^= ch;
            al3 ^= cl;
            idx3 = al3;
        }

        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l0, (__m128i*) h0);
        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l1, (__m128i*) h1);
        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l2, (__m128i*) h2);
        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l3, (__m128i*) h3);

        keccakf(h0, 24);
        keccakf(h1, 24);
        keccakf(h2, 24);
        keccakf(h3, 24);

        extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
        extra_hashes[scratchPad[1]->state[0] & 3](scratchPad[1]->state, 200, output + 32);
        extra_hashes[scratchPad[2]->state[0] & 3](scratchPad[2]->state, 200, output + 64);
        extra_hashes[scratchPad[3]->state[0] & 3](scratchPad[3]->state, 200, output + 96);
    }

  inline static void hashPowV2(const uint8_t* __restrict__ input,
                            size_t size,
                            uint8_t* __restrict__ output,
                            ScratchPad** __restrict__ scratchPad)
  {
      keccak((const uint8_t*) input, (int) size, scratchPad[0]->state, 200);
      keccak((const uint8_t*) input + size, (int) size, scratchPad[1]->state, 200);
      keccak((const uint8_t*) input + 2 * size, (int) size, scratchPad[2]->state, 200);
      keccak((const uint8_t*) input + 3 * size, (int) size, scratchPad[3]->state, 200);

      uint64_t tweak1_2_0 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35) ^
                             *(reinterpret_cast<const uint64_t*>(scratchPad[0]->state) + 24));
      uint64_t tweak1_2_1 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + size) ^
                             *(reinterpret_cast<const uint64_t*>(scratchPad[1]->state) + 24));
      uint64_t tweak1_2_2 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + 2 * size) ^
                             *(reinterpret_cast<const uint64_t*>(scratchPad[2]->state) + 24));
      uint64_t tweak1_2_3 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + 3 * size) ^
                             *(reinterpret_cast<const uint64_t*>(scratchPad[3]->state) + 24));

      const uint8_t* l0 = scratchPad[0]->memory;
      const uint8_t* l1 = scratchPad[1]->memory;
      const uint8_t* l2 = scratchPad[2]->memory;
      const uint8_t* l3 = scratchPad[3]->memory;
      uint64_t* h0 = reinterpret_cast<uint64_t*>(scratchPad[0]->state);
      uint64_t* h1 = reinterpret_cast<uint64_t*>(scratchPad[1]->state);
      uint64_t* h2 = reinterpret_cast<uint64_t*>(scratchPad[2]->state);
      uint64_t* h3 = reinterpret_cast<uint64_t*>(scratchPad[3]->state);

      cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h0, (__m128i*) l0);
      cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h1, (__m128i*) l1);
      cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h2, (__m128i*) l2);
      cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h3, (__m128i*) l3);

      uint64_t al0 = h0[0] ^h0[4];
      uint64_t al1 = h1[0] ^h1[4];
      uint64_t al2 = h2[0] ^h2[4];
      uint64_t al3 = h3[0] ^h3[4];
      uint64_t ah0 = h0[1] ^h0[5];
      uint64_t ah1 = h1[1] ^h1[5];
      uint64_t ah2 = h2[1] ^h2[5];
      uint64_t ah3 = h3[1] ^h3[5];

      __m128i bx0 = _mm_set_epi64x(h0[3] ^ h0[7], h0[2] ^ h0[6]);
      __m128i bx1 = _mm_set_epi64x(h1[3] ^ h1[7], h1[2] ^ h1[6]);
      __m128i bx2 = _mm_set_epi64x(h2[3] ^ h2[7], h2[2] ^ h2[6]);
      __m128i bx3 = _mm_set_epi64x(h3[3] ^ h3[7], h3[2] ^ h3[6]);

      uint64_t idx0 = h0[0] ^h0[4];
      uint64_t idx1 = h1[0] ^h1[4];
      uint64_t idx2 = h2[0] ^h2[4];
      uint64_t idx3 = h3[0] ^h3[4];

      for (size_t i = 0; i < ITERATIONS; i++) {
          __m128i cx0;
          __m128i cx1;
          __m128i cx2;
          __m128i cx3;

          if (SOFT_AES) {
              cx0 = soft_aesenc((uint32_t*)&l0[idx0 & MASK], _mm_set_epi64x(ah0, al0));
              cx1 = soft_aesenc((uint32_t*)&l1[idx1 & MASK], _mm_set_epi64x(ah1, al1));
              cx2 = soft_aesenc((uint32_t*)&l2[idx2 & MASK], _mm_set_epi64x(ah2, al2));
              cx3 = soft_aesenc((uint32_t*)&l3[idx3 & MASK], _mm_set_epi64x(ah3, al3));
          } else {
              cx0 = _mm_load_si128((__m128i*) &l0[idx0 & MASK]);
              cx1 = _mm_load_si128((__m128i*) &l1[idx1 & MASK]);
              cx2 = _mm_load_si128((__m128i*) &l2[idx2 & MASK]);
              cx3 = _mm_load_si128((__m128i*) &l3[idx3 & MASK]);

              cx0 = _mm_aesenc_si128(cx0, _mm_set_epi64x(ah0, al0));
              cx1 = _mm_aesenc_si128(cx1, _mm_set_epi64x(ah1, al1));
              cx2 = _mm_aesenc_si128(cx2, _mm_set_epi64x(ah2, al2));
              cx3 = _mm_aesenc_si128(cx3, _mm_set_epi64x(ah3, al3));
          }

          _mm_store_si128((__m128i*) &l0[idx0 & MASK], _mm_xor_si128(bx0, cx0));
          _mm_store_si128((__m128i*) &l1[idx1 & MASK], _mm_xor_si128(bx1, cx1));
          _mm_store_si128((__m128i*) &l2[idx2 & MASK], _mm_xor_si128(bx2, cx2));
          _mm_store_si128((__m128i*) &l3[idx3 & MASK], _mm_xor_si128(bx3, cx3));

          static const uint32_t table = 0x75310;
          uint8_t tmp = reinterpret_cast<const uint8_t*>(&l0[idx0 & MASK])[11];
          uint8_t index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
          ((uint8_t*)(&l0[idx0 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);
          tmp = reinterpret_cast<const uint8_t*>(&l1[idx1 & MASK])[11];
          index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
          ((uint8_t*)(&l1[idx1 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);
          tmp = reinterpret_cast<const uint8_t*>(&l2[idx2 & MASK])[11];
          index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
          ((uint8_t*)(&l2[idx2 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);
          tmp = reinterpret_cast<const uint8_t*>(&l3[idx3 & MASK])[11];
          index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
          ((uint8_t*)(&l3[idx3 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);

          idx0 = EXTRACT64(cx0);
          idx1 = EXTRACT64(cx1);
          idx2 = EXTRACT64(cx2);
          idx3 = EXTRACT64(cx3);

          bx0 = cx0;
          bx1 = cx1;
          bx2 = cx2;
          bx3 = cx3;


          uint64_t hi, lo, cl, ch;
          cl = ((uint64_t*) &l0[idx0 & MASK])[0];
          ch = ((uint64_t*) &l0[idx0 & MASK])[1];
          lo = __umul128(idx0, cl, &hi);

          al0 += hi;
          ah0 += lo;

          ah0 ^= tweak1_2_0;
          ((uint64_t*) &l0[idx0 & MASK])[0] = al0;
          ((uint64_t*) &l0[idx0 & MASK])[1] = ah0;
          ah0 ^= tweak1_2_0;

          ah0 ^= ch;
          al0 ^= cl;
          idx0 = al0;


          cl = ((uint64_t*) &l1[idx1 & MASK])[0];
          ch = ((uint64_t*) &l1[idx1 & MASK])[1];
          lo = __umul128(idx1, cl, &hi);

          al1 += hi;
          ah1 += lo;

          ah1 ^= tweak1_2_1;
          ((uint64_t*) &l1[idx1 & MASK])[0] = al1;
          ((uint64_t*) &l1[idx1 & MASK])[1] = ah1;
          ah1 ^= tweak1_2_1;

          ah1 ^= ch;
          al1 ^= cl;
          idx1 = al1;


          cl = ((uint64_t*) &l2[idx2 & MASK])[0];
          ch = ((uint64_t*) &l2[idx2 & MASK])[1];
          lo = __umul128(idx2, cl, &hi);

          al2 += hi;
          ah2 += lo;

          ah2 ^= tweak1_2_2;
          ((uint64_t*) &l2[idx2 & MASK])[0] = al2;
          ((uint64_t*) &l2[idx2 & MASK])[1] = ah2;
          ah2 ^= tweak1_2_2;

          ah2 ^= ch;
          al2 ^= cl;
          idx2 = al2;


          cl = ((uint64_t*) &l3[idx3 & MASK])[0];
          ch = ((uint64_t*) &l3[idx3 & MASK])[1];
          lo = __umul128(idx3, cl, &hi);

          al3 += hi;
          ah3 += lo;

          ah3 ^= tweak1_2_3;
          ((uint64_t*) &l3[idx3 & MASK])[0] = al3;
          ((uint64_t*) &l3[idx3 & MASK])[1] = ah3;
          ah3 ^= tweak1_2_3;

          ah3 ^= ch;
          al3 ^= cl;
          idx3 = al3;
      }

      cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l0, (__m128i*) h0);
      cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l1, (__m128i*) h1);
      cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l2, (__m128i*) h2);
      cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l3, (__m128i*) h3);

      keccakf(h0, 24);
      keccakf(h1, 24);
      keccakf(h2, 24);
      keccakf(h3, 24);

      extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
      extra_hashes[scratchPad[1]->state[0] & 3](scratchPad[1]->state, 200, output + 32);
      extra_hashes[scratchPad[2]->state[0] & 3](scratchPad[2]->state, 200, output + 64);
      extra_hashes[scratchPad[3]->state[0] & 3](scratchPad[3]->state, 200, output + 96);
  }

    inline static void hashLiteTube(const uint8_t* __restrict__ input,
                                 size_t size,
                                 uint8_t* __restrict__ output,
                                 ScratchPad** __restrict__ scratchPad)
    {
        keccak((const uint8_t*) input, (int) size, scratchPad[0]->state, 200);
        keccak((const uint8_t*) input + size, (int) size, scratchPad[1]->state, 200);
        keccak((const uint8_t*) input + 2 * size, (int) size, scratchPad[2]->state, 200);
        keccak((const uint8_t*) input + 3 * size, (int) size, scratchPad[3]->state, 200);

        uint64_t tweak1_2_0 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35) ^
                               *(reinterpret_cast<const uint64_t*>(scratchPad[0]->state) + 24));
        uint64_t tweak1_2_1 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + size) ^
                               *(reinterpret_cast<const uint64_t*>(scratchPad[1]->state) + 24));
        uint64_t tweak1_2_2 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + 2 * size) ^
                               *(reinterpret_cast<const uint64_t*>(scratchPad[2]->state) + 24));
        uint64_t tweak1_2_3 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + 3 * size) ^
                               *(reinterpret_cast<const uint64_t*>(scratchPad[3]->state) + 24));

        const uint8_t* l0 = scratchPad[0]->memory;
        const uint8_t* l1 = scratchPad[1]->memory;
        const uint8_t* l2 = scratchPad[2]->memory;
        const uint8_t* l3 = scratchPad[3]->memory;
        uint64_t* h0 = reinterpret_cast<uint64_t*>(scratchPad[0]->state);
        uint64_t* h1 = reinterpret_cast<uint64_t*>(scratchPad[1]->state);
        uint64_t* h2 = reinterpret_cast<uint64_t*>(scratchPad[2]->state);
        uint64_t* h3 = reinterpret_cast<uint64_t*>(scratchPad[3]->state);

        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h0, (__m128i*) l0);
        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h1, (__m128i*) l1);
        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h2, (__m128i*) l2);
        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h3, (__m128i*) l3);

        uint64_t al0 = h0[0] ^h0[4];
        uint64_t al1 = h1[0] ^h1[4];
        uint64_t al2 = h2[0] ^h2[4];
        uint64_t al3 = h3[0] ^h3[4];
        uint64_t ah0 = h0[1] ^h0[5];
        uint64_t ah1 = h1[1] ^h1[5];
        uint64_t ah2 = h2[1] ^h2[5];
        uint64_t ah3 = h3[1] ^h3[5];

        __m128i bx0 = _mm_set_epi64x(h0[3] ^ h0[7], h0[2] ^ h0[6]);
        __m128i bx1 = _mm_set_epi64x(h1[3] ^ h1[7], h1[2] ^ h1[6]);
        __m128i bx2 = _mm_set_epi64x(h2[3] ^ h2[7], h2[2] ^ h2[6]);
        __m128i bx3 = _mm_set_epi64x(h3[3] ^ h3[7], h3[2] ^ h3[6]);

        uint64_t idx0 = h0[0] ^h0[4];
        uint64_t idx1 = h1[0] ^h1[4];
        uint64_t idx2 = h2[0] ^h2[4];
        uint64_t idx3 = h3[0] ^h3[4];

        for (size_t i = 0; i < ITERATIONS; i++) {
            __m128i cx0;
            __m128i cx1;
            __m128i cx2;
            __m128i cx3;

            if (SOFT_AES) {
                cx0 = soft_aesenc((uint32_t*)&l0[idx0 & MASK], _mm_set_epi64x(ah0, al0));
                cx1 = soft_aesenc((uint32_t*)&l1[idx1 & MASK], _mm_set_epi64x(ah1, al1));
                cx2 = soft_aesenc((uint32_t*)&l2[idx2 & MASK], _mm_set_epi64x(ah2, al2));
                cx3 = soft_aesenc((uint32_t*)&l3[idx3 & MASK], _mm_set_epi64x(ah3, al3));
            } else {
                cx0 = _mm_load_si128((__m128i*) &l0[idx0 & MASK]);
                cx1 = _mm_load_si128((__m128i*) &l1[idx1 & MASK]);
                cx2 = _mm_load_si128((__m128i*) &l2[idx2 & MASK]);
                cx3 = _mm_load_si128((__m128i*) &l3[idx3 & MASK]);

                cx0 = _mm_aesenc_si128(cx0, _mm_set_epi64x(ah0, al0));
                cx1 = _mm_aesenc_si128(cx1, _mm_set_epi64x(ah1, al1));
                cx2 = _mm_aesenc_si128(cx2, _mm_set_epi64x(ah2, al2));
                cx3 = _mm_aesenc_si128(cx3, _mm_set_epi64x(ah3, al3));
            }

            _mm_store_si128((__m128i*) &l0[idx0 & MASK], _mm_xor_si128(bx0, cx0));
            _mm_store_si128((__m128i*) &l1[idx1 & MASK], _mm_xor_si128(bx1, cx1));
            _mm_store_si128((__m128i*) &l2[idx2 & MASK], _mm_xor_si128(bx2, cx2));
            _mm_store_si128((__m128i*) &l3[idx3 & MASK], _mm_xor_si128(bx3, cx3));

            static const uint32_t table = 0x75310;
            uint8_t tmp = reinterpret_cast<const uint8_t*>(&l0[idx0 & MASK])[11];
            uint8_t index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
            ((uint8_t*)(&l0[idx0 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);
            tmp = reinterpret_cast<const uint8_t*>(&l1[idx1 & MASK])[11];
            index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
            ((uint8_t*)(&l1[idx1 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);
            tmp = reinterpret_cast<const uint8_t*>(&l2[idx2 & MASK])[11];
            index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
            ((uint8_t*)(&l2[idx2 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);
            tmp = reinterpret_cast<const uint8_t*>(&l3[idx3 & MASK])[11];
            index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
            ((uint8_t*)(&l3[idx3 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);

            idx0 = EXTRACT64(cx0);
            idx1 = EXTRACT64(cx1);
            idx2 = EXTRACT64(cx2);
            idx3 = EXTRACT64(cx3);

            bx0 = cx0;
            bx1 = cx1;
            bx2 = cx2;
            bx3 = cx3;


            uint64_t hi, lo, cl, ch;
            cl = ((uint64_t*) &l0[idx0 & MASK])[0];
            ch = ((uint64_t*) &l0[idx0 & MASK])[1];
            lo = __umul128(idx0, cl, &hi);

            al0 += hi;
            ah0 += lo;

            ah0 ^= tweak1_2_0;
            ((uint64_t*) &l0[idx0 & MASK])[0] = al0;
            ((uint64_t*) &l0[idx0 & MASK])[1] = ah0;
            ah0 ^= tweak1_2_0;

            ((uint64_t*)&l0[idx0 & MASK])[1] ^= ((uint64_t*)&l0[idx0 & MASK])[0];

            ah0 ^= ch;
            al0 ^= cl;
            idx0 = al0;


            cl = ((uint64_t*) &l1[idx1 & MASK])[0];
            ch = ((uint64_t*) &l1[idx1 & MASK])[1];
            lo = __umul128(idx1, cl, &hi);

            al1 += hi;
            ah1 += lo;

            ah1 ^= tweak1_2_1;
            ((uint64_t*) &l1[idx1 & MASK])[0] = al1;
            ((uint64_t*) &l1[idx1 & MASK])[1] = ah1;
            ah1 ^= tweak1_2_1;

            ((uint64_t*)&l1[idx1 & MASK])[1] ^= ((uint64_t*)&l1[idx1 & MASK])[0];

            ah1 ^= ch;
            al1 ^= cl;
            idx1 = al1;


            cl = ((uint64_t*) &l2[idx2 & MASK])[0];
            ch = ((uint64_t*) &l2[idx2 & MASK])[1];
            lo = __umul128(idx2, cl, &hi);

            al2 += hi;
            ah2 += lo;

            ah2 ^= tweak1_2_2;
            ((uint64_t*) &l2[idx2 & MASK])[0] = al2;
            ((uint64_t*) &l2[idx2 & MASK])[1] = ah2;
            ah2 ^= tweak1_2_2;

            ((uint64_t*)&l2[idx2 & MASK])[1] ^= ((uint64_t*)&l2[idx2 & MASK])[0];

            ah2 ^= ch;
            al2 ^= cl;
            idx2 = al2;


            cl = ((uint64_t*) &l3[idx3 & MASK])[0];
            ch = ((uint64_t*) &l3[idx3 & MASK])[1];
            lo = __umul128(idx3, cl, &hi);

            al3 += hi;
            ah3 += lo;

            ah3 ^= tweak1_2_3;
            ((uint64_t*) &l3[idx3 & MASK])[0] = al3;
            ((uint64_t*) &l3[idx3 & MASK])[1] = ah3;
            ah3 ^= tweak1_2_3;

            ((uint64_t*)&l3[idx3 & MASK])[1] ^= ((uint64_t*)&l3[idx3 & MASK])[0];

            ah3 ^= ch;
            al3 ^= cl;
            idx3 = al3;
        }

        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l0, (__m128i*) h0);
        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l1, (__m128i*) h1);
        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l2, (__m128i*) h2);
        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l3, (__m128i*) h3);

        keccakf(h0, 24);
        keccakf(h1, 24);
        keccakf(h2, 24);
        keccakf(h3, 24);

        extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
        extra_hashes[scratchPad[1]->state[0] & 3](scratchPad[1]->state, 200, output + 32);
        extra_hashes[scratchPad[2]->state[0] & 3](scratchPad[2]->state, 200, output + 64);
        extra_hashes[scratchPad[3]->state[0] & 3](scratchPad[3]->state, 200, output + 96);
    }

    inline static void hashHeavy(const uint8_t* __restrict__ input,
                                 size_t size,
                                 uint8_t* __restrict__ output,
                                 ScratchPad** __restrict__ scratchPad)
    {
        // not supported
    }

    inline static void hashHeavyHaven(const uint8_t* __restrict__ input,
                                      size_t size,
                                      uint8_t* __restrict__ output,
                                      ScratchPad** __restrict__ scratchPad)
    {
        // not supported
    }

    inline static void hashHeavyTube(const uint8_t* __restrict__ input,
                                     size_t size,
                                     uint8_t* __restrict__ output,
                                     ScratchPad** __restrict__ scratchPad)
    {
        // not supported
    }
};

template<size_t ITERATIONS, size_t INDEX_SHIFT, size_t MEM, size_t MASK, bool SOFT_AES>
class CryptoNightMultiHash<ITERATIONS, INDEX_SHIFT, MEM, MASK, SOFT_AES, 5>
{
public:
    inline static void hash(const uint8_t* __restrict__ input,
                            size_t size,
                            uint8_t* __restrict__ output,
                            ScratchPad** __restrict__ scratchPad)
    {
        keccak((const uint8_t*) input, (int) size, scratchPad[0]->state, 200);
        keccak((const uint8_t*) input + size, (int) size, scratchPad[1]->state, 200);
        keccak((const uint8_t*) input + 2 * size, (int) size, scratchPad[2]->state, 200);
        keccak((const uint8_t*) input + 3 * size, (int) size, scratchPad[3]->state, 200);
        keccak((const uint8_t*) input + 4 * size, (int) size, scratchPad[4]->state, 200);

        const uint8_t* l0 = scratchPad[0]->memory;
        const uint8_t* l1 = scratchPad[1]->memory;
        const uint8_t* l2 = scratchPad[2]->memory;
        const uint8_t* l3 = scratchPad[3]->memory;
        const uint8_t* l4 = scratchPad[4]->memory;
        uint64_t* h0 = reinterpret_cast<uint64_t*>(scratchPad[0]->state);
        uint64_t* h1 = reinterpret_cast<uint64_t*>(scratchPad[1]->state);
        uint64_t* h2 = reinterpret_cast<uint64_t*>(scratchPad[2]->state);
        uint64_t* h3 = reinterpret_cast<uint64_t*>(scratchPad[3]->state);
        uint64_t* h4 = reinterpret_cast<uint64_t*>(scratchPad[4]->state);

        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h0, (__m128i*) l0);
        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h1, (__m128i*) l1);
        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h2, (__m128i*) l2);
        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h3, (__m128i*) l3);
        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h4, (__m128i*) l4);

        uint64_t al0 = h0[0] ^h0[4];
        uint64_t al1 = h1[0] ^h1[4];
        uint64_t al2 = h2[0] ^h2[4];
        uint64_t al3 = h3[0] ^h3[4];
        uint64_t al4 = h4[0] ^h4[4];
        uint64_t ah0 = h0[1] ^h0[5];
        uint64_t ah1 = h1[1] ^h1[5];
        uint64_t ah2 = h2[1] ^h2[5];
        uint64_t ah3 = h3[1] ^h3[5];
        uint64_t ah4 = h4[1] ^h4[5];

        __m128i bx0 = _mm_set_epi64x(h0[3] ^ h0[7], h0[2] ^ h0[6]);
        __m128i bx1 = _mm_set_epi64x(h1[3] ^ h1[7], h1[2] ^ h1[6]);
        __m128i bx2 = _mm_set_epi64x(h2[3] ^ h2[7], h2[2] ^ h2[6]);
        __m128i bx3 = _mm_set_epi64x(h3[3] ^ h3[7], h3[2] ^ h3[6]);
        __m128i bx4 = _mm_set_epi64x(h4[3] ^ h4[7], h4[2] ^ h4[6]);

        uint64_t idx0 = h0[0] ^h0[4];
        uint64_t idx1 = h1[0] ^h1[4];
        uint64_t idx2 = h2[0] ^h2[4];
        uint64_t idx3 = h3[0] ^h3[4];
        uint64_t idx4 = h4[0] ^h4[4];

        for (size_t i = 0; i < ITERATIONS; i++) {
            __m128i cx0;
            __m128i cx1;
            __m128i cx2;
            __m128i cx3;
            __m128i cx4;

            if (SOFT_AES) {
                cx0 = soft_aesenc((uint32_t*)&l0[idx0 & MASK], _mm_set_epi64x(ah0, al0));
                cx1 = soft_aesenc((uint32_t*)&l1[idx1 & MASK], _mm_set_epi64x(ah1, al1));
                cx2 = soft_aesenc((uint32_t*)&l2[idx2 & MASK], _mm_set_epi64x(ah2, al2));
                cx3 = soft_aesenc((uint32_t*)&l3[idx3 & MASK], _mm_set_epi64x(ah3, al3));
                cx4 = soft_aesenc((uint32_t*)&l4[idx4 & MASK], _mm_set_epi64x(ah4, al4));
            } else {
                cx0 = _mm_load_si128((__m128i*) &l0[idx0 & MASK]);
                cx1 = _mm_load_si128((__m128i*) &l1[idx1 & MASK]);
                cx2 = _mm_load_si128((__m128i*) &l2[idx2 & MASK]);
                cx3 = _mm_load_si128((__m128i*) &l3[idx3 & MASK]);
                cx4 = _mm_load_si128((__m128i*) &l4[idx4 & MASK]);

                cx0 = _mm_aesenc_si128(cx0, _mm_set_epi64x(ah0, al0));
                cx1 = _mm_aesenc_si128(cx1, _mm_set_epi64x(ah1, al1));
                cx2 = _mm_aesenc_si128(cx2, _mm_set_epi64x(ah2, al2));
                cx3 = _mm_aesenc_si128(cx3, _mm_set_epi64x(ah3, al3));
                cx4 = _mm_aesenc_si128(cx4, _mm_set_epi64x(ah4, al4));
            }

            _mm_store_si128((__m128i*) &l0[idx0 & MASK], _mm_xor_si128(bx0, cx0));
            _mm_store_si128((__m128i*) &l1[idx1 & MASK], _mm_xor_si128(bx1, cx1));
            _mm_store_si128((__m128i*) &l2[idx2 & MASK], _mm_xor_si128(bx2, cx2));
            _mm_store_si128((__m128i*) &l3[idx3 & MASK], _mm_xor_si128(bx3, cx3));
            _mm_store_si128((__m128i*) &l4[idx4 & MASK], _mm_xor_si128(bx4, cx4));

            idx0 = EXTRACT64(cx0);
            idx1 = EXTRACT64(cx1);
            idx2 = EXTRACT64(cx2);
            idx3 = EXTRACT64(cx3);
            idx4 = EXTRACT64(cx4);

            bx0 = cx0;
            bx1 = cx1;
            bx2 = cx2;
            bx3 = cx3;
            bx4 = cx4;

            uint64_t hi, lo, cl, ch;
            cl = ((uint64_t*) &l0[idx0 & MASK])[0];
            ch = ((uint64_t*) &l0[idx0 & MASK])[1];
            lo = __umul128(idx0, cl, &hi);

            al0 += hi;
            ah0 += lo;

            ((uint64_t*) &l0[idx0 & MASK])[0] = al0;
            ((uint64_t*) &l0[idx0 & MASK])[1] = ah0;

            ah0 ^= ch;
            al0 ^= cl;
            idx0 = al0;


            cl = ((uint64_t*) &l1[idx1 & MASK])[0];
            ch = ((uint64_t*) &l1[idx1 & MASK])[1];
            lo = __umul128(idx1, cl, &hi);

            al1 += hi;
            ah1 += lo;

            ((uint64_t*) &l1[idx1 & MASK])[0] = al1;
            ((uint64_t*) &l1[idx1 & MASK])[1] = ah1;

            ah1 ^= ch;
            al1 ^= cl;
            idx1 = al1;


            cl = ((uint64_t*) &l2[idx2 & MASK])[0];
            ch = ((uint64_t*) &l2[idx2 & MASK])[1];
            lo = __umul128(idx2, cl, &hi);

            al2 += hi;
            ah2 += lo;

            ((uint64_t*) &l2[idx2 & MASK])[0] = al2;
            ((uint64_t*) &l2[idx2 & MASK])[1] = ah2;

            ah2 ^= ch;
            al2 ^= cl;
            idx2 = al2;


            cl = ((uint64_t*) &l3[idx3 & MASK])[0];
            ch = ((uint64_t*) &l3[idx3 & MASK])[1];
            lo = __umul128(idx3, cl, &hi);

            al3 += hi;
            ah3 += lo;

            ((uint64_t*) &l3[idx3 & MASK])[0] = al3;
            ((uint64_t*) &l3[idx3 & MASK])[1] = ah3;

            ah3 ^= ch;
            al3 ^= cl;
            idx3 = al3;


            cl = ((uint64_t*) &l4[idx4 & MASK])[0];
            ch = ((uint64_t*) &l4[idx4 & MASK])[1];
            lo = __umul128(idx4, cl, &hi);

            al4 += hi;
            ah4 += lo;

            ((uint64_t*) &l4[idx4 & MASK])[0] = al4;
            ((uint64_t*) &l4[idx4 & MASK])[1] = ah4;

            ah4 ^= ch;
            al4 ^= cl;
            idx4 = al4;
        }

        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l0, (__m128i*) h0);
        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l1, (__m128i*) h1);
        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l2, (__m128i*) h2);
        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l3, (__m128i*) h3);
        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l4, (__m128i*) h4);

        keccakf(h0, 24);
        keccakf(h1, 24);
        keccakf(h2, 24);
        keccakf(h3, 24);
        keccakf(h4, 24);

        extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
        extra_hashes[scratchPad[1]->state[0] & 3](scratchPad[1]->state, 200, output + 32);
        extra_hashes[scratchPad[2]->state[0] & 3](scratchPad[2]->state, 200, output + 64);
        extra_hashes[scratchPad[3]->state[0] & 3](scratchPad[3]->state, 200, output + 96);
        extra_hashes[scratchPad[4]->state[0] & 3](scratchPad[4]->state, 200, output + 128);
    }

  inline static void hashPowV2(const uint8_t* __restrict__ input,
                            size_t size,
                            uint8_t* __restrict__ output,
                            ScratchPad** __restrict__ scratchPad)
  {
      keccak((const uint8_t*) input, (int) size, scratchPad[0]->state, 200);
      keccak((const uint8_t*) input + size, (int) size, scratchPad[1]->state, 200);
      keccak((const uint8_t*) input + 2 * size, (int) size, scratchPad[2]->state, 200);
      keccak((const uint8_t*) input + 3 * size, (int) size, scratchPad[3]->state, 200);
      keccak((const uint8_t*) input + 4 * size, (int) size, scratchPad[4]->state, 200);

      uint64_t tweak1_2_0 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35) ^
                             *(reinterpret_cast<const uint64_t*>(scratchPad[0]->state) + 24));
      uint64_t tweak1_2_1 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + size) ^
                             *(reinterpret_cast<const uint64_t*>(scratchPad[1]->state) + 24));
      uint64_t tweak1_2_2 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + 2 * size) ^
                             *(reinterpret_cast<const uint64_t*>(scratchPad[2]->state) + 24));
      uint64_t tweak1_2_3 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + 3 * size) ^
                             *(reinterpret_cast<const uint64_t*>(scratchPad[3]->state) + 24));
      uint64_t tweak1_2_4 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + 4 * size) ^
                             *(reinterpret_cast<const uint64_t*>(scratchPad[4]->state) + 24));


      const uint8_t* l0 = scratchPad[0]->memory;
      const uint8_t* l1 = scratchPad[1]->memory;
      const uint8_t* l2 = scratchPad[2]->memory;
      const uint8_t* l3 = scratchPad[3]->memory;
      const uint8_t* l4 = scratchPad[4]->memory;
      uint64_t* h0 = reinterpret_cast<uint64_t*>(scratchPad[0]->state);
      uint64_t* h1 = reinterpret_cast<uint64_t*>(scratchPad[1]->state);
      uint64_t* h2 = reinterpret_cast<uint64_t*>(scratchPad[2]->state);
      uint64_t* h3 = reinterpret_cast<uint64_t*>(scratchPad[3]->state);
      uint64_t* h4 = reinterpret_cast<uint64_t*>(scratchPad[4]->state);

      cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h0, (__m128i*) l0);
      cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h1, (__m128i*) l1);
      cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h2, (__m128i*) l2);
      cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h3, (__m128i*) l3);
      cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h4, (__m128i*) l4);

      uint64_t al0 = h0[0] ^h0[4];
      uint64_t al1 = h1[0] ^h1[4];
      uint64_t al2 = h2[0] ^h2[4];
      uint64_t al3 = h3[0] ^h3[4];
      uint64_t al4 = h4[0] ^h4[4];
      uint64_t ah0 = h0[1] ^h0[5];
      uint64_t ah1 = h1[1] ^h1[5];
      uint64_t ah2 = h2[1] ^h2[5];
      uint64_t ah3 = h3[1] ^h3[5];
      uint64_t ah4 = h4[1] ^h4[5];

      __m128i bx0 = _mm_set_epi64x(h0[3] ^ h0[7], h0[2] ^ h0[6]);
      __m128i bx1 = _mm_set_epi64x(h1[3] ^ h1[7], h1[2] ^ h1[6]);
      __m128i bx2 = _mm_set_epi64x(h2[3] ^ h2[7], h2[2] ^ h2[6]);
      __m128i bx3 = _mm_set_epi64x(h3[3] ^ h3[7], h3[2] ^ h3[6]);
      __m128i bx4 = _mm_set_epi64x(h4[3] ^ h4[7], h4[2] ^ h4[6]);

      uint64_t idx0 = h0[0] ^h0[4];
      uint64_t idx1 = h1[0] ^h1[4];
      uint64_t idx2 = h2[0] ^h2[4];
      uint64_t idx3 = h3[0] ^h3[4];
      uint64_t idx4 = h4[0] ^h4[4];

      for (size_t i = 0; i < ITERATIONS; i++) {
          __m128i cx0;
          __m128i cx1;
          __m128i cx2;
          __m128i cx3;
          __m128i cx4;

          if (SOFT_AES) {
              cx0 = soft_aesenc((uint32_t*)&l0[idx0 & MASK], _mm_set_epi64x(ah0, al0));
              cx1 = soft_aesenc((uint32_t*)&l1[idx1 & MASK], _mm_set_epi64x(ah1, al1));
              cx2 = soft_aesenc((uint32_t*)&l2[idx2 & MASK], _mm_set_epi64x(ah2, al2));
              cx3 = soft_aesenc((uint32_t*)&l3[idx3 & MASK], _mm_set_epi64x(ah3, al3));
              cx4 = soft_aesenc((uint32_t*)&l4[idx4 & MASK], _mm_set_epi64x(ah4, al4));
          } else {
              cx0 = _mm_load_si128((__m128i*) &l0[idx0 & MASK]);
              cx1 = _mm_load_si128((__m128i*) &l1[idx1 & MASK]);
              cx2 = _mm_load_si128((__m128i*) &l2[idx2 & MASK]);
              cx3 = _mm_load_si128((__m128i*) &l3[idx3 & MASK]);
              cx4 = _mm_load_si128((__m128i*) &l4[idx4 & MASK]);

              cx0 = _mm_aesenc_si128(cx0, _mm_set_epi64x(ah0, al0));
              cx1 = _mm_aesenc_si128(cx1, _mm_set_epi64x(ah1, al1));
              cx2 = _mm_aesenc_si128(cx2, _mm_set_epi64x(ah2, al2));
              cx3 = _mm_aesenc_si128(cx3, _mm_set_epi64x(ah3, al3));
              cx4 = _mm_aesenc_si128(cx4, _mm_set_epi64x(ah4, al4));
          }

          _mm_store_si128((__m128i*) &l0[idx0 & MASK], _mm_xor_si128(bx0, cx0));
          _mm_store_si128((__m128i*) &l1[idx1 & MASK], _mm_xor_si128(bx1, cx1));
          _mm_store_si128((__m128i*) &l2[idx2 & MASK], _mm_xor_si128(bx2, cx2));
          _mm_store_si128((__m128i*) &l3[idx3 & MASK], _mm_xor_si128(bx3, cx3));
          _mm_store_si128((__m128i*) &l4[idx4 & MASK], _mm_xor_si128(bx4, cx4));

          static const uint32_t table = 0x75310;
          uint8_t tmp = reinterpret_cast<const uint8_t*>(&l0[idx0 & MASK])[11];
          uint8_t index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
          ((uint8_t*)(&l0[idx0 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);
          tmp = reinterpret_cast<const uint8_t*>(&l1[idx1 & MASK])[11];
          index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
          ((uint8_t*)(&l1[idx1 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);
          tmp = reinterpret_cast<const uint8_t*>(&l2[idx2 & MASK])[11];
          index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
          ((uint8_t*)(&l2[idx2 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);
          tmp = reinterpret_cast<const uint8_t*>(&l3[idx3 & MASK])[11];
          index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
          ((uint8_t*)(&l3[idx3 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);
          tmp = reinterpret_cast<const uint8_t*>(&l4[idx4 & MASK])[11];
          index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
          ((uint8_t*)(&l4[idx4 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);

          idx0 = EXTRACT64(cx0);
          idx1 = EXTRACT64(cx1);
          idx2 = EXTRACT64(cx2);
          idx3 = EXTRACT64(cx3);
          idx4 = EXTRACT64(cx4);

          bx0 = cx0;
          bx1 = cx1;
          bx2 = cx2;
          bx3 = cx3;
          bx4 = cx4;

          uint64_t hi, lo, cl, ch;
          cl = ((uint64_t*) &l0[idx0 & MASK])[0];
          ch = ((uint64_t*) &l0[idx0 & MASK])[1];
          lo = __umul128(idx0, cl, &hi);

          al0 += hi;
          ah0 += lo;

          ah0 ^= tweak1_2_0;
          ((uint64_t*) &l0[idx0 & MASK])[0] = al0;
          ((uint64_t*) &l0[idx0 & MASK])[1] = ah0;
          ah0 ^= tweak1_2_0;

          ah0 ^= ch;
          al0 ^= cl;
          idx0 = al0;


          cl = ((uint64_t*) &l1[idx1 & MASK])[0];
          ch = ((uint64_t*) &l1[idx1 & MASK])[1];
          lo = __umul128(idx1, cl, &hi);

          al1 += hi;
          ah1 += lo;

          ah1 ^= tweak1_2_1;
          ((uint64_t*) &l1[idx1 & MASK])[0] = al1;
          ((uint64_t*) &l1[idx1 & MASK])[1] = ah1;
          ah1 ^= tweak1_2_1;

          ah1 ^= ch;
          al1 ^= cl;
          idx1 = al1;


          cl = ((uint64_t*) &l2[idx2 & MASK])[0];
          ch = ((uint64_t*) &l2[idx2 & MASK])[1];
          lo = __umul128(idx2, cl, &hi);

          al2 += hi;
          ah2 += lo;

          ah2 ^= tweak1_2_2;
          ((uint64_t*) &l2[idx2 & MASK])[0] = al2;
          ((uint64_t*) &l2[idx2 & MASK])[1] = ah2;
          ah2 ^= tweak1_2_2;

          ah2 ^= ch;
          al2 ^= cl;
          idx2 = al2;


          cl = ((uint64_t*) &l3[idx3 & MASK])[0];
          ch = ((uint64_t*) &l3[idx3 & MASK])[1];
          lo = __umul128(idx3, cl, &hi);

          al3 += hi;
          ah3 += lo;

          ah3 ^= tweak1_2_3;
          ((uint64_t*) &l3[idx3 & MASK])[0] = al3;
          ((uint64_t*) &l3[idx3 & MASK])[1] = ah3;
          ah3 ^= tweak1_2_3;

          ah3 ^= ch;
          al3 ^= cl;
          idx3 = al3;


          cl = ((uint64_t*) &l4[idx4 & MASK])[0];
          ch = ((uint64_t*) &l4[idx4 & MASK])[1];
          lo = __umul128(idx4, cl, &hi);

          al4 += hi;
          ah4 += lo;

          ah4 ^= tweak1_2_4;
          ((uint64_t*) &l4[idx4 & MASK])[0] = al4;
          ((uint64_t*) &l4[idx4 & MASK])[1] = ah4;
          ah4 ^= tweak1_2_4;

          ah4 ^= ch;
          al4 ^= cl;
          idx4 = al4;
      }

      cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l0, (__m128i*) h0);
      cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l1, (__m128i*) h1);
      cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l2, (__m128i*) h2);
      cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l3, (__m128i*) h3);
      cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l4, (__m128i*) h4);

      keccakf(h0, 24);
      keccakf(h1, 24);
      keccakf(h2, 24);
      keccakf(h3, 24);
      keccakf(h4, 24);

      extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
      extra_hashes[scratchPad[1]->state[0] & 3](scratchPad[1]->state, 200, output + 32);
      extra_hashes[scratchPad[2]->state[0] & 3](scratchPad[2]->state, 200, output + 64);
      extra_hashes[scratchPad[3]->state[0] & 3](scratchPad[3]->state, 200, output + 96);
      extra_hashes[scratchPad[4]->state[0] & 3](scratchPad[4]->state, 200, output + 128);
  }

    inline static void hashLiteTube(const uint8_t* __restrict__ input,
                                 size_t size,
                                 uint8_t* __restrict__ output,
                                 ScratchPad** __restrict__ scratchPad)
    {
        keccak((const uint8_t*) input, (int) size, scratchPad[0]->state, 200);
        keccak((const uint8_t*) input + size, (int) size, scratchPad[1]->state, 200);
        keccak((const uint8_t*) input + 2 * size, (int) size, scratchPad[2]->state, 200);
        keccak((const uint8_t*) input + 3 * size, (int) size, scratchPad[3]->state, 200);
        keccak((const uint8_t*) input + 4 * size, (int) size, scratchPad[4]->state, 200);

        uint64_t tweak1_2_0 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35) ^
                               *(reinterpret_cast<const uint64_t*>(scratchPad[0]->state) + 24));
        uint64_t tweak1_2_1 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + size) ^
                               *(reinterpret_cast<const uint64_t*>(scratchPad[1]->state) + 24));
        uint64_t tweak1_2_2 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + 2 * size) ^
                               *(reinterpret_cast<const uint64_t*>(scratchPad[2]->state) + 24));
        uint64_t tweak1_2_3 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + 3 * size) ^
                               *(reinterpret_cast<const uint64_t*>(scratchPad[3]->state) + 24));
        uint64_t tweak1_2_4 = (*reinterpret_cast<const uint64_t*>(reinterpret_cast<const uint8_t*>(input) + 35 + 4 * size) ^
                               *(reinterpret_cast<const uint64_t*>(scratchPad[4]->state) + 24));


        const uint8_t* l0 = scratchPad[0]->memory;
        const uint8_t* l1 = scratchPad[1]->memory;
        const uint8_t* l2 = scratchPad[2]->memory;
        const uint8_t* l3 = scratchPad[3]->memory;
        const uint8_t* l4 = scratchPad[4]->memory;
        uint64_t* h0 = reinterpret_cast<uint64_t*>(scratchPad[0]->state);
        uint64_t* h1 = reinterpret_cast<uint64_t*>(scratchPad[1]->state);
        uint64_t* h2 = reinterpret_cast<uint64_t*>(scratchPad[2]->state);
        uint64_t* h3 = reinterpret_cast<uint64_t*>(scratchPad[3]->state);
        uint64_t* h4 = reinterpret_cast<uint64_t*>(scratchPad[4]->state);

        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h0, (__m128i*) l0);
        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h1, (__m128i*) l1);
        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h2, (__m128i*) l2);
        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h3, (__m128i*) l3);
        cn_explode_scratchpad<MEM, SOFT_AES>((__m128i*) h4, (__m128i*) l4);

        uint64_t al0 = h0[0] ^h0[4];
        uint64_t al1 = h1[0] ^h1[4];
        uint64_t al2 = h2[0] ^h2[4];
        uint64_t al3 = h3[0] ^h3[4];
        uint64_t al4 = h4[0] ^h4[4];
        uint64_t ah0 = h0[1] ^h0[5];
        uint64_t ah1 = h1[1] ^h1[5];
        uint64_t ah2 = h2[1] ^h2[5];
        uint64_t ah3 = h3[1] ^h3[5];
        uint64_t ah4 = h4[1] ^h4[5];

        __m128i bx0 = _mm_set_epi64x(h0[3] ^ h0[7], h0[2] ^ h0[6]);
        __m128i bx1 = _mm_set_epi64x(h1[3] ^ h1[7], h1[2] ^ h1[6]);
        __m128i bx2 = _mm_set_epi64x(h2[3] ^ h2[7], h2[2] ^ h2[6]);
        __m128i bx3 = _mm_set_epi64x(h3[3] ^ h3[7], h3[2] ^ h3[6]);
        __m128i bx4 = _mm_set_epi64x(h4[3] ^ h4[7], h4[2] ^ h4[6]);

        uint64_t idx0 = h0[0] ^h0[4];
        uint64_t idx1 = h1[0] ^h1[4];
        uint64_t idx2 = h2[0] ^h2[4];
        uint64_t idx3 = h3[0] ^h3[4];
        uint64_t idx4 = h4[0] ^h4[4];

        for (size_t i = 0; i < ITERATIONS; i++) {
            __m128i cx0;
            __m128i cx1;
            __m128i cx2;
            __m128i cx3;
            __m128i cx4;

            if (SOFT_AES) {
                cx0 = soft_aesenc((uint32_t*)&l0[idx0 & MASK], _mm_set_epi64x(ah0, al0));
                cx1 = soft_aesenc((uint32_t*)&l1[idx1 & MASK], _mm_set_epi64x(ah1, al1));
                cx2 = soft_aesenc((uint32_t*)&l2[idx2 & MASK], _mm_set_epi64x(ah2, al2));
                cx3 = soft_aesenc((uint32_t*)&l3[idx3 & MASK], _mm_set_epi64x(ah3, al3));
                cx4 = soft_aesenc((uint32_t*)&l4[idx4 & MASK], _mm_set_epi64x(ah4, al4));
            } else {
                cx0 = _mm_load_si128((__m128i*) &l0[idx0 & MASK]);
                cx1 = _mm_load_si128((__m128i*) &l1[idx1 & MASK]);
                cx2 = _mm_load_si128((__m128i*) &l2[idx2 & MASK]);
                cx3 = _mm_load_si128((__m128i*) &l3[idx3 & MASK]);
                cx4 = _mm_load_si128((__m128i*) &l4[idx4 & MASK]);

                cx0 = _mm_aesenc_si128(cx0, _mm_set_epi64x(ah0, al0));
                cx1 = _mm_aesenc_si128(cx1, _mm_set_epi64x(ah1, al1));
                cx2 = _mm_aesenc_si128(cx2, _mm_set_epi64x(ah2, al2));
                cx3 = _mm_aesenc_si128(cx3, _mm_set_epi64x(ah3, al3));
                cx4 = _mm_aesenc_si128(cx4, _mm_set_epi64x(ah4, al4));
            }

            _mm_store_si128((__m128i*) &l0[idx0 & MASK], _mm_xor_si128(bx0, cx0));
            _mm_store_si128((__m128i*) &l1[idx1 & MASK], _mm_xor_si128(bx1, cx1));
            _mm_store_si128((__m128i*) &l2[idx2 & MASK], _mm_xor_si128(bx2, cx2));
            _mm_store_si128((__m128i*) &l3[idx3 & MASK], _mm_xor_si128(bx3, cx3));
            _mm_store_si128((__m128i*) &l4[idx4 & MASK], _mm_xor_si128(bx4, cx4));

            static const uint32_t table = 0x75310;
            uint8_t tmp = reinterpret_cast<const uint8_t*>(&l0[idx0 & MASK])[11];
            uint8_t index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
            ((uint8_t*)(&l0[idx0 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);
            tmp = reinterpret_cast<const uint8_t*>(&l1[idx1 & MASK])[11];
            index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
            ((uint8_t*)(&l1[idx1 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);
            tmp = reinterpret_cast<const uint8_t*>(&l2[idx2 & MASK])[11];
            index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
            ((uint8_t*)(&l2[idx2 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);
            tmp = reinterpret_cast<const uint8_t*>(&l3[idx3 & MASK])[11];
            index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
            ((uint8_t*)(&l3[idx3 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);
            tmp = reinterpret_cast<const uint8_t*>(&l4[idx4 & MASK])[11];
            index = (((tmp >> INDEX_SHIFT) & 6) | (tmp & 1)) << 1;
            ((uint8_t*)(&l4[idx4 & MASK]))[11] = tmp ^ ((table >> index) & 0x30);

            idx0 = EXTRACT64(cx0);
            idx1 = EXTRACT64(cx1);
            idx2 = EXTRACT64(cx2);
            idx3 = EXTRACT64(cx3);
            idx4 = EXTRACT64(cx4);

            bx0 = cx0;
            bx1 = cx1;
            bx2 = cx2;
            bx3 = cx3;
            bx4 = cx4;

            uint64_t hi, lo, cl, ch;
            cl = ((uint64_t*) &l0[idx0 & MASK])[0];
            ch = ((uint64_t*) &l0[idx0 & MASK])[1];
            lo = __umul128(idx0, cl, &hi);

            al0 += hi;
            ah0 += lo;

            ah0 ^= tweak1_2_0;
            ((uint64_t*) &l0[idx0 & MASK])[0] = al0;
            ((uint64_t*) &l0[idx0 & MASK])[1] = ah0;
            ah0 ^= tweak1_2_0;

            ((uint64_t*)&l0[idx0 & MASK])[1] ^= ((uint64_t*)&l0[idx0 & MASK])[0];

            ah0 ^= ch;
            al0 ^= cl;
            idx0 = al0;


            cl = ((uint64_t*) &l1[idx1 & MASK])[0];
            ch = ((uint64_t*) &l1[idx1 & MASK])[1];
            lo = __umul128(idx1, cl, &hi);

            al1 += hi;
            ah1 += lo;

            ah1 ^= tweak1_2_1;
            ((uint64_t*) &l1[idx1 & MASK])[0] = al1;
            ((uint64_t*) &l1[idx1 & MASK])[1] = ah1;
            ah1 ^= tweak1_2_1;

            ((uint64_t*)&l1[idx1 & MASK])[1] ^= ((uint64_t*)&l1[idx1 & MASK])[0];

            ah1 ^= ch;
            al1 ^= cl;
            idx1 = al1;


            cl = ((uint64_t*) &l2[idx2 & MASK])[0];
            ch = ((uint64_t*) &l2[idx2 & MASK])[1];
            lo = __umul128(idx2, cl, &hi);

            al2 += hi;
            ah2 += lo;

            ah2 ^= tweak1_2_2;
            ((uint64_t*) &l2[idx2 & MASK])[0] = al2;
            ((uint64_t*) &l2[idx2 & MASK])[1] = ah2;
            ah2 ^= tweak1_2_2;

            ((uint64_t*)&l2[idx2 & MASK])[1] ^= ((uint64_t*)&l2[idx2 & MASK])[0];

            ah2 ^= ch;
            al2 ^= cl;
            idx2 = al2;


            cl = ((uint64_t*) &l3[idx3 & MASK])[0];
            ch = ((uint64_t*) &l3[idx3 & MASK])[1];
            lo = __umul128(idx3, cl, &hi);

            al3 += hi;
            ah3 += lo;

            ah3 ^= tweak1_2_3;
            ((uint64_t*) &l3[idx3 & MASK])[0] = al3;
            ((uint64_t*) &l3[idx3 & MASK])[1] = ah3;
            ah3 ^= tweak1_2_3;

            ((uint64_t*)&l3[idx3 & MASK])[1] ^= ((uint64_t*)&l3[idx3 & MASK])[0];

            ah3 ^= ch;
            al3 ^= cl;
            idx3 = al3;


            cl = ((uint64_t*) &l4[idx4 & MASK])[0];
            ch = ((uint64_t*) &l4[idx4 & MASK])[1];
            lo = __umul128(idx4, cl, &hi);

            al4 += hi;
            ah4 += lo;

            ah4 ^= tweak1_2_4;
            ((uint64_t*) &l4[idx4 & MASK])[0] = al4;
            ((uint64_t*) &l4[idx4 & MASK])[1] = ah4;
            ah4 ^= tweak1_2_4;

            ((uint64_t*)&l4[idx4 & MASK])[1] ^= ((uint64_t*)&l4[idx4 & MASK])[0];

            ah4 ^= ch;
            al4 ^= cl;
            idx4 = al4;
        }

        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l0, (__m128i*) h0);
        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l1, (__m128i*) h1);
        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l2, (__m128i*) h2);
        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l3, (__m128i*) h3);
        cn_implode_scratchpad<MEM, SOFT_AES>((__m128i*) l4, (__m128i*) h4);

        keccakf(h0, 24);
        keccakf(h1, 24);
        keccakf(h2, 24);
        keccakf(h3, 24);
        keccakf(h4, 24);

        extra_hashes[scratchPad[0]->state[0] & 3](scratchPad[0]->state, 200, output);
        extra_hashes[scratchPad[1]->state[0] & 3](scratchPad[1]->state, 200, output + 32);
        extra_hashes[scratchPad[2]->state[0] & 3](scratchPad[2]->state, 200, output + 64);
        extra_hashes[scratchPad[3]->state[0] & 3](scratchPad[3]->state, 200, output + 96);
        extra_hashes[scratchPad[4]->state[0] & 3](scratchPad[4]->state, 200, output + 128);
    }

    inline static void hashHeavy(const uint8_t* __restrict__ input,
                                 size_t size,
                                 uint8_t* __restrict__ output,
                                 ScratchPad** __restrict__ scratchPad)
    {
        // not supported
    }

    inline static void hashHeavyHaven(const uint8_t* __restrict__ input,
                                      size_t size,
                                      uint8_t* __restrict__ output,
                                      ScratchPad** __restrict__ scratchPad)
    {
        // not supported
    }

    inline static void hashHeavyTube(const uint8_t* __restrict__ input,
                                     size_t size,
                                     uint8_t* __restrict__ output,
                                     ScratchPad** __restrict__ scratchPad)
    {
        // not supported
    }
};

#endif /* __CRYPTONIGHT_X86_H__ */
