#ifndef __Mt_MorphologicFunctions16_H__
#define __Mt_MorphologicFunctions16_H__

#include "../../../common/utils/utils.h"
#include "../../common/simd.h"
#include "../../common/16bit.h"

namespace Filtering { namespace MaskTools { namespace Filters { namespace Morphologic {

typedef Word (Operator16)(Word, Word, Word, Word, Word, Word, Word, Word, Word, int); // last is nMaxDeviation
typedef __m128i (Limit16)(__m128i source, __m128i sum, __m128i deviation);
typedef void (ProcessLine16Sse4)(Byte *pDst, const Byte *pSrcp, const Byte *pSrc, const Byte *pSrcn, const __m128i &maxDeviation, int width);

enum Directions {
    Vertical = 1,
    Horizontal = 2,
    Both = Vertical | Horizontal,
    Square = 7
};

//height should be already divised by 2
template<Border borderMode, Operator16 op>
void process_line_morpho_stacked_c(Byte *pDst, const Byte *pSrcp, const Byte *pSrc, const Byte *pSrcn, int maxDeviation, int width, int height, ptrdiff_t srcPitch, ptrdiff_t dstPitch, int nOrigHeight) {
    UNUSED(height);
    Byte *pDstLsb = pDst + nOrigHeight * dstPitch / 2;
    const Byte *pSrcpLsb = pSrcp + nOrigHeight * srcPitch / 2;
    const Byte *pSrcLsb = pSrc + nOrigHeight * srcPitch / 2;
    const Byte *pSrcnLsb = pSrcn + nOrigHeight * srcPitch / 2;
    const int leftOffset = borderMode == Border::Left ? 0 : 1;
    const int rightOffset = borderMode == Border::Right ? 0 : 1;

    for (int x = 0; x < width; ++x) {
        Word output = op(read_word_stacked(pSrcp, pSrcpLsb, x-leftOffset), 
            read_word_stacked(pSrcp, pSrcpLsb, x), 
            read_word_stacked(pSrcp, pSrcpLsb, x+rightOffset), 
            read_word_stacked(pSrc, pSrcLsb, x-leftOffset), 
            read_word_stacked(pSrc, pSrcLsb, x), 
            read_word_stacked(pSrc, pSrcLsb, x+rightOffset), 
            read_word_stacked(pSrcn, pSrcnLsb, x-leftOffset), 
            read_word_stacked(pSrcn, pSrcnLsb, x), 
            read_word_stacked(pSrcn, pSrcnLsb, x+rightOffset), 
            maxDeviation);
        
        write_word_stacked(pDst, pDstLsb, x, output);
    }
}

template<Border borderMode, Operator16 op>
void process_line_morpho_16_c(Word *pDst, const Word *pSrcp, const Word *pSrc, const Word *pSrcn, int maxDeviation, int width, int height, ptrdiff_t srcPitch, ptrdiff_t dstPitch, int nOrigHeight) {
    UNUSED(dstPitch); UNUSED(srcPitch); UNUSED(height); UNUSED(nOrigHeight);

    const int leftOffset = borderMode == Border::Left ? 0 : 1;
    const int rightOffset = borderMode == Border::Right ? 0 : 1;

    for (int x = 0; x < width; ++x) {
        pDst[x] = op(
            reinterpret_cast<const Word*>(pSrcp)[x-leftOffset], 
            reinterpret_cast<const Word*>(pSrcp)[x], 
            reinterpret_cast<const Word*>(pSrcp)[x+rightOffset], 
            reinterpret_cast<const Word*>(pSrc)[x-leftOffset], 
            reinterpret_cast<const Word*>(pSrc)[x], 
            reinterpret_cast<const Word*>(pSrc)[x+rightOffset], 
            reinterpret_cast<const Word*>(pSrcn)[x-leftOffset], 
            reinterpret_cast<const Word*>(pSrcn)[x], 
            reinterpret_cast<const Word*>(pSrcn)[x+rightOffset], 
            maxDeviation);
    }
}

template<class T>
struct MorphologicProcessor {
    typedef void (ProcessLineC)(T *pDst, const T *pSrcp, const T *pSrc, const T *pSrcn, int maxDeviation, int width, int height, ptrdiff_t srcPitch, ptrdiff_t dstPitch, int nOrigHeight);

    template<ProcessLineC process_line_left, ProcessLineC process_line, ProcessLineC process_line_right>
    static void generic_16_c(T *pDst, ptrdiff_t nDstPitch, const T *pSrc, ptrdiff_t nSrcPitch, int nMaxDeviation, const int *pCoordinates, int nCoordinates, int nWidth, int nHeight, int nOrigHeight)
    {
        const T *pSrcp = pSrc - nSrcPitch;
        const T *pSrcn = pSrc + nSrcPitch;


        UNUSED(nCoordinates); UNUSED(pCoordinates);

        /* top-left */
        process_line_left(pDst, pSrc, pSrc, pSrcn, nMaxDeviation, 1, nHeight, nSrcPitch, nDstPitch, nOrigHeight);

        /* top */
        process_line(pDst + 1, pSrc+1, pSrc+1, pSrcn+1, nMaxDeviation, nWidth - 2, nHeight, nSrcPitch, nDstPitch, nOrigHeight);

        /* top-right */
        process_line_right(pDst + nWidth - 1, pSrc + nWidth - 1, pSrc + nWidth - 1, pSrcn + nWidth - 1, nMaxDeviation, 1, nHeight, nSrcPitch, nDstPitch, nOrigHeight);

        pDst  += nDstPitch;
        pSrcp += nSrcPitch;
        pSrc  += nSrcPitch;
        pSrcn += nSrcPitch;

        for ( int y = 1; y < nHeight - 1; y++ )
        {
            /* left */
            process_line_left(pDst, pSrcp, pSrc, pSrcn, nMaxDeviation, 1, nHeight, nSrcPitch, nDstPitch, nOrigHeight);
            /* center */
            process_line(pDst + 1, pSrcp+1, pSrc+1, pSrcn+1, nMaxDeviation, nWidth - 2, nHeight, nSrcPitch, nDstPitch, nOrigHeight);
            /* right */
            process_line_right(pDst + nWidth - 1, pSrcp + nWidth - 1, pSrc + nWidth - 1, pSrcn + nWidth - 1, nMaxDeviation, 1, nHeight, nSrcPitch, nDstPitch, nOrigHeight);

            pDst  += nDstPitch;
            pSrcp += nSrcPitch;
            pSrc  += nSrcPitch;
            pSrcn += nSrcPitch;
        }

        /* bottom-left */
        process_line_left(pDst, pSrcp, pSrc, pSrc, nMaxDeviation, 1, nHeight, nSrcPitch, nDstPitch, nOrigHeight);
        /* bottom */
        process_line(pDst + 1, pSrcp+1, pSrc+1, pSrc+1, nMaxDeviation, nWidth - 2, nHeight, nSrcPitch, nDstPitch, nOrigHeight);
        /* bottom-right */
        process_line_right(pDst + nWidth - 1, pSrcp + nWidth - 1, pSrc + nWidth - 1, pSrc + nWidth - 1, nMaxDeviation, 1, nHeight, nSrcPitch, nDstPitch, nOrigHeight);
    }
};

//-------------------------extern "C" ----------------------------------------
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("sse4.1")))
#endif
static MT_FORCEINLINE __m128i limit_up_sse4_16(__m128i source, __m128i sum, __m128i deviation) {
  auto limit = _mm_adds_epu16(source, deviation);
  return _mm_min_epu16(limit, _mm_max_epu16(source, sum));
}

#if defined(GCC) || defined(CLANG)
__attribute__((__target__("sse4.1")))
#endif
static MT_FORCEINLINE __m128i limit_down_sse4_16(__m128i source, __m128i sum, __m128i deviation) {
  auto limit = _mm_subs_epu16(source, deviation);
  return _mm_max_epu16(limit, _mm_min_epu16(source, sum));
}


template<Border borderMode, Limit16 limit, MemoryMode mem_mode>
static void process_line_xxflate_16(Byte *pDst, const Byte *pSrcp, const Byte *pSrc, const Byte *pSrcn, const __m128i &maxDeviation, int byte_width) {
  auto zero = _mm_setzero_si128();
  for (int x = 0; x < byte_width; x += 16) {
    auto up_left = load16_one_to_left<borderMode, mem_mode>(pSrcp + x);
    auto up_center = simd_load_si128<mem_mode>(pSrcp + x);
    auto up_right = load16_one_to_right<borderMode, mem_mode>(pSrcp + x);

    auto middle_left = load16_one_to_left<borderMode, mem_mode>(pSrc + x);
    auto middle_right = load16_one_to_right<borderMode, mem_mode>(pSrc + x);

    auto down_left = load16_one_to_left<borderMode, mem_mode>(pSrcn + x);
    auto down_center = simd_load_si128<mem_mode>(pSrcn + x);
    auto down_right = load16_one_to_right<borderMode, mem_mode>(pSrcn + x);

    auto up_left_lo = _mm_unpacklo_epi16(up_left, zero);
    auto up_left_hi = _mm_unpackhi_epi16(up_left, zero);

    auto up_center_lo = _mm_unpacklo_epi16(up_center, zero);
    auto up_center_hi = _mm_unpackhi_epi16(up_center, zero);

    auto up_right_lo = _mm_unpacklo_epi16(up_right, zero);
    auto up_right_hi = _mm_unpackhi_epi16(up_right, zero);

    auto middle_left_lo = _mm_unpacklo_epi16(middle_left, zero);
    auto middle_left_hi = _mm_unpackhi_epi16(middle_left, zero);

    auto middle_right_lo = _mm_unpacklo_epi16(middle_right, zero);
    auto middle_right_hi = _mm_unpackhi_epi16(middle_right, zero);

    auto down_left_lo = _mm_unpacklo_epi16(down_left, zero);
    auto down_left_hi = _mm_unpackhi_epi16(down_left, zero);

    auto down_center_lo = _mm_unpacklo_epi16(down_center, zero);
    auto down_center_hi = _mm_unpackhi_epi16(down_center, zero);

    auto down_right_lo = _mm_unpacklo_epi16(down_right, zero);
    auto down_right_hi = _mm_unpackhi_epi16(down_right, zero);

    auto sum_lo = _mm_add_epi32(up_left_lo, up_center_lo);
    sum_lo = _mm_add_epi32(sum_lo, up_right_lo);
    sum_lo = _mm_add_epi32(sum_lo, middle_left_lo);
    sum_lo = _mm_add_epi32(sum_lo, middle_right_lo);
    sum_lo = _mm_add_epi32(sum_lo, down_left_lo);
    sum_lo = _mm_add_epi32(sum_lo, down_center_lo);
    sum_lo = _mm_add_epi32(sum_lo, down_right_lo);

    auto sum_hi = _mm_add_epi32(up_left_hi, up_center_hi);
    sum_hi = _mm_add_epi32(sum_hi, up_right_hi);
    sum_hi = _mm_add_epi32(sum_hi, middle_left_hi);
    sum_hi = _mm_add_epi32(sum_hi, middle_right_hi);
    sum_hi = _mm_add_epi32(sum_hi, down_left_hi);
    sum_hi = _mm_add_epi32(sum_hi, down_center_hi);
    sum_hi = _mm_add_epi32(sum_hi, down_right_hi);

    sum_lo = _mm_srai_epi32(sum_lo, 3);
    sum_hi = _mm_srai_epi32(sum_hi, 3);

    auto result = _mm_packus_epi32(sum_lo, sum_hi);

    auto middle_center = simd_load_si128<mem_mode>(pSrc + x);

    result = limit(middle_center, result, maxDeviation); // ??? bits_per_pixel template?

    simd_store_si128<mem_mode>(pDst + x, result);
  }
}

template<Border borderMode, decltype(_mm_max_epu16) op, Limit16 limit, MemoryMode mem_mode>
static MT_FORCEINLINE void process_line_xxpand_both_16(Byte *pDst, const Byte *pSrcp, const Byte *pSrc, const Byte *pSrcn, const __m128i &maxDeviation, int byte_width) {
  for (int x = 0; x < byte_width; x += 16) {
    __m128i up_center = simd_load_si128<mem_mode>(pSrcp + x);
    __m128i middle_center = simd_load_si128<mem_mode>(pSrc + x);
    __m128i down_center = simd_load_si128<mem_mode>(pSrcn + x);
    __m128i middle_left = load16_one_to_left<borderMode, mem_mode>(pSrc + x);
    __m128i middle_right = load16_one_to_right<borderMode, mem_mode>(pSrc + x);
    __m128i acc = op(up_center, middle_left);
    acc = op(acc, middle_right);
    acc = op(acc, down_center);

    __m128i result = limit(middle_center, acc, maxDeviation);
    simd_store_si128<mem_mode>(pDst + x, result);
  }
}

/* Vertical mt_xxpand */

template<decltype(_mm_max_epu16) op, Limit16 limit, MemoryMode mem_mode>
static MT_FORCEINLINE void process_line_xxpand_vertical_16(Byte *pDst, const Byte *pSrcp, const Byte *pSrc, const Byte *pSrcn, const __m128i &maxDeviation, int byte_width) {
  for (int x = 0; x < byte_width; x += 16) {
    auto up_center = simd_load_si128<mem_mode>(pSrcp + x);
    auto down_center = simd_load_si128<mem_mode>(pSrcn + x);
    auto middle_center = simd_load_si128<mem_mode>(pSrc + x);
    auto acc = op(up_center, down_center);
    auto result = limit(middle_center, acc, maxDeviation);
    simd_store_si128<mem_mode>(pDst + x, result);
  }
}

template<decltype(_mm_max_epu16) op, Limit16 limit, MemoryMode mem_mode>
static void xxpand_sse4_vertical_16(Word *pDst16, ptrdiff_t nDstPitch, const Word *pSrc16, ptrdiff_t nSrcPitch, int nMaxDeviation, const int *pCoordinates, int nCoordinates, int nWidth, int nHeight, int nOrigHeight) {
  // back to byte level
  const BYTE* pSrc = reinterpret_cast<const BYTE *>(pSrc16);
  BYTE* pDst = reinterpret_cast<BYTE *>(pDst16);
  nSrcPitch *= sizeof(uint16_t);
  nDstPitch *= sizeof(uint16_t);

  const Byte *pSrcp = pSrc - nSrcPitch;
  const Byte *pSrcn = pSrc + nSrcPitch;

  UNUSED(nCoordinates); UNUSED(pCoordinates); UNUSED(nOrigHeight);
  int byte_width = nWidth * sizeof(uint16_t);
  auto max_dev_v = _mm_set1_epi16(Word(nMaxDeviation));
  int mod16_width = byte_width / 16 * 16;
  bool not_mod16 = byte_width != mod16_width;

  process_line_xxpand_vertical_16<op, limit, mem_mode>(pDst, pSrc, pSrc, pSrcn, max_dev_v, mod16_width);

  if (not_mod16) {
    process_line_xxpand_vertical_16<op, limit, MemoryMode::SSE2_UNALIGNED>(pDst + byte_width - 16, pSrc + byte_width - 16, pSrc + byte_width - 16, pSrcn + byte_width - 16, max_dev_v, 16);
  }

  pDst += nDstPitch;
  pSrcp += nSrcPitch;
  pSrc += nSrcPitch;
  pSrcn += nSrcPitch;

  for (int y = 1; y < nHeight - 1; y++)
  {
    process_line_xxpand_vertical_16<op, limit, mem_mode>(pDst, pSrcp, pSrc, pSrcn, max_dev_v, mod16_width);

    if (not_mod16) {
      process_line_xxpand_vertical_16<op, limit, MemoryMode::SSE2_UNALIGNED>(pDst + byte_width - 16, pSrcp + byte_width - 16, pSrc + byte_width - 16, pSrcn + byte_width - 16, max_dev_v, 16);
    }

    pDst += nDstPitch;
    pSrcp += nSrcPitch;
    pSrc += nSrcPitch;
    pSrcn += nSrcPitch;
  }

  process_line_xxpand_vertical_16<op, limit, mem_mode>(pDst, pSrcp, pSrc, pSrc, max_dev_v, mod16_width);

  if (not_mod16) {
    process_line_xxpand_vertical_16<op, limit, MemoryMode::SSE2_UNALIGNED>(pDst + byte_width - 16, pSrcp + byte_width - 16, pSrc + byte_width - 16, pSrc + byte_width - 16, max_dev_v, 16);
  }
}


/* Horizontal mt_xxpand */

static MT_FORCEINLINE Word expand_c_horizontal_core_16(Word left, Word center, Word right, Word max_dev) {
  Word ma = left;

  if (center > ma) ma = center;
  if (right > ma) ma = right;

  if (ma - center > max_dev) ma = center + max_dev;
  return static_cast<Word>(ma); // todo bits_per_pixel template????
}

static MT_FORCEINLINE Word inpand_c_horizontal_core_16(Word left, Word center, Word right, Word max_dev) {
  Word mi = left;

  if (center < mi) mi = center;
  if (right < mi) mi = right;

  if (center - mi > max_dev) mi = center - max_dev;
  return static_cast<Word>(mi);
}

//this implemented in a somewhat retarded way to allow pDst and pSrc be the same pointer (used in 3x3 expand)
template<decltype(_mm_max_epu16) op, Limit16 limit, MemoryMode mem_mode, decltype(expand_c_horizontal_core_16) c_core>
static void xxpand_sse4_horizontal_16(Word *pDst16, ptrdiff_t nDstPitch, const Word *pSrc16, ptrdiff_t nSrcPitch, int nMaxDeviation, const int *pCoordinates, int nCoordinates, int nWidth, int nHeight, int nOrigHeight) {
  // back to byte level
  const BYTE* pSrc = reinterpret_cast<const BYTE *>(pSrc16);
  BYTE* pDst = reinterpret_cast<BYTE *>(pDst16);
  nSrcPitch *= sizeof(uint16_t);
  nDstPitch *= sizeof(uint16_t);

  UNUSED(nCoordinates); UNUSED(pCoordinates); UNUSED(nOrigHeight);
  const int byte_width = nWidth * sizeof(uint16_t);
  int mod16_width = (byte_width / 16) * 16;
  int sse_loop_limit = byte_width == mod16_width ? mod16_width - 16 : mod16_width;

  Word max_dev = Word(nMaxDeviation);
  __m128i max_dev_v = _mm_set1_epi16(max_dev);
  __m128i left_mask = _mm_set_epi32(0, 0, 0, 0xFFFF);
#pragma warning(disable: 4309)
  __m128i right_mask = _mm_set_epi16(0xFFFF, 0, 0, 0, 0, 0, 0, 0);
#pragma warning(default: 4309)

  __m128i left;

  for (int y = 0; y < nHeight; ++y) {
    //left border
    __m128i center = simd_load_si128<mem_mode>(pSrc);
    __m128i right = simd_load_si128<MemoryMode::SSE2_UNALIGNED>(pSrc + sizeof(uint16_t)); // +2: word
    left = _mm_or_si128(_mm_and_si128(center, left_mask), _mm_slli_si128(center, sizeof(uint16_t))); // shift 2: word

    __m128i result = op(left, right);
    result = limit(center, result, max_dev_v);

    left = simd_load_si128<MemoryMode::SSE2_UNALIGNED>(pSrc + 16- sizeof(uint16_t));
    simd_store_si128<mem_mode>(pDst, result);

    //main processing loop
    for (int x = 16; x < sse_loop_limit; x += 16) {
      center = simd_load_si128<mem_mode>(pSrc + x);
      right = simd_load_si128<MemoryMode::SSE2_UNALIGNED>(pSrc + x + sizeof(uint16_t));

      result = op(left, right);
      result = limit(center, result, max_dev_v);

      left = simd_load_si128<MemoryMode::SSE2_UNALIGNED>(pSrc + x + 16-sizeof(uint16_t));

      simd_store_si128<mem_mode>(pDst + x, result);
    }

    //right border
    if (mod16_width == byte_width) {
      center = simd_load_si128<mem_mode>(pSrc + mod16_width - 16);
      right = _mm_or_si128(_mm_and_si128(center, right_mask), _mm_srli_si128(center, sizeof(uint16_t)));

      result = op(left, right);
      result = limit(center, result, max_dev_v);

      simd_store_si128<mem_mode>(pDst + mod16_width - 16, result);
    }
    else { //some stuff left
      Word l = _mm_cvtsi128_si32(left) & 0xFFFF;

      int x;
      for (x = mod16_width / sizeof(uint16_t); x < nWidth - 1; ++x) {
        Word temp = c_core(l, reinterpret_cast<const uint16_t *>(pSrc)[x], reinterpret_cast<const uint16_t *>(pSrc)[x + 1], max_dev);
        l = reinterpret_cast<const uint16_t *>(pSrc)[x];
        reinterpret_cast<uint16_t *>(pDst)[x] = temp;
      }
      reinterpret_cast<uint16_t *>(pDst)[x] = c_core(l, reinterpret_cast<const uint16_t *>(pSrc)[x], reinterpret_cast<const uint16_t *>(pSrc)[x], max_dev);
    }
    pSrc += nSrcPitch;
    pDst += nDstPitch;
  }
}

/* Square mt_xxpand */

template<decltype(_mm_max_epu16) op, Limit16 limit, MemoryMode mem_mode, decltype(expand_c_horizontal_core_16) c_core>
static void xxpand_sse4_square_16(Word *pDst, ptrdiff_t nDstPitch, const Word *pSrc, ptrdiff_t nSrcPitch, int nMaxDeviation, const int *pCoordinates, int nCoordinates, int nWidth, int nHeight, int nOrigHeight) {
  xxpand_sse4_vertical_16<op, limit, mem_mode>(pDst, nDstPitch, pSrc, nSrcPitch, nMaxDeviation, pCoordinates, nCoordinates, nWidth, nHeight, nOrigHeight);
  xxpand_sse4_horizontal_16<op, limit, mem_mode, c_core>(pDst, nDstPitch, pDst, nDstPitch, nMaxDeviation, pCoordinates, nCoordinates, nWidth, nHeight, nOrigHeight);
}


template<ProcessLine16Sse4 process_line_left, ProcessLine16Sse4 process_line, ProcessLine16Sse4 process_line_right>
static void generic_sse4_16(Word *pDst16, ptrdiff_t nDstPitch, const Word *pSrc16, ptrdiff_t nSrcPitch, int nMaxDeviation, const int *pCoordinates, int nCoordinates, int nWidth, int nHeight, int nOrigHeight) {
  // back to byte level
  const BYTE* pSrc = reinterpret_cast<const BYTE *>(pSrc16);
  BYTE* pDst = reinterpret_cast<BYTE *>(pDst16);
  nSrcPitch *= sizeof(uint16_t);
  nDstPitch *= sizeof(uint16_t);

  const Byte *pSrcp = pSrc - nSrcPitch;
  const Byte *pSrcn = pSrc + nSrcPitch;

  UNUSED(nCoordinates); UNUSED(pCoordinates); UNUSED(nOrigHeight);
  auto max_dev_v = _mm_set1_epi16(Word(nMaxDeviation));
  int byte_width = nWidth * sizeof(uint16_t);
  int mod16_width = (byte_width - sizeof(uint16_t) - 16) / 16 * 16 + 16;
  /* top-left */
  process_line_left(pDst, pSrc, pSrc, pSrcn, max_dev_v, 16);
  /* top */
  process_line(pDst + 16, pSrc + 16, pSrc + 16, pSrcn + 16, max_dev_v, mod16_width - 16);

  /* top-right */
  process_line_right(pDst + byte_width - 16, pSrc + byte_width - 16, pSrc + byte_width - 16, pSrcn + byte_width - 16, max_dev_v, 16);

  pDst += nDstPitch;
  pSrcp += nSrcPitch;
  pSrc += nSrcPitch;
  pSrcn += nSrcPitch;

  for (int y = 1; y < nHeight - 1; y++)
  {
    /* left */
    process_line_left(pDst, pSrcp, pSrc, pSrcn, max_dev_v, 16);
    /* center */
    process_line(pDst + 16, pSrcp + 16, pSrc + 16, pSrcn + 16, max_dev_v, mod16_width - 16);
    /* right */
    process_line_right(pDst + byte_width - 16, pSrcp + byte_width - 16, pSrc + byte_width - 16, pSrcn + byte_width - 16, max_dev_v, 16);

    pDst += nDstPitch;
    pSrcp += nSrcPitch;
    pSrc += nSrcPitch;
    pSrcn += nSrcPitch;
  }

  /* bottom-left */
  process_line_left(pDst, pSrcp, pSrc, pSrc, max_dev_v, 16);
  /* bottom */
  process_line(pDst + 16, pSrcp + 16, pSrc + 16, pSrc + 16, max_dev_v, mod16_width - 16);
  /* bottom-right */
  process_line_right(pDst + byte_width - 16, pSrcp + byte_width - 16, pSrc + byte_width - 16, pSrc + byte_width - 16, max_dev_v, 16);
}
//-----------------------------------------------------------------


template<class T>
void generic_custom_stacked_c(Byte *pDst, ptrdiff_t nDstPitch, const Byte *pSrc, ptrdiff_t nSrcPitch, int nMaxDeviation, const int *pCoordinates, int nCoordinates, int nWidth, int nHeight, int nOrigHeight)
{
    Byte *pDstLsb = pDst + nOrigHeight * nDstPitch / 2;
    const Byte *pSrcLsb = pSrc + nOrigHeight * nDstPitch / 2;

    for ( int j = 0; j < nHeight; j++ )
    {
        for ( int i = 0; i < nWidth; i++ )
        {
            T new_value(read_word_stacked(pSrc, pSrcLsb, i), nMaxDeviation);
            for ( int k = 0; k < nCoordinates; k+=2 )
            {
                if ( pCoordinates[k] + i >= 0 && pCoordinates[k] + i < nWidth &&
                    pCoordinates[k+1] + j >= 0 && pCoordinates[k+1] + j < nHeight ) {
                  new_value.add(read_word_stacked(pSrc, pSrcLsb, i + pCoordinates[k] + pCoordinates[k+1] * static_cast<int>(nSrcPitch)));
                }
            }
            write_word_stacked(pDst, pDstLsb, i, new_value.finalize());
        }
        pSrc += nSrcPitch;
        pSrcLsb += nSrcPitch;
        pDst += nDstPitch;
        pDstLsb += nDstPitch;
    }
}


template<class T>
void generic_custom_16_c(Word *pDst, ptrdiff_t nDstPitch, const Word *pSrc, ptrdiff_t nSrcPitch, int nMaxDeviation, const int *pCoordinates, int nCoordinates, int nWidth, int nHeight, int nOrigHeight)
{
    UNUSED(nOrigHeight);

    for ( int j = 0; j < nHeight; j++ )
    {
        for ( int i = 0; i < nWidth; i++ )
        {
            T new_value(pSrc[i], nMaxDeviation);
            for ( int k = 0; k < nCoordinates; k+=2 )
            {
                if ( pCoordinates[k] + i >= 0 && pCoordinates[k] + i < nWidth &&
                    pCoordinates[k+1] + j >= 0 && pCoordinates[k+1] + j < nHeight ) {
                        new_value.add(pSrc[i + pCoordinates[k] + pCoordinates[k+1] * nSrcPitch]);
                }
            }
            pDst[i] = new_value.finalize();
        }
        pSrc += nSrcPitch;
        pDst += nDstPitch;
    }
}


} } } } // namespace Morphologic, Filters, MaskTools, Filtering

#endif