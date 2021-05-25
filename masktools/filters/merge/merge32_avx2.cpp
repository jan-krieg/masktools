#include "merge.h"
#include "../../common/simd_avx2.h"

namespace Filtering { namespace MaskTools { namespace Filters { namespace Merge {

   static void merge32_avx2_c(Byte *pDst, ptrdiff_t nDstPitch, const Byte *pSrc1, ptrdiff_t nSrc1Pitch,
      const Byte *pMask, ptrdiff_t nSrc2Pitch, int nWidth, int nHeight)
   {
      for (int y = 0; y < nHeight; ++y)
      {
         for (int x = 0; x < nWidth; ++x)
            reinterpret_cast<float *>(pDst)[x] = (1.0f - reinterpret_cast<const float *>(pMask)[x]) * reinterpret_cast<float *>(pDst)[x] + 
            reinterpret_cast<const float *>(pMask)[x] * reinterpret_cast<const float *>(pSrc1)[x];
         pDst += nDstPitch;
         pSrc1 += nSrc1Pitch;
         pMask += nSrc2Pitch;
      }
   }

   static void merge32_luma_420_avx2_c(Byte *pDst, ptrdiff_t nDstPitch, const Byte *pSrc1, ptrdiff_t nSrc1Pitch,
      const Byte *pMask, ptrdiff_t nSrc2Pitch, int nWidth, int nHeight)
   {
      for (int y = 0; y < nHeight; ++y)
      {
         for (int x = 0; x < nWidth; ++x)
         {
           // 420: both width and height is halved, averaging from 4 pixels of full size mask
            const float nMask = 0.25f * (reinterpret_cast<const float *>(pMask)[x * 2] + reinterpret_cast<const float *>(pMask + nSrc2Pitch)[x * 2] +
                                         reinterpret_cast<const float *>(pMask)[x * 2 + 1] + reinterpret_cast<const float *>(pMask + nSrc2Pitch)[x * 2 + 1]);
            reinterpret_cast<float *>(pDst)[x] = (1.0f - nMask) * reinterpret_cast<float *>(pDst)[x] + nMask * reinterpret_cast<const float *>(pSrc1)[x];
         }
         pDst += nDstPitch;
         pSrc1 += nSrc1Pitch;
         pMask += nSrc2Pitch * 2;
      }
   }

   template<bool allow_leftminus1>
   static void internal_merge32_luma_420_mpeg2_avx2_c(Byte *pDst, ptrdiff_t nDstPitch, const Byte *pSrc1, ptrdiff_t nSrc1Pitch,
     const Byte *pMask, ptrdiff_t nSrc2Pitch, int nWidth, int nHeight)
   {
     for (int y = 0; y < nHeight; ++y)
     {
       float right;
       right = allow_leftminus1 ?
         reinterpret_cast<const float*>(pMask)[-1] + reinterpret_cast<const float*>(pMask + nSrc2Pitch)[-1] :
         reinterpret_cast<const float*>(pMask)[0] + reinterpret_cast<const float*>(pMask + nSrc2Pitch)[0];

       for (int x = 0; x < nWidth; ++x)
       {
         float left = right;
         const float mid = reinterpret_cast<const float *>(pMask)[x * 2] + reinterpret_cast<const float *>(pMask + nSrc2Pitch)[x * 2];
         right = reinterpret_cast<const float *>(pMask)[x * 2 + 1] + reinterpret_cast<const float *>(pMask + nSrc2Pitch)[x * 2 + 1];

         const float nMask = 0.125f * (left + 2 * mid + right);
         // 420: both width and height is halved, 1-2-1 weighted averaging from 6 pixels of full size mask

         reinterpret_cast<float *>(pDst)[x] = (1.0f - nMask) * reinterpret_cast<float *>(pDst)[x] + nMask * reinterpret_cast<const float *>(pSrc1)[x];
       }
       pDst += nDstPitch;
       pSrc1 += nSrc1Pitch;
       pMask += nSrc2Pitch * 2;
     }
   }

   static void merge32_luma_420_mpeg2_avx2_c(Byte *pDst, ptrdiff_t nDstPitch, const Byte *pSrc1, ptrdiff_t nSrc1Pitch,
     const Byte *pMask, ptrdiff_t nSrc2Pitch, int nWidth, int nHeight)
   {
     internal_merge32_luma_420_mpeg2_avx2_c<false>(pDst, nDstPitch, pSrc1, nSrc1Pitch, pMask, nSrc2Pitch, nWidth, nHeight);
   }

