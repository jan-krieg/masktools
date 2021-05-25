#include "edgemask.h"
#include "../functions32_avx.h"
#include "../../../common/simd_avx.h"

using namespace Filtering;

namespace Filtering { namespace MaskTools { namespace Filters { namespace Mask { namespace Edge {

// remark: for Float mask is 0.0 or 1.0, done in threshold<Float, Float>
/*
inline Float convolution(Float a11, Float a21, Float a31, Float a12, Float a22, Float a32, Float a13, Float a23, Float a33, const Float matrix[10], Float nLowThreshold, Float nHighThreshold)
{
   return threshold<Float, Float>(abs((a11 * matrix[0] + a21 * matrix[1] + a31 * matrix[2] + 
                                    a12 * matrix[3] + a22 * matrix[4] + a32 * matrix[5] +
                                    a13 * matrix[6] + a23 * matrix[7] + a33 * matrix[8]) / matrix[9]), nLowThreshold, nHighThreshold);
}

inline Float sobel(Float a11, Float a21, Float a31, Float a12, Float a22, Float a32, Float a13, Float a23, Float a33, const Float matrix[10], Float nLowThreshold, Float nHighThreshold)
{
   UNUSED(a11); UNUSED(a13); UNUSED(a22); UNUSED(a31); UNUSED(a33); UNUSED(matrix); 
   return threshold<Float, Float>(abs(a32 + a23 - a12 - a21 ) / 2.0f, nLowThreshold, nHighThreshold);
}

inline Float roberts(Float a11, Float a21, Float a31, Float a12, Float a22, Float a32, Float a13, Float a23, Float a33, const Float matrix[10], Float nLowThreshold, Float nHighThreshold)
{
   UNUSED(a11); UNUSED(a12); UNUSED(a13); UNUSED(a21); UNUSED(a31); UNUSED(a33); UNUSED(matrix); 
   return threshold<Float, Float>(abs( (a22 * 2.0f) - a32 - a23 ) / 2.0f, nLowThreshold, nHighThreshold);
}

inline Float laplace(Float a11, Float a21, Float a31, Float a12, Float a22, Float a32, Float a13, Float a23, Float a33, const Float matrix[10], Float nLowThreshold, Float nHighThreshold)
{
   UNUSED(matrix); 
   return threshold<Float, Float>(abs( (a22 * 8.0f) - a32 - a23 - a11 - a21 - a31 - a12 - a13 - a33 ) / 8.0f, nLowThreshold, nHighThreshold);
}

inline Float morpho(Float a11, Float a21, Float a31, Float a12, Float a22, Float a32, Float a13, Float a23, Float a33, const Float matrix[10], Float nLowThreshold, Float nHighThreshold)
{
   Float nMin = a11, nMax = a11;

   UNUSED(matrix); 

   nMin = min<Float>( nMin, a21 );
   nMax = max<Float>( nMax, a21 );
   nMin = min<Float>( nMin, a31 );
   nMax = max<Float>( nMax, a31 );
   nMin = min<Float>( nMin, a12 );
   nMax = max<Float>( nMax, a12 );
   nMin = min<Float>( nMin, a22 );
   nMax = max<Float>( nMax, a22 );
   nMin = min<Float>( nMin, a32 );
   nMax = max<Float>( nMax, a32 );
   nMin = min<Float>( nMin, a13 );
   nMax = max<Float>( nMax, a13 );
   nMin = min<Float>( nMin, a23 );
   nMax = max<Float>( nMax, a23 );
   nMin = min<Float>( nMin, a33 );
   nMax = max<Float>( nMax, a33 );

   return threshold<Float, Float>( nMax - nMin, nLowThreshold, nHighThreshold );
}

inline Float cartoon(Float a11, Float a21, Float a31, Float a12, Float a22, Float a32, Float a13, Float a23, Float a33, const Float matrix[10], Float nLowThreshold, Float nHighThreshold)
{
   Float val = (a21 * 2.0f) - a22 - a31;

   UNUSED(a11); UNUSED(a12); UNUSED(a13); UNUSED(a23); UNUSED(a32); UNUSED(a33); UNUSED(matrix); 

   return val > 0 ? 0 : threshold<Float, Float>( -val, nLowThreshold, nHighThreshold );
}

inline Float prewitt(Float a11, Float a21, Float a31, Float a12, Float a22, Float a32, Float a13, Float a23, Float a33, const Float matrix[10], Float nLowThreshold, Float nHighThreshold)
{
   const Float p90 = a11 + a21 + a31 - a13 - a23 - a33;
   const Float p180 = a11 + a12 + a13 - a31 - a32 - a33;
   const Float p45 = a12 + a11 + a21 - a33 - a32 - a23;
   const Float p135 = a13 + a12 + a23 - a31 - a32 - a21;

   const Float max1 = max<Float>( abs<Float>( p90 ), abs<Float>( p180 ) );
   const Float max2 = max<Float>( abs<Float>( p45 ), abs<Float>( p135 ) );
   const Float maxv = max<Float>( max1, max2 );

   UNUSED(a22); UNUSED(matrix); 

   return threshold<Float, Float>( maxv, nLowThreshold, nHighThreshold );
}

inline Float half_prewitt(Float a11, Float a21, Float a31, Float a12, Float a22, Float a32, Float a13, Float a23, Float a33, const Float matrix[10], Float nLowThreshold, Float nHighThreshold)
{
   const Float p90 = a11 + 2 * a21 + a31 - a13 - 2 * a23 - a33;
   const Float p180 = a11 + 2 * a12 + a13 - a31 - 2 * a32 - a33;
   const Float maxv = max<Float>( abs<Float>( p90 ), abs<Float>( p180 ) );

   UNUSED(a22); UNUSED(matrix);
   
   return threshold<Float, Float>( maxv, nLowThreshold, nHighThreshold );
}

class Thresholds {
   Float nMinThreshold, nMaxThreshold;
public:
   Thresholds(Float nMinThreshold, Float nMaxThreshold) :
   nMinThreshold(nMinThreshold), nMaxThreshold(nMaxThreshold)
   {
   }

   int minpitch() const { return 0; }
   int maxpitch() const { return 0; }
   void nextRow() { }
   Float min(int x) const { UNUSED(x); return nMinThreshold; }
   Float max(int x) const { UNUSED(x); return nMaxThreshold; }
};

template<Filters::Mask::Operator op>
void mask32_t(Float *pDst, ptrdiff_t nDstPitch, const Float *pSrc, ptrdiff_t nSrcPitch, const Float matrix[10], Float nLowThreshold, Float nHighThreshold, int nWidth, int nHeight)
{
   Thresholds thresholds(static_cast<Float>(nLowThreshold), static_cast<Float>(nHighThreshold));

   Filters::Mask::generic32_c<op, Thresholds>(pDst, nDstPitch, pSrc, nSrcPitch, thresholds, matrix, nWidth, nHeight);
}
*/

template<CpuFlags flags, Border borderMode, MemoryMode mem_mode>
static MT_FORCEINLINE void process_line_convolution_avx(Byte *pDst, const Byte *pSrcp, const Byte *pSrc, const Byte *pSrcn, const Float matrix[10], const __m256 &lowThresh, const __m256 &highThresh, int width) {
    UNUSED(pSrcp);
    auto coef0 = _mm256_set1_ps(matrix[0]);
    auto coef1 = _mm256_set1_ps(matrix[1]);
    auto coef2 = _mm256_set1_ps(matrix[2]);
    auto coef3 = _mm256_set1_ps(matrix[3]);
    auto coef4 = _mm256_set1_ps(matrix[4]);
    auto coef5 = _mm256_set1_ps(matrix[5]);
    auto coef6 = _mm256_set1_ps(matrix[6]);
    auto coef7 = _mm256_set1_ps(matrix[7]);
    auto coef8 = _mm256_set1_ps(matrix[8]);
    
    auto divisor = _mm256_set1_ps(1.0f/matrix[9]);

    for (int x = 0; x < width; x+=32) {
        auto up_left = load32_one_to_left_si256<borderMode, mem_mode>(pSrcp+x);
        auto up_center = simd256_load_ps<mem_mode>(pSrcp+x);
        auto up_right = load32_one_to_right_si256<borderMode, mem_mode>(pSrcp+x);

        auto middle_left = load32_one_to_left_si256<borderMode, mem_mode>(pSrc+x);
        auto middle_center = simd256_load_ps<mem_mode>(pSrc+x);
        auto middle_right = load32_one_to_right_si256<borderMode, mem_mode>(pSrc+x);

        auto down_left = load32_one_to_left_si256<borderMode, mem_mode>(pSrcn+x);
        auto down_center = simd256_load_ps<mem_mode>(pSrcn+x);
        auto down_right = load32_one_to_right_si256<borderMode, mem_mode>(pSrcn+x);

        auto acc = _mm256_mul_ps(up_left, coef0);
        acc = _mm256_add_ps(acc, _mm256_mul_ps(up_center, coef1));
        acc = _mm256_add_ps(acc, _mm256_mul_ps(up_right, coef2));
        acc = _mm256_add_ps(acc, _mm256_mul_ps(middle_left, coef3));
        acc = _mm256_add_ps(acc, _mm256_mul_ps(middle_center, coef4));
        acc = _mm256_add_ps(acc, _mm256_mul_ps(middle_right, coef5));
        acc = _mm256_add_ps(acc, _mm256_mul_ps(down_left, coef6));
        acc = _mm256_add_ps(acc, _mm256_mul_ps(down_center, coef7));
        acc = _mm256_add_ps(acc, _mm256_mul_ps(down_right, coef8));

        acc = simd256_abs_ps(acc);
        acc = _mm256_mul_ps(acc, divisor);

        auto result = threshold32_avx(acc, lowThresh, highThresh);

        simd256_store_ps<mem_mode>(pDst+x, result);
    }
}

template<CpuFlags flags, Border borderMode, MemoryMode mem_mode>
static MT_FORCEINLINE void process_line_sobel_avx(Byte *pDst, const Byte *pSrcp, const Byte *pSrc, const Byte *pSrcn, const Float matrix[10], const __m256 &lowThresh, const __m256 &highThresh, int width) {
    UNUSED(matrix);
    auto oneHalf = _mm256_set1_ps(0.5f);

    for (int x = 0; x < width; x+=32) {
        auto up_center = simd256_load_ps<mem_mode>(pSrcp+x);

        auto middle_left = load32_one_to_left_si256<borderMode, mem_mode>(pSrc+x);
        auto middle_right = load32_one_to_right_si256<borderMode, mem_mode>(pSrc+x);

        auto down_center = simd256_load_ps<mem_mode>(pSrcn+x);

        auto pos = _mm256_add_ps(middle_right, down_center);

        auto neg = _mm256_add_ps(middle_left, up_center);
        
        auto diff = simd256_abs_diff_ps(pos, neg);
        diff = _mm256_mul_ps(diff, oneHalf); //  (abs(a32 + a23 - a12 - a21) / 2.0f
        
        auto result = threshold32_avx(diff, lowThresh, highThresh);

        simd256_store_ps<mem_mode>(pDst+x, result);
    }
}

template<CpuFlags flags, Border borderMode, MemoryMode mem_mode>
static MT_FORCEINLINE void process_line_roberts_avx(Byte *pDst, const Byte *pSrcp, const Byte *pSrc, const Byte *pSrcn, const Float matrix[10], const __m256 &lowThresh, const __m256 &highThresh, int width) {
    UNUSED(pSrcp);
    UNUSED(matrix);
    auto oneHalf = _mm256_set1_ps(0.5f);

    for (int x = 0; x < width; x+=32) {
        auto middle_center = simd256_load_ps<mem_mode>(pSrc+x);
        auto middle_right = load32_one_to_right_si256<borderMode, mem_mode>(pSrc+x);
        auto down_center = simd256_load_ps<mem_mode>(pSrcn+x);

        auto pos = _mm256_add_ps(middle_center, middle_center);

        auto neg = _mm256_add_ps(middle_right, down_center);

        auto diff = simd256_abs_diff_ps(pos, neg);

        diff = _mm256_mul_ps(diff, oneHalf); 

        auto result = threshold32_avx(diff, lowThresh, highThresh);

        simd256_store_ps<mem_mode>(pDst+x, result);
    }
}

template<CpuFlags flags, Border borderMode, MemoryMode mem_mode>
static MT_FORCEINLINE void process_line_laplace_avx(Byte *pDst, const Byte *pSrcp, const Byte *pSrc, const Byte *pSrcn, const Float matrix[10], const __m256 &lowThresh, const __m256 &highThresh, int width) {
    UNUSED(pSrcp);
    UNUSED(matrix);
    auto oneEightth = _mm256_set1_ps(1.0f/8);
    auto Eight = _mm256_set1_ps(8.0f);
    // abs( (a22 * 8.0f) - a32 - a23 - a11 - a21 - a31 - a12 - a13 - a33 ) / 8.0f
    for (int x = 0; x < width; x+=32) {
        auto up_left = load32_one_to_left_si256<borderMode, mem_mode>(pSrcp+x);
        auto up_center = simd256_load_ps<mem_mode>(pSrcp+x);
        auto up_right = load32_one_to_right_si256<borderMode, mem_mode>(pSrcp+x);

        auto middle_left = load32_one_to_left_si256<borderMode, mem_mode>(pSrc+x);
        auto middle_center = simd256_load_ps<mem_mode>(pSrc+x);
        auto middle_right = load32_one_to_right_si256<borderMode, mem_mode>(pSrc+x);

        auto down_left = load32_one_to_left_si256<borderMode, mem_mode>(pSrcn+x);
        auto down_center = simd256_load_ps<mem_mode>(pSrcn+x);
        auto down_right = load32_one_to_right_si256<borderMode, mem_mode>(pSrcn+x);


        auto acc = _mm256_add_ps(up_left, up_center);
        acc = _mm256_add_ps(acc, up_right);
        acc = _mm256_add_ps(acc, middle_left);
        acc = _mm256_add_ps(acc, middle_right);
        acc = _mm256_add_ps(acc, down_left);
        acc = _mm256_add_ps(acc, down_center);
        acc = _mm256_add_ps(acc, down_right);

        auto pos = _mm256_mul_ps(middle_center, Eight);

        auto diff = simd256_abs_diff_ps(pos, acc);
        diff = _mm256_mul_ps(diff, oneEightth);

        auto result = threshold32_avx(diff, lowThresh, highThresh);

        simd256_store_ps<mem_mode>(pDst+x, result);
    }
}

template<CpuFlags flags, Border borderMode, MemoryMode mem_mode>
static MT_FORCEINLINE void process_line_morpho_avx(Byte *pDst, const Byte *pSrcp, const Byte *pSrc, const Byte *pSrcn, const Float matrix[10], const __m256 &lowThresh, const __m256 &highThresh, int width) {
    UNUSED(matrix);

    for (int x = 0; x < width; x+=32) {
        auto up_left = load32_one_to_left_si256<borderMode, mem_mode>(pSrcp+x);
        auto up_center = simd256_load_ps<mem_mode>(pSrcp+x);
        auto up_right = load32_one_to_right_si256<borderMode, mem_mode>(pSrcp+x);

        auto middle_left = load32_one_to_left_si256<borderMode, mem_mode>(pSrc+x);
        auto middle_center = simd256_load_ps<mem_mode>(pSrc+x);
        auto middle_right = load32_one_to_right_si256<borderMode, mem_mode>(pSrc+x);

        auto down_left = load32_one_to_left_si256<borderMode, mem_mode>(pSrcn+x);
        auto down_center = simd256_load_ps<mem_mode>(pSrcn+x);
        auto down_right = load32_one_to_right_si256<borderMode, mem_mode>(pSrcn+x);

        auto maxv = _mm256_max_ps(middle_right, up_right);
        maxv = _mm256_max_ps(maxv, down_center);
        maxv = _mm256_max_ps(maxv, down_right);
        maxv = _mm256_max_ps(maxv, middle_center);
        maxv = _mm256_max_ps(maxv, up_left);
        maxv = _mm256_max_ps(maxv, down_left);
        maxv = _mm256_max_ps(maxv, up_center);
        maxv = _mm256_max_ps(maxv, middle_left);

        auto minv = _mm256_min_ps(middle_right, up_right);
        minv = _mm256_min_ps(minv, down_center);
        minv = _mm256_min_ps(minv, down_right);
        minv = _mm256_min_ps(minv, middle_center);
        minv = _mm256_min_ps(minv, up_left);
        minv = _mm256_min_ps(minv, down_left);
        minv = _mm256_min_ps(minv, up_center);
        minv = _mm256_min_ps(minv, middle_left);
        
        auto diff = _mm256_sub_ps(maxv, minv);
        auto result = threshold32_avx(diff, lowThresh, highThresh);

        simd256_store_ps<mem_mode>(pDst+x, result);
    }
}

template<CpuFlags flags, Border borderMode, MemoryMode mem_mode>
static MT_FORCEINLINE void process_line_cartoon_avx(Byte *pDst, const Byte *pSrcp, const Byte *pSrc, const Byte *pSrcn, const Float matrix[10], const __m256 &lowThresh, const __m256 &highThresh, int width) {
    UNUSED(matrix); UNUSED(pSrcn);
    /*
    Float val = (a21 * 2.0f) - a22 - a31;
    return val > 0 ? 0 : threshold<Float, Float>(-val, nLowThreshold, nHighThreshold);
    */
    for (int x = 0; x < width; x+=32) {
        auto up_center = simd256_load_ps<mem_mode>(pSrcp+x);
        auto up_right = load32_one_to_right_si256<borderMode, mem_mode>(pSrcp+x);
        auto middle_center = simd256_load_ps<mem_mode>(pSrc+x);
        
        auto acc = _mm256_add_ps(up_right, middle_center);

        acc = _mm256_sub_ps(acc, up_center); // -up_center*2
        acc = _mm256_sub_ps(acc, up_center);

        auto result = threshold32_avx(acc, lowThresh, highThresh);

        simd256_store_ps<mem_mode>(pDst+x, result);
    }
}

template<CpuFlags flags, Border borderMode, MemoryMode mem_mode>
static MT_FORCEINLINE void process_line_prewitt_avx(Byte *pDst, const Byte *pSrcp, const Byte *pSrc, const Byte *pSrcn, const Float matrix[10], const __m256 &lowThresh, const __m256 &highThresh, int width) {
    UNUSED(matrix);

    for (int x = 0; x < width; x+=32) {
        auto up_left = load32_one_to_left_si256<borderMode, mem_mode>(pSrcp+x);
        auto up_center = simd256_load_ps<mem_mode>(pSrcp+x);
        auto up_right = load32_one_to_right_si256<borderMode, mem_mode>(pSrcp+x);

        auto middle_left = load32_one_to_left_si256<borderMode, mem_mode>(pSrc+x);
        auto middle_right = load32_one_to_right_si256<borderMode, mem_mode>(pSrc+x);

        auto down_left = load32_one_to_left_si256<borderMode, mem_mode>(pSrcn+x);
        auto down_center = simd256_load_ps<mem_mode>(pSrcn+x);
        auto down_right = load32_one_to_right_si256<borderMode, mem_mode>(pSrcn+x);


        auto a21_minus_a23 = _mm256_sub_ps(up_center, down_center); // a21 - a23
        
        auto a11_minus_a33 = _mm256_sub_ps(up_left, down_right); // a11 - a33

        auto t1 = _mm256_add_ps(a21_minus_a23, a11_minus_a33); // a11 + a21 - a23 - a33

        auto a12_minus_a32 = _mm256_sub_ps(middle_left, middle_right); // a12 - a32

        auto a13_minus_a31 = _mm256_sub_ps(down_left, up_right); // a13 - a31

        auto t2 = _mm256_add_ps(a12_minus_a32, a13_minus_a31); //a13 + a12 - a31 - a32

        auto p135 = _mm256_sub_ps(t2, a21_minus_a23); //a13 + a12 + a23 - a31 - a32 - a21

        auto p180 = _mm256_add_ps(t2, a11_minus_a33); //a11+ a12+ a13 - a31 - a32 - a33

        auto p90 = _mm256_sub_ps(a13_minus_a31, t1); // a13 - a31 - a11 - a21 + a23 + a33 //negative

        auto p45 = _mm256_add_ps(t1, a12_minus_a32); // a12 + a11 + a21 - a33 - a32 - a23

        p45 = simd256_abs_ps(p45);
        p90 = simd256_abs_ps(p90);
        p135 = simd256_abs_ps(p135);
        p180 = simd256_abs_ps(p180);

        auto max1 = _mm256_max_ps(p45, p90);
        auto max2 = _mm256_max_ps(p135, p180);

        auto result = _mm256_max_ps(max1, max2);

        result = threshold32_avx(result, lowThresh, highThresh);

        simd256_store_ps<mem_mode>(pDst+x, result);
    }
}

template<CpuFlags flags, Border borderMode, MemoryMode mem_mode>
static MT_FORCEINLINE void process_line_half_prewitt_avx(Byte *pDst, const Byte *pSrcp, const Byte *pSrc, const Byte *pSrcn, const Float matrix[10], const __m256 &lowThresh, const __m256 &highThresh, int width) {
    UNUSED(matrix);

    for (int x = 0; x < width; x+=32) {
        auto up_left = load32_one_to_left_si256<borderMode, mem_mode>(pSrcp+x);
        auto up_center = simd256_load_ps<mem_mode>(pSrcp+x);
        auto up_right = load32_one_to_right_si256<borderMode, mem_mode>(pSrcp+x);

        auto middle_left = load32_one_to_left_si256<borderMode, mem_mode>(pSrc+x);
        auto middle_right = load32_one_to_right_si256<borderMode, mem_mode>(pSrc+x);

        auto down_left = load32_one_to_left_si256<borderMode, mem_mode>(pSrcn+x);
        auto down_center = simd256_load_ps<mem_mode>(pSrcn+x);
        auto down_right = load32_one_to_right_si256<borderMode, mem_mode>(pSrcn+x);

        //a11 + 2 * (a21 - a23) + a31 - a13 - a33
        auto t1 = _mm256_sub_ps(up_center, down_center); //2 * (a21 - a23)
        t1 = _mm256_add_ps(t1, t1); // *2
        
        auto t2 = _mm256_sub_ps(up_left, down_left); //a11 - a13
        
        auto t3 = _mm256_sub_ps(up_right, down_right); //a31 - a33

        t1 = _mm256_add_ps(t1, t2);

        auto p90 = _mm256_add_ps(t1, t3);

        //a11 + 2 * (a12 - a32) + a13 - a31 - a33
        t1 = _mm256_sub_ps(middle_left, middle_right); //2 * (a12 - a32)
        t1 = _mm256_add_ps(t1, t2); // 2*

        t2 = _mm256_sub_ps(up_left, up_right); //a11 - a31

        t3 = _mm256_sub_ps(down_left, down_right); //a13 - a33

        t1 = _mm256_add_ps(t1, t2);

        auto p180 = _mm256_add_ps(t1, t3);

        p90 = simd256_abs_ps(p90);
        p180 = simd256_abs_ps(p180);

        auto result = _mm256_max_ps(p90, p180);

        result = threshold32_avx(result, lowThresh, highThresh);

        simd256_store_ps<mem_mode>(pDst+x, result);
    }
}

using namespace Filters::Mask;

#define DEFINE_AVX_VERSIONS(name) \
Processor32 *name##_32_avx = &generic32_avx< \
    process_line_##name##_avx<CPU_AVX, Border::Left, MemoryMode::SSE2_UNALIGNED>, \
    process_line_##name##_avx<CPU_AVX, Border::None, MemoryMode::SSE2_UNALIGNED>, \
    process_line_##name##_avx<CPU_AVX, Border::Right, MemoryMode::SSE2_UNALIGNED> \
>; 


#define DEFINE_ALL_VERSIONS(name) \
DEFINE_AVX_VERSIONS(name)

DEFINE_ALL_VERSIONS(sobel)
DEFINE_ALL_VERSIONS(roberts)
DEFINE_ALL_VERSIONS(laplace)
DEFINE_ALL_VERSIONS(prewitt)
DEFINE_ALL_VERSIONS(half_prewitt)

DEFINE_ALL_VERSIONS(convolution)
DEFINE_ALL_VERSIONS(morpho)
DEFINE_ALL_VERSIONS(cartoon)

#undef DEFINE_ALL_VERSIONS
#undef DEFINE_AVX_VERSIONS


} } } } }