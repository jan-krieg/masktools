#include "binarize.h"
#include "../../common/simd.h"
#include "../../common/16bit.h"

using namespace Filtering;

typedef Word (Operator)(Word, Word);

template<int bits_per_pixel>
inline Word binarize_upper(Word x, Word t) { return x > t ? 0 : ((1 << bits_per_pixel)-1); }
template<int bits_per_pixel>
inline Word binarize_lower(Word x, Word t) { return x > t ? ((1 << bits_per_pixel)-1) : 0; }

template<int bits_per_pixel>
inline Word binarize_0_x(Word x, Word t) { return x > t ? 0 : x; }
template<int bits_per_pixel>
inline Word binarize_t_x(Word x, Word t) { return x > t ? t : x; }
template<int bits_per_pixel>
inline Word binarize_x_0(Word x, Word t) { return x > t ? x : 0; }
template<int bits_per_pixel>
inline Word binarize_x_t(Word x, Word t) { return x > t ? x : t; }

template<int bits_per_pixel>
inline Word binarize_t_0(Word x, Word t) { return x > t ? t : 0; }
template<int bits_per_pixel>
inline Word binarize_0_t(Word x, Word t) { return x > t ? 0 : t; }

template<int bits_per_pixel>
inline Word binarize_x_255(Word x, Word t) { return x > t ? x : ((1 << bits_per_pixel)-1); }
template<int bits_per_pixel>
inline Word binarize_t_255(Word x, Word t) { return x > t ? t : ((1 << bits_per_pixel)-1); }
template<int bits_per_pixel>
inline Word binarize_255_x(Word x, Word t) { return x > t ? ((1 << bits_per_pixel)-1) : x; }
template<int bits_per_pixel>
inline Word binarize_255_t(Word x, Word t) { return x > t ? ((1 << bits_per_pixel)-1) : t; }

// specialize them
#define MAKE_TEMPLATES(bits_per_pixel) \
template Word binarize_upper<bits_per_pixel>(Word x, Word t); \
template Word binarize_lower<bits_per_pixel>(Word x, Word t); \
template Word binarize_0_x<bits_per_pixel>(Word x, Word t); \
template Word binarize_t_x<bits_per_pixel>(Word x, Word t); \
template Word binarize_x_0<bits_per_pixel>(Word x, Word t); \
template Word binarize_x_t<bits_per_pixel>(Word x, Word t); \
template Word binarize_t_0<bits_per_pixel>(Word x, Word t); \
template Word binarize_0_t<bits_per_pixel>(Word x, Word t); \
template Word binarize_x_255<bits_per_pixel>(Word x, Word t); \
template Word binarize_t_255<bits_per_pixel>(Word x, Word t); \
template Word binarize_255_x<bits_per_pixel>(Word x, Word t); \
template Word binarize_255_t<bits_per_pixel>(Word x, Word t);

MAKE_TEMPLATES(10)
MAKE_TEMPLATES(12)
MAKE_TEMPLATES(14)
MAKE_TEMPLATES(16)
#undef MAKE_TEMPLATES

template <Operator op>
void binarize_stacked_t(Byte *pDst, ptrdiff_t nDstPitch, Word nThreshold, int nWidth, int nHeight, int nOrigHeight)
{
    auto pLsb = pDst + nDstPitch * nOrigHeight / 2;

    for ( int y = 0; y < nHeight / 2; y++ )
    {
        for ( int x = 0; x < nWidth; x++ ) {
            Word result = op(read_word_stacked(pDst, pLsb, x), nThreshold);
            write_word_stacked(pDst, pLsb, x, result);
        }
        pDst += nDstPitch;
        pLsb += nDstPitch;
    }
}

template <Operator op>
void binarize_native_t(Byte *pDst, ptrdiff_t nDstPitch, Word nThreshold, int nWidth, int nHeight, int)
{
    for ( int y = 0; y < nHeight; y++ )
    {
        auto pDst16 = reinterpret_cast<Word*>(pDst);
        for ( int x = 0; x < nWidth; x+=1 )
            pDst16[x] = op(pDst16[x], nThreshold);
        pDst += nDstPitch;
    }
}

/* SSE2 functions */
template<int bits_per_pixel>
static inline __m128i binarize_upper_sse2_op(__m128i x, __m128i t, __m128i, __m128i &max) {
    auto r = _mm_subs_epu16(x, t);
    if (bits_per_pixel == 16) {
      return _mm_cmpeq_epi16(r, _mm_setzero_si128()); // set FFFF where equal to 0, that is x<t
    } 
    else {
      return _mm_and_si128(_mm_cmpeq_epi16(r, _mm_setzero_si128()), max);
    }
}

template<int bits_per_pixel>
static MT_FORCEINLINE __m128i binarize_lower_sse2_op(__m128i x, __m128i, __m128i halfrange, __m128i &max) {
#pragma warning(disable: 4309)
  auto r = _mm_add_epi16(x,  _mm_set1_epi16(0x8000)); // 0x8000 for (at 8 bit version:0x80) and 16bit branch signed cmp
#pragma warning(default: 4309)
  if (bits_per_pixel == 16) {
      return _mm_cmpgt_epi16(r, halfrange);
    } 
    else {
      return _mm_and_si128(_mm_cmpgt_epi16(r, halfrange), max);
    }
}

template<int bits_per_pixel>
static inline __m128i binarize_0_x_sse2_op(__m128i x, __m128i t, __m128i halfrange, __m128i &max) {
    auto upper = binarize_upper_sse2_op<16>(x, t, halfrange, max); // 16 bit returns faster 0xFFFF needed for and
    return _mm_and_si128(upper, x);
}

template<int bits_per_pixel>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("sse4.1")))
#endif
static inline __m128i binarize_t_x_sse2_op(__m128i x, __m128i t, __m128i, __m128i &) {
    return _mm_min_epu16(t, x);
}

template<int bits_per_pixel>
static inline __m128i binarize_x_0_sse2_op(__m128i x, __m128i t, __m128i halfrange, __m128i &max) {
    auto lower = binarize_lower_sse2_op<16>(x, t, halfrange, max);  // 16 bit returns faster 0xFFFF  needed for and
    return _mm_and_si128(lower, x);
}

template<int bits_per_pixel>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("sse4.1")))
#endif
static inline __m128i binarize_x_t_sse2_op(__m128i x, __m128i t, __m128i, __m128i &) {
    return _mm_max_epu16(t, x);
}

template<int bits_per_pixel>
static inline __m128i binarize_t_0_sse2_op(__m128i x, __m128i t, __m128i halfrange, __m128i &max) {
    auto lower = binarize_lower_sse2_op<16>(x, t, halfrange, max);  // 16 bit returns faster 0xFFFF needed for and
    return _mm_and_si128(lower, t);
}

template<int bits_per_pixel>
static inline __m128i binarize_0_t_sse2_op(__m128i x, __m128i t, __m128i halfrange, __m128i &max) {
    auto upper = binarize_upper_sse2_op<16>(x, t, halfrange, max);  // 16 bit returns faster 0xFFFF needed for and
    return _mm_and_si128(upper, t);
}

template<int bits_per_pixel>
static inline __m128i binarize_x_255_sse2_op(__m128i x, __m128i t, __m128i halfrange, __m128i &max) {
    auto upper = binarize_upper_sse2_op<bits_per_pixel>(x, t, halfrange, max); 
    return _mm_or_si128(upper, x);
}

template<int bits_per_pixel>
static inline __m128i binarize_255_x_sse2_op(__m128i x, __m128i t, __m128i halfrange, __m128i &max) {
    auto lower = binarize_lower_sse2_op<bits_per_pixel>(x, t, halfrange, max);
    return _mm_or_si128(lower, x);
}

template<int bits_per_pixel>
static inline __m128i binarize_t_255_sse2_op(__m128i x, __m128i t, __m128i halfrange, __m128i &max) {
    auto upper = binarize_upper_sse2_op<bits_per_pixel>(x, t, halfrange, max);
    return _mm_or_si128(upper, t);
}

template<int bits_per_pixel>
static inline __m128i binarize_255_t_sse2_op(__m128i x, __m128i t, __m128i halfrange, __m128i &max) {
    auto lower = binarize_lower_sse2_op<bits_per_pixel>(x, t, halfrange, max);
    return _mm_or_si128(lower, t);
}