   static void merge32_luma_420_mpeg2_allow_leftminus1_avx2_c(Byte *pDst, ptrdiff_t nDstPitch, const Byte *pSrc1, ptrdiff_t nSrc1Pitch,
     const Byte *pMask, ptrdiff_t nSrc2Pitch, int nWidth, int nHeight)
   {
     internal_merge32_luma_420_mpeg2_avx2_c<true>(pDst, nDstPitch, pSrc1, nSrc1Pitch, pMask, nSrc2Pitch, nWidth, nHeight);
   }

   static void merge32_luma_422_avx2_c(Byte *pDst, ptrdiff_t nDstPitch, const Byte *pSrc1, ptrdiff_t nSrc1Pitch,
     const Byte *pMask, ptrdiff_t nSrc2Pitch, int nWidth, int nHeight)
   {
     for (int y = 0; y < nHeight; ++y)
     {
       for (int x = 0; x < nWidth; ++x)
       {
         const float nMask = 0.5f * (reinterpret_cast<const float *>(pMask)[x * 2] + reinterpret_cast<const float *>(pMask)[x * 2 + 1]); // 422: only width is halved, averaging from two pixels of full size mask
         reinterpret_cast<float *>(pDst)[x] = (1.0f - nMask) * reinterpret_cast<float *>(pDst)[x] + nMask * reinterpret_cast<const float *>(pSrc1)[x];
       }
       pDst += nDstPitch;
       pSrc1 += nSrc1Pitch;
       pMask += nSrc2Pitch;
     }
   }

   template<bool allow_leftminus1>
   static void internal_merge32_luma_422_mpeg2_avx2_c(Byte *pDst, ptrdiff_t nDstPitch, const Byte *pSrc1, ptrdiff_t nSrc1Pitch,
     const Byte *pMask, ptrdiff_t nSrc2Pitch, int nWidth, int nHeight)
   {
     for (int y = 0; y < nHeight; ++y)
     {
       float right;
       right = allow_leftminus1 ?
         reinterpret_cast<const float*>(pMask)[-1]:
         reinterpret_cast<const float*>(pMask)[0];

       for (int x = 0; x < nWidth; ++x)
       {
         float left = right;
         const float mid = reinterpret_cast<const float *>(pMask)[x * 2];
         right = reinterpret_cast<const float *>(pMask)[x * 2 + 1];

         const float nMask = 0.25f * (left + 2 * mid + right);
         // 422: width is halved, 1-2-1 weighted averaging from 3 pixels of full size mask

         reinterpret_cast<float *>(pDst)[x] = (1.0f - nMask) * reinterpret_cast<float *>(pDst)[x] + nMask * reinterpret_cast<const float *>(pSrc1)[x];
       }
       pDst += nDstPitch;
       pSrc1 += nSrc1Pitch;
       pMask += nSrc2Pitch;
     }
   }

   static void merge32_luma_422_mpeg2_avx2_c(Byte *pDst, ptrdiff_t nDstPitch, const Byte *pSrc1, ptrdiff_t nSrc1Pitch,
     const Byte *pMask, ptrdiff_t nSrc2Pitch, int nWidth, int nHeight)
   {
     internal_merge32_luma_422_mpeg2_avx2_c<false>(pDst, nDstPitch, pSrc1, nSrc1Pitch, pMask, nSrc2Pitch, nWidth, nHeight);
   }

   static void merge32_luma_422_mpeg2_allow_leftminus1_avx2_c(Byte *pDst, ptrdiff_t nDstPitch, const Byte *pSrc1, ptrdiff_t nSrc1Pitch,
     const Byte *pMask, ptrdiff_t nSrc2Pitch, int nWidth, int nHeight)
   {
     internal_merge32_luma_422_mpeg2_avx2_c<true>(pDst, nDstPitch, pSrc1, nSrc1Pitch, pMask, nSrc2Pitch, nWidth, nHeight);
   }


