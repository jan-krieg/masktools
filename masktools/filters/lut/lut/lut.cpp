#include "lut.h"

using namespace Filtering;

void Filtering::MaskTools::Filters::Lut::Single::lut_c(Byte * dstp, ptrdiff_t dst_pitch, int width, int height, const Byte * lut)
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++) {
            dstp[x] = lut[dstp[x]];
        }
        dstp += dst_pitch;
    }
}

void Filtering::MaskTools::Filters::Lut::Single::realtime8_c(Byte *dstp, ptrdiff_t dst_pitch, int width, int height, Parser::Context &ctx)
{
  for (int y = 0; y < height; y++)
  {
    for (int x = 0; x < width; x++) {
      dstp[x] = ctx.compute_byte_x(dstp[x]);
    }
    dstp += dst_pitch;
  }
}


void Filtering::MaskTools::Filters::Lut::Single::realtime32_c(Byte *dstp, ptrdiff_t dst_pitch, int width, int height, bool chroma, Parser::Context &ctx)
{
  for (int y = 0; y < height; y++)
  {
    for (int x = 0; x < width; x++) {
      reinterpret_cast<Float *>(dstp)[x] = ctx.compute_float_x(reinterpret_cast<Float *>(dstp)[x], chroma);
    }
    dstp += dst_pitch;
  }
}