#pragma warning(disable: 4309)
template<int bits_per_pixel, decltype(binarize_upper_sse2_op<16>) op, decltype(binarize_upper<16>) op_c>
void binarize_sse2_stacked_t(Byte *pDst, ptrdiff_t nDstPitch, Word nThreshold, int nWidth, int nHeight, int nOrigHeight)
{
    int wMod8 = (nWidth / 8) * 8;
    auto pDst2 = pDst;
    auto pDstLsb = pDst + nDstPitch * nOrigHeight / 2;

    auto t = _mm_set1_epi16(Word(nThreshold));
    auto halfrange = _mm_add_epi16(t, _mm_set1_epi16(0x8000)); // shift for signed int cmp
    auto ff = _mm_set1_epi16(0x00FF);
    auto zero = _mm_setzero_si128();
    auto max = _mm_set1_epi16(Word((1 << bits_per_pixel) - 1));

    for ( int j = 0; j < nHeight / 2; ++j ) {
        for ( int i = 0; i < wMod8; i+=8 ) {
            auto src =  read_word_stacked_simd(pDst, pDstLsb, i);
            auto result = op(src, t, halfrange, max);

            write_word_stacked_simd(pDst, pDstLsb, i, result, ff, zero);
        }
        pDst += nDstPitch;
                pDstLsb += nDstPitch;
    }

    if (nWidth > wMod8) {
        binarize_stacked_t<op_c>(pDst2 + wMod8, nDstPitch, nThreshold, nWidth - wMod8, nHeight, nOrigHeight);
    }
}

template<int bits_per_pixel, decltype(binarize_upper_sse2_op<16>) op, decltype(binarize_upper<16>) op_c> // 16 or whatever for decltype
void binarize_sse2_native_t(Byte *pDst, ptrdiff_t nDstPitch, Word nThreshold, int nWidth, int nHeight, int nOrigHeight)
{
    nWidth *= 2; // really rowsize: width * sizeof(uint16)
    int wMod16 = (nWidth / 16) * 16;
    auto pDst2 = pDst;
    auto t = _mm_set1_epi16(Word(nThreshold));
    auto halfrange = _mm_add_epi16(t, _mm_set1_epi16(0x8000)); // shift for signed int cmp
    auto max = _mm_set1_epi16(Word((1 << bits_per_pixel) - 1));

    for (int j = 0; j < nHeight; ++j) {
        for (int i = 0; i < wMod16; i+=16) {
            auto src = simd_load_si128<MemoryMode::SSE2_UNALIGNED>(reinterpret_cast<const __m128i*>(pDst+i));

            auto result = op(src, t, halfrange, max);

            simd_store_si128<MemoryMode::SSE2_UNALIGNED>(reinterpret_cast<__m128i*>(pDst+i), result);
        }
        pDst += nDstPitch;
    }
    if (nWidth > wMod16) {
        binarize_native_t<op_c>(pDst2 + wMod16, nDstPitch, nThreshold, (nWidth - wMod16)/sizeof(uint16_t), nHeight, nOrigHeight); // rowsize/2
    }
}