   template <MemoryMode mem_mode>
   MT_FORCEINLINE __m256 merge32_avx2_core(Byte *pDst, const Byte *pSrc, const __m256& mask) {
      // ( 1- mask) * dst + mask * src = 
      // dst - dst * mask + src * mask = 
      // dst + mask * (src - dst)
      auto dst = simd256_load_ps<mem_mode>(pDst);
      auto src = simd256_load_ps<mem_mode>(pSrc);
      auto diff = _mm256_sub_ps(src, dst);
      auto tmp = _mm256_mul_ps(diff, mask); // (p2-p1)*mask
      return _mm256_add_ps(tmp, dst); // +dst
   }

   template <MemoryMode mem_mode>
   void merge32_avx2_t(Byte *pDst, ptrdiff_t nDstPitch, const Byte *pSrc1, ptrdiff_t nSrc1Pitch,
      const Byte *pMask, ptrdiff_t nSrc2Pitch, int nWidth, int nHeight)
   {
      nWidth *= sizeof(Float);

      int wMod32 = (nWidth / 32) * 32;
      auto pDst_s = pDst;
      auto pSrc1_s = pSrc1;
      auto pMask_s = pMask;
      for (int j = 0; j < nHeight; ++j) {
         for (int i = 0; i < wMod32; i += 32) {
            auto mask = simd256_load_ps<mem_mode>(pMask + i);
            auto result = merge32_avx2_core<mem_mode>(pDst + i, pSrc1 + i, mask);
            simd256_store_ps<mem_mode>(pDst + i, result);
         }
         pDst += nDstPitch;
         pSrc1 += nSrc1Pitch;
         pMask += nSrc2Pitch;
      }

      if (nWidth > wMod32) {
         merge32_avx2_c(pDst_s + wMod32, nDstPitch, pSrc1_s + wMod32, nSrc1Pitch, pMask_s + wMod32, nSrc2Pitch, (nWidth - wMod32) / sizeof(Float), nHeight);
      }
      _mm256_zeroupper();
   }

   template <MemoryMode mem_mode>
   void merge32_luma_420_avx2_t(Byte *pDst, ptrdiff_t nDstPitch, const Byte *pSrc1, ptrdiff_t nSrc1Pitch,
     const Byte *pMask, ptrdiff_t nSrc2Pitch, int nWidth, int nHeight)
   {
     nWidth *= sizeof(Float);

     int wMod32 = (nWidth / 32) * 32;
     auto pDst_s = pDst;
     auto pSrc1_s = pSrc1;
     auto pMask_s = pMask;
     auto vOneFourth = _mm256_set1_ps(0.25f);
     for (int j = 0; j < nHeight; ++j) {
       for (int i = 0; i < wMod32; i += 32) {
         // preparing mask
         auto src2_row1_t1 = simd256_load_ps<mem_mode>(pMask + i * 2);
         auto src2_row1_t2 = simd256_load_ps<mem_mode>(pMask + i * 2 + 32);
         auto src2_row2_t1 = simd256_load_ps<mem_mode>(pMask + nSrc2Pitch + i * 2);
         auto src2_row2_t2 = simd256_load_ps<mem_mode>(pMask + nSrc2Pitch + i * 2 + 32);

         // sum vertically
         auto avg_lo = _mm256_add_ps(src2_row1_t1, src2_row2_t1);
         auto avg_hi = _mm256_add_ps(src2_row1_t2, src2_row2_t2);

         // sum horizontally, 128bit lanes
         auto avg = _mm256_hadd_ps(avg_lo, avg_hi);
         // adjust across lanes
         avg = _mm256_castpd_ps(_mm256_permute4x64_pd(_mm256_castps_pd(avg), (0 << 0) + (2 << 2) + (1 << 4) + (3 << 6)));
         // make average
         auto mask = _mm256_mul_ps(avg, vOneFourth);
         
         auto result = merge32_avx2_core<mem_mode>(pDst + i, pSrc1 + i, mask);

         simd256_store_ps<mem_mode>(pDst + i, result);
       }
       pDst += nDstPitch;
       pSrc1 += nSrc1Pitch;
       pMask += nSrc2Pitch * 2;
     }
     if (nWidth > wMod32) {
       merge32_luma_420_avx2_c(pDst_s + wMod32, nDstPitch, pSrc1_s + wMod32, nSrc1Pitch, pMask_s + wMod32 * 2, nSrc2Pitch, (nWidth - wMod32) / sizeof(Float), nHeight);
     }
     _mm256_zeroupper();
   }

   template <MemoryMode mem_mode>
   void merge32_luma_420_mpeg2_avx2_t(Byte *pDst, ptrdiff_t nDstPitch, const Byte *pSrc1, ptrdiff_t nSrc1Pitch,
     const Byte *pMask, ptrdiff_t nSrc2Pitch, int nWidth, int nHeight)
   {
     nWidth *= sizeof(Float);

     int wMod32 = (nWidth / 32) * 32;
     auto pDst_s = pDst;
     auto pSrc1_s = pSrc1;
     auto pMask_s = pMask;
     auto vOneEight = _mm256_set1_ps(0.125f);
     for (int j = 0; j < nHeight; ++j) {
       auto row1 = simd256_load_ps<mem_mode>(pMask + 0 * 2);
       auto row2 = simd256_load_ps<mem_mode>(pMask + 0 * 2 + 16);
       // sum vertically
       auto right = _mm256_add_ps(row1, row2);
       // move lowest to highest, others n/a
       // adjust across lanes
       right = _mm256_castsi256_ps(_MM256_SLLI_SI256<32 - 4>(_mm256_castps_si256(right)));

       for (int i = 0; i < wMod32; i += 32) {
         // preparing mask
         auto row1_lo = simd256_load_ps<mem_mode>(pMask + i * 2);
         auto row1_hi = simd256_load_ps<mem_mode>(pMask + i * 2 + 32);
         auto row2_lo = simd256_load_ps<mem_mode>(pMask + nSrc2Pitch + i * 2);
         auto row2_hi = simd256_load_ps<mem_mode>(pMask + nSrc2Pitch + i * 2 + 32);

         // sum vertically
         auto tmp_lo = _mm256_add_ps(row1_lo, row2_lo);
         auto tmp_hi = _mm256_add_ps(row1_hi, row2_hi);

         // mask evens and collapse
         // L0 L1 L2 L3 L4 L5 L6 L7 H0 H1 H2 H3 H4 H5 H6 H7
         // L0 L2 H0 H2 L4 L6 H4 H6 // after _mm256_shuffle_ps(tmp_lo, tmp_hi, (0 << 0) | (2 << 2) | (0 << 4) | (2 << 6));
         // L0L2  L4L6  H0H2  H4H6  // after _mm256_permute4x64_pd(_mm256_castps_pd(mid), (0 << 0) + (2 << 2) + (1 << 4) + (3 << 6));
         // L0 L2 L4 L6 H0 H2 H4 H6 // needed
         auto mid = _mm256_shuffle_ps(tmp_lo, tmp_hi, (0 << 0) | (2 << 2) | (0 << 4) | (2 << 6));
         // adjust across lanes
         mid = _mm256_castpd_ps(_mm256_permute4x64_pd(_mm256_castps_pd(mid), (0 << 0) + (2 << 2) + (1 << 4) + (3 << 6)));

         // mask odds and collapse
         // L0 L1 L2 L3 L4 L5 L6 L7 H0 H1 H2 H3 H4 H5 H6 H7
         // L1 L3 H1 H3 L5 L7 H5 H7 // after _mm256_shuffle_ps(tmp_lo, tmp_hi, (1 << 0) | (3 << 2) | (1 << 4) | (3 << 6));
         // L1L3  L5L7  H1H3  H5H7  // after _mm256_permute4x64_pd(_mm256_castps_pd(right), (0 << 0) + (2 << 2) + (1 << 4) + (3 << 6));
         // L1 L3 L5 L7 H1 H3 H5 H7 // needed
         auto old_right = right;
         right = _mm256_shuffle_ps(tmp_lo, tmp_hi, (1 << 0) | (3 << 2) | (1 << 4) | (3 << 6));
         // adjust across lanes
         right = _mm256_castpd_ps(_mm256_permute4x64_pd(_mm256_castps_pd(right), (0 << 0) + (2 << 2) + (1 << 4) + (3 << 6)));

         auto left = _mm256_castsi256_ps(_mm256_or_si256(_MM256_SLLI_SI256<4>(_mm256_castps_si256(right)), _MM256_SRLI_SI256<32-4>(_mm256_castps_si256(old_right)))); // shift in from previous right

         auto mask = _mm256_mul_ps(_mm256_add_ps(_mm256_add_ps(_mm256_add_ps(mid, mid), left), right), vOneEight); // (left + mid*2 + right) / 8

         auto result = merge32_avx2_core<mem_mode>(pDst + i, pSrc1 + i, mask);

         simd256_store_ps<mem_mode>(pDst + i, result);
       }
       pDst += nDstPitch;
       pSrc1 += nSrc1Pitch;
       pMask += nSrc2Pitch * 2;
     }
     if (nWidth > wMod32) {
       // pMask offset: mask is not subsampled -> width*2
       if (wMod32 != 0)
         merge32_luma_420_mpeg2_allow_leftminus1_avx2_c(pDst_s + wMod32, nDstPitch, pSrc1_s + wMod32, nSrc1Pitch, pMask_s + wMod32 * 2, nSrc2Pitch, (nWidth - wMod32) / sizeof(Float), nHeight);
       else
         merge32_luma_420_mpeg2_avx2_c(pDst_s + wMod32, nDstPitch, pSrc1_s + wMod32, nSrc1Pitch, pMask_s + wMod32 * 2, nSrc2Pitch, (nWidth - wMod32) / sizeof(Float), nHeight);
     }
     _mm256_zeroupper();
   }

   template <MemoryMode mem_mode>
   void merge32_luma_422_avx2_t(Byte *pDst, ptrdiff_t nDstPitch, const Byte *pSrc1, ptrdiff_t nSrc1Pitch,
     const Byte *pMask, ptrdiff_t nSrc2Pitch, int nWidth, int nHeight)
   {
     nWidth *= sizeof(Float);

     int wMod32 = (nWidth / 32) * 32;
     auto pDst_s = pDst;
     auto pSrc1_s = pSrc1;
     auto pMask_s = pMask;
     auto vHalf = _mm256_set1_ps(0.5f);
     for (int j = 0; j < nHeight; ++j) {
       for (int i = 0; i < wMod32; i += 32) {
         _mm_prefetch(reinterpret_cast<const char*>(pMask) + i * 2 + 64, _MM_HINT_T0);
         // preparing mask
         auto mask_row_t1 = simd256_load_ps<mem_mode>(pMask + i * 2);
         auto mask_row_t2 = simd256_load_ps<mem_mode>(pMask + i * 2 + 32);
         // sum horizontally, 128bit lanes
         auto avg = _mm256_hadd_ps(mask_row_t1, mask_row_t2);
         // adjust across lanes
         avg = _mm256_castpd_ps(_mm256_permute4x64_pd(_mm256_castps_pd(avg), (0 << 0) + (2 << 2) + (1 << 4) + (3 << 6)));
         // make average
         auto mask = _mm256_mul_ps(avg, vHalf);

         auto result = merge32_avx2_core<mem_mode>(pDst + i, pSrc1 + i, mask);
         simd256_store_ps<mem_mode>(pDst + i, result);
       }
       pDst += nDstPitch;
       pSrc1 += nSrc1Pitch;
       pMask += nSrc2Pitch;
     }
     if (nWidth > wMod32) {
       merge32_luma_422_avx2_c(pDst_s + wMod32, nDstPitch, pSrc1_s + wMod32, nSrc1Pitch, pMask_s + wMod32 * 2, nSrc2Pitch, (nWidth - wMod32) / sizeof(Float), nHeight);
     }
     _mm256_zeroupper();
   }

   template <MemoryMode mem_mode>
   void merge32_luma_422_mpeg2_avx2_t(Byte *pDst, ptrdiff_t nDstPitch, const Byte *pSrc1, ptrdiff_t nSrc1Pitch,
     const Byte *pMask, ptrdiff_t nSrc2Pitch, int nWidth, int nHeight)
   {
     nWidth *= sizeof(Float);

     int wMod32 = (nWidth / 32) * 32;
     auto pDst_s = pDst;
     auto pSrc1_s = pSrc1;
     auto pMask_s = pMask;
     auto vOneFourth = _mm256_set1_ps(0.25f);
     for (int j = 0; j < nHeight; ++j) {
       auto row1 = simd256_load_ps<mem_mode>(pMask + 0 * 2);
       // move lowest to highest, others n/a
       // adjust across lanes
       auto right = _mm256_castsi256_ps(_MM256_SLLI_SI256<32 - 4>(_mm256_castps_si256(row1)));

       for (int i = 0; i < wMod32; i += 32) {
         // preparing mask
         auto row1_lo = simd256_load_ps<mem_mode>(pMask + i * 2);
         auto row1_hi = simd256_load_ps<mem_mode>(pMask + i * 2 + 32);

         // mask evens and collapse
         // L0 L1 L2 L3 L4 L5 L6 L7 H0 H1 H2 H3 H4 H5 H6 H7
         // L0 L2 H0 H2 L4 L6 H4 H6 // after _mm256_shuffle_ps(tmp_lo, tmp_hi, (0 << 0) | (2 << 2) | (0 << 4) | (2 << 6));
         // L0L2  L4L6  H0H2  H4H6  // after _mm256_permute4x64_pd(_mm256_castps_pd(mid), (0 << 0) + (2 << 2) + (1 << 4) + (3 << 6));
         // L0 L2 L4 L6 H0 H2 H4 H6 // needed
         auto mid = _mm256_shuffle_ps(row1_lo, row1_hi, (0 << 0) | (2 << 2) | (0 << 4) | (2 << 6));
         // adjust across lanes
         mid = _mm256_castpd_ps(_mm256_permute4x64_pd(_mm256_castps_pd(mid), (0 << 0) + (2 << 2) + (1 << 4) + (3 << 6)));

         // mask odds and collapse
         // L0 L1 L2 L3 L4 L5 L6 L7 H0 H1 H2 H3 H4 H5 H6 H7
         // L1 L3 H1 H3 L5 L7 H5 H7 // after _mm256_shuffle_ps(tmp_lo, tmp_hi, (1 << 0) | (3 << 2) | (1 << 4) | (3 << 6));
         // L1L3  L5L7  H1H3  H5H7  // after _mm256_permute4x64_pd(_mm256_castps_pd(right), (0 << 0) + (2 << 2) + (1 << 4) + (3 << 6));
         // L1 L3 L5 L7 H1 H3 H5 H7 // needed
         auto old_right = right;
         right = _mm256_shuffle_ps(row1_lo, row1_hi, (1 << 0) | (3 << 2) | (1 << 4) | (3 << 6));
         // adjust across lanes
         right = _mm256_castpd_ps(_mm256_permute4x64_pd(_mm256_castps_pd(right), (0 << 0) + (2 << 2) + (1 << 4) + (3 << 6)));

         auto left = _mm256_castsi256_ps(_mm256_or_si256(_MM256_SLLI_SI256<4>(_mm256_castps_si256(right)), _MM256_SRLI_SI256<32 - 4>(_mm256_castps_si256(old_right)))); // shift in from previous right

         auto mask = _mm256_mul_ps(_mm256_add_ps(_mm256_add_ps(_mm256_add_ps(mid, mid), left), right), vOneFourth); // (left + mid*2 + right) / 4

         auto result = merge32_avx2_core<mem_mode>(pDst + i, pSrc1 + i, mask);
         simd256_store_ps<mem_mode>(pDst + i, result);
       }
       pDst += nDstPitch;
       pSrc1 += nSrc1Pitch;
       pMask += nSrc2Pitch;
     }
     if (nWidth > wMod32) {
       // pMask offset: mask is not subsampled -> width*2
       if (wMod32 != 0)
         merge32_luma_422_mpeg2_allow_leftminus1_avx2_c(pDst_s + wMod32, nDstPitch, pSrc1_s + wMod32, nSrc1Pitch, pMask_s + wMod32 * 2, nSrc2Pitch, (nWidth - wMod32) / sizeof(Float), nHeight);
       else
         merge32_luma_422_mpeg2_avx2_c(pDst_s + wMod32, nDstPitch, pSrc1_s + wMod32, nSrc1Pitch, pMask_s + wMod32 * 2, nSrc2Pitch, (nWidth - wMod32) / sizeof(Float), nHeight);
     }
     _mm256_zeroupper();
   }


   Processor32 *merge32_avx2 = merge32_avx2_t<MemoryMode::SSE2_UNALIGNED>;
   Processor32 *merge32_luma_420_avx2 = merge32_luma_420_avx2_t<MemoryMode::SSE2_UNALIGNED>;
   Processor32 *merge32_luma_420_mpeg2_avx2 = merge32_luma_420_mpeg2_avx2_t<MemoryMode::SSE2_UNALIGNED>;
   Processor32 *merge32_luma_422_avx2 = merge32_luma_422_avx2_t<MemoryMode::SSE2_UNALIGNED>;
   Processor32 *merge32_luma_422_mpeg2_avx2 = merge32_luma_422_mpeg2_avx2_t<MemoryMode::SSE2_UNALIGNED>;

} } } }