namespace Filtering { namespace MaskTools { namespace Filters { namespace Binarize {

#define DEFINE_PROCESSORS(layout,bits_per_pixel) \
Processor16 *binarize_upper_##layout##_##bits_per_pixel##_c = &binarize_##layout##_t<binarize_upper< bits_per_pixel >>;  \
Processor16 *binarize_lower_##layout##_##bits_per_pixel##_c = &binarize_##layout##_t<binarize_lower< bits_per_pixel >>;  \
Processor16 *binarize_0_x_##layout##_##bits_per_pixel##_c   = &binarize_##layout##_t<binarize_0_x< bits_per_pixel >>;    \
Processor16 *binarize_t_x_##layout##_##bits_per_pixel##_c   = &binarize_##layout##_t<binarize_t_x< bits_per_pixel >>;    \
Processor16 *binarize_x_0_##layout##_##bits_per_pixel##_c   = &binarize_##layout##_t<binarize_x_0< bits_per_pixel >>;    \
Processor16 *binarize_x_t_##layout##_##bits_per_pixel##_c   = &binarize_##layout##_t<binarize_x_t< bits_per_pixel >>;    \
Processor16 *binarize_t_0_##layout##_##bits_per_pixel##_c   = &binarize_##layout##_t<binarize_t_0< bits_per_pixel >>;    \
Processor16 *binarize_0_t_##layout##_##bits_per_pixel##_c   = &binarize_##layout##_t<binarize_0_t< bits_per_pixel >>;    \
Processor16 *binarize_x_255_##layout##_##bits_per_pixel##_c = &binarize_##layout##_t<binarize_x_255< bits_per_pixel >>;  \
Processor16 *binarize_t_255_##layout##_##bits_per_pixel##_c = &binarize_##layout##_t<binarize_t_255< bits_per_pixel >>;  \
Processor16 *binarize_255_x_##layout##_##bits_per_pixel##_c = &binarize_##layout##_t<binarize_255_x< bits_per_pixel >>;  \
Processor16 *binarize_255_t_##layout##_##bits_per_pixel##_c = &binarize_##layout##_t<binarize_255_t< bits_per_pixel >>;  \
    \
Processor16 *binarize_upper_##layout##_##bits_per_pixel##_sse2 = &binarize_sse2_##layout##_t< bits_per_pixel,binarize_upper_sse2_op< bits_per_pixel >, binarize_upper< bits_per_pixel >>; \
Processor16 *binarize_lower_##layout##_##bits_per_pixel##_sse2 = &binarize_sse2_##layout##_t< bits_per_pixel,binarize_lower_sse2_op< bits_per_pixel >, binarize_lower< bits_per_pixel >>; \
Processor16 *binarize_0_x_##layout##_##bits_per_pixel##_sse2   = &binarize_sse2_##layout##_t< bits_per_pixel,binarize_0_x_sse2_op< bits_per_pixel >,   binarize_0_x< bits_per_pixel >>;     \
Processor16 *binarize_t_x_##layout##_##bits_per_pixel##_sse2   = &binarize_sse2_##layout##_t< bits_per_pixel,binarize_t_x_sse2_op< bits_per_pixel >,   binarize_t_x< bits_per_pixel >>;     \
Processor16 *binarize_x_0_##layout##_##bits_per_pixel##_sse2   = &binarize_sse2_##layout##_t< bits_per_pixel,binarize_x_0_sse2_op< bits_per_pixel >,   binarize_x_0< bits_per_pixel >>;     \
Processor16 *binarize_x_t_##layout##_##bits_per_pixel##_sse2   = &binarize_sse2_##layout##_t< bits_per_pixel,binarize_x_t_sse2_op< bits_per_pixel >,   binarize_x_t< bits_per_pixel >>;     \
Processor16 *binarize_t_0_##layout##_##bits_per_pixel##_sse2   = &binarize_sse2_##layout##_t< bits_per_pixel,binarize_t_0_sse2_op< bits_per_pixel >,   binarize_t_0< bits_per_pixel >>;     \
Processor16 *binarize_0_t_##layout##_##bits_per_pixel##_sse2   = &binarize_sse2_##layout##_t< bits_per_pixel,binarize_0_t_sse2_op< bits_per_pixel >,   binarize_0_t< bits_per_pixel >>;     \
Processor16 *binarize_x_255_##layout##_##bits_per_pixel##_sse2 = &binarize_sse2_##layout##_t< bits_per_pixel,binarize_x_255_sse2_op< bits_per_pixel >, binarize_x_255< bits_per_pixel >>; \
Processor16 *binarize_t_255_##layout##_##bits_per_pixel##_sse2 = &binarize_sse2_##layout##_t< bits_per_pixel,binarize_t_255_sse2_op< bits_per_pixel >, binarize_t_255< bits_per_pixel >>; \
Processor16 *binarize_255_x_##layout##_##bits_per_pixel##_sse2 = &binarize_sse2_##layout##_t< bits_per_pixel,binarize_255_x_sse2_op< bits_per_pixel >, binarize_255_x< bits_per_pixel >>; \
Processor16 *binarize_255_t_##layout##_##bits_per_pixel##_sse2 = &binarize_sse2_##layout##_t< bits_per_pixel,binarize_255_t_sse2_op< bits_per_pixel >, binarize_255_t< bits_per_pixel >>; \


DEFINE_PROCESSORS(stacked,16)
DEFINE_PROCESSORS(native,10)
DEFINE_PROCESSORS(native,12)
DEFINE_PROCESSORS(native,14)
DEFINE_PROCESSORS(native,16)

#undef DEFINE_PROCESSORS


} } } }