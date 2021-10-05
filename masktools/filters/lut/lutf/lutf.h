#ifndef __Mt_Lutf_H__
#define __Mt_Lutf_H__

#include "../../../common/base/filter.h"
#include "../../../../common/parser/parser.h"

#include "../functions.h"

namespace Filtering { namespace MaskTools { namespace Filters { namespace Lut { namespace Frame {

typedef void(Processor)(Byte *pDst, ptrdiff_t nDstPitch, const Byte *pSrc, ptrdiff_t nSrcPitch, const Byte lut[65536], Parser::Context *ctx, int nWidth, int nHeight); // common lut/realtime, templatized
typedef void(Processor16)(Word *pDst, ptrdiff_t nDstPitch, const Word *pSrc, ptrdiff_t nSrcPitch, const Word *lut, Parser::Context *ctx, int nWidth, int nHeight); // common lut/realtime, templatized
typedef void(Processor32)(Float *pDst, ptrdiff_t nDstPitch, const Float *pSrc, ptrdiff_t nSrcPitch, Parser::Context *ctx, int nWidth, int nHeight, bool chroma); // realtime only

extern Processor *processors_array[NUM_MODES];
extern Processor *processorsCtx_array[NUM_MODES];

extern Processor16 *processors10_array[NUM_MODES];
extern Processor16 *processors10Ctx_array[NUM_MODES];
extern Processor16 *processors12_array[NUM_MODES];
extern Processor16 *processors12Ctx_array[NUM_MODES];
extern Processor16 *processors14_array[NUM_MODES];
extern Processor16 *processors14Ctx_array[NUM_MODES];
extern Processor16 *processors16_array[NUM_MODES];
extern Processor16 *processors16Ctx_array[NUM_MODES];
extern Processor32 *processors32Ctx_array[NUM_MODES];

class Lutf : public MaskTools::Filter
{
  // re-use repeated luts, like in lutxyz
  struct Lut {
    bool used;
    Byte *ptr;
  };

  Lut luts[4+1]; // max plane count + 1

  static Byte *calculateLut(const std::deque<Filtering::Parser::Symbol> &expr, int bits_per_pixel, String scale_inputs, int clamp_float, int& compute_error) {
    Parser::Context ctx(expr, scale_inputs, clamp_float);
    int pixelsize = bits_per_pixel == 8 ? 1 : 2; // byte / uint16_t
    size_t buffer_size = ((size_t)1 << bits_per_pixel) * ((size_t)1 << bits_per_pixel) *pixelsize;
    Byte *lut = new Byte[buffer_size];

    switch (bits_per_pixel) {
    case 8:
      for (int x = 0; x < 256; x++)
        for (int y = 0; y < 256; y++)
          lut[(x << 8) + y] = ctx.compute_byte_xy(x, y);
      break;
    case 10:
      for (int x = 0; x < 1024; x++)
        for (int y = 0; y < 1024; y++)
          reinterpret_cast<Word *>(lut)[(x << 10) + y] = ctx.compute_word_xy<10>(x, y);
      break;
    case 12:
      for (int x = 0; x < 4096; x++)
        for (int y = 0; y < 4096; y++)
          reinterpret_cast<Word *>(lut)[(x << 12) + y] = ctx.compute_word_xy<12>(x, y);
      break;
    case 14:
      for (int x = 0; x < 16384; x++)
        for (int y = 0; y < 16384; y++)
          reinterpret_cast<Word *>(lut)[(x << 14) + y] = ctx.compute_word_xy<14>(x, y);
      break;
    case 16:
      // 64bit only
      for (int x = 0; x < 65536; x++)
        for (int y = 0; y < 65536; y++)
          reinterpret_cast<Word *>(lut)[((size_t)x << 16) + y] = ctx.compute_word_xy<16>(x, y);
      break;
    }

    // problem if compute_error != Parser::Context::compute_error_t::CE_NONE
    compute_error = ctx.get_compute_error();

    return lut;
  }

  // for realtime
  std::deque<Filtering::Parser::Symbol> *parsed_expressions[4];

  ProcessorList<Processor> processors;
  ProcessorList<Processor16> processors16;
  ProcessorList<Processor32> processors32;

  int bits_per_pixel;
  bool realtime;
  String scale_inputs;
  int clamp_float_i;

protected:
    virtual void process(int n, const Plane<Byte> &dst, int nPlane, const Filtering::Frame<const Byte> frames[4], const Constraint constraints[4]) override
    {
        UNUSED(n);
        
        if (realtime) {
          // thread safety
          Parser::Context ctx(*parsed_expressions[nPlane], scale_inputs, clamp_float_i);
          if (bits_per_pixel == 8)
            processors.best_processor(constraints[nPlane])(dst.data(), dst.pitch(),
              frames[0].plane(nPlane).data(), frames[0].plane(nPlane).pitch(),
              nullptr, &ctx, dst.width(), dst.height());
          else if (bits_per_pixel <= 16)
            processors16.best_processor(constraints[nPlane])((Word *)dst.data(), dst.pitch(),
              (Word *)frames[0].plane(nPlane).data(), frames[0].plane(nPlane).pitch(),
              nullptr, &ctx, dst.width(), dst.height());
          else {
            const bool chroma = ((nPlane == 1 || nPlane == 2) && !planes_isRGB[C]);
            processors32.best_processor(constraints[nPlane])((Float *)dst.data(), dst.pitch(),
              (Float *)frames[0].plane(nPlane).data(), frames[0].plane(nPlane).pitch(),
              &ctx, dst.width(), dst.height(), chroma);
          }
        }
        else {
          // lut
          if (bits_per_pixel == 8)
            processors.best_processor(constraints[nPlane])(dst.data(), dst.pitch(),
              frames[0].plane(nPlane).data(), frames[0].plane(nPlane).pitch(),
              luts[nPlane].ptr, nullptr, dst.width(), dst.height());
          else if (bits_per_pixel <= 16)
            processors16.best_processor(constraints[nPlane])((Word *)dst.data(), dst.pitch(),
              (Word *)frames[0].plane(nPlane).data(), frames[0].plane(nPlane).pitch(),
              (Word *)luts[nPlane].ptr, nullptr, dst.width(), dst.height());
          // no 32 bit lut
        }
    }

public:
   Lutf(const Parameters &parameters, CpuFlags cpuFlags) : MaskTools::Filter( parameters, FilterProcessingType::INPLACE, (CpuFlags)cpuFlags)
   {
     for (int i = 0; i < 4; i++) {
       parsed_expressions[i] = nullptr;
     }

     for (int i = 0; i < 4+1; ++i) {
       luts[i].used = false;
       luts[i].ptr = nullptr;
     }

      static const char *expr_strs[] = { "yExpr", "uExpr", "vExpr", "aExpr" };

      Parser::Parser parser = Parser::getDefaultParser().addSymbol(Parser::Symbol::X).addSymbol(Parser::Symbol::Y);

      bits_per_pixel = bit_depths[C];
      realtime = parameters["realtime"].toBool();
      scale_inputs = parameters["scale_inputs"].toString();
      if (!checkValidScaleInputs(scale_inputs, error))
        return; // error message filled
      bool clamp_float = parameters["clamp_float"].toBool();
      bool clamp_float_UV = parameters["clamp_float_UV"].toBool();
      clamp_float_i = clamp_float ? (clamp_float_UV ? 2 : 1) : 0;

      // 14, 16 bit and float: default realtime, 
      // hardcore users on x64 with 8GB memory and plenty of time can override 16 bit lutxy with realtime=false :)
      // lut sizes
      // 10 bits: 2 MBytes (2*1024*1024) per expression 
      // 12 bits: 32 MBytes (2*4096*4096) per expression 
      // 14 bits: 512 MBytes (2*16384*16384) per expression 
      // 16 bits: 8 GBytes (2*65536*65536) per expression 
      if (bits_per_pixel >= 14 && !parameters["realtime"].is_defined()) {
        realtime = true;
      }

      if (bits_per_pixel == 32)
        realtime = true;

      if (bits_per_pixel == 16) {
        if ((uint64_t)std::numeric_limits<size_t>::max() <= 0xFFFFFFFFull && bits_per_pixel == 16) {
          realtime = true; // not even possible a real 16 bit lutxy on 32 bit environment
        }
      }

      /* compute the luts */
      for ( int i = 0; i < 4; i++ )
      {
          if (operators[i] != PROCESS) {
              continue;
          }

          if (parameters[expr_strs[i]].undefinedOrEmptyString() && parameters["expr"].undefinedOrEmptyString()) {
              operators[i] = NONE; //inplace
              continue;
          }

          bool customExpressionDefined = false;
          if (parameters[expr_strs[i]].is_defined()) {
            parser.parse_strict(parameters[expr_strs[i]].toString(), Parser::SYMBOL_SEPARATORS);
            customExpressionDefined = true;
          }
          else
            parser.parse_strict(parameters["expr"].toString(), Parser::SYMBOL_SEPARATORS);

          auto err_pos = parser.getErrorPos();
          if (err_pos >= 0) {
            error = "Error at position " + std::to_string(1 + err_pos) + ": cannot convert to number: " + parser.getFailedSymbol();
            return;
          }

          // for check:
          Parser::Context ctx(parser.getExpression(), scale_inputs, clamp_float);

          if (!ctx.check())
          {
            error = "invalid expression in the lut";
            return;
          }

          if (realtime) {
            parsed_expressions[i] = new std::deque<Parser::Symbol>(parser.getExpression());

            continue;
          }

          // pure lut, no realtime

          int compute_error = Parser::Context::compute_error_t::CE_NONE;

          // save memory, reuse luts, like in xyz
          if (customExpressionDefined) {
            luts[i].used = true;
            luts[i].ptr = calculateLut(parser.getExpression(), bits_per_pixel, scale_inputs, clamp_float, /*ref*/ compute_error);
          }
          else {
            if (luts[4].ptr == nullptr) { // 0..3: planes, 4:last common
              luts[4].used = true;
              luts[4].ptr = calculateLut(parser.getExpression(), bits_per_pixel, scale_inputs, clamp_float, /*ref*/ compute_error);
            }
            luts[i].ptr = luts[4].ptr;
          }

          if (compute_error != Parser::Context::compute_error_t::CE_NONE) {
            error = "invalid expression in the lut code = " + std::to_string(compute_error);
            return;
          }

      }
      if (realtime) {
        switch (bits_per_pixel) {
        case 8: processors.push_back(processorsCtx_array[ModeToInt(parameters["mode"].toString())]); break;
        case 10: processors16.push_back(processors10Ctx_array[ModeToInt(parameters["mode"].toString())]); break;
        case 12: processors16.push_back(processors12Ctx_array[ModeToInt(parameters["mode"].toString())]); break;
        case 14: processors16.push_back(processors14Ctx_array[ModeToInt(parameters["mode"].toString())]); break;
        case 16: processors16.push_back(processors16Ctx_array[ModeToInt(parameters["mode"].toString())]); break;
        case 32: processors32.push_back(processors32Ctx_array[ModeToInt(parameters["mode"].toString())]); break;
        }
      }
      else {
        // real lut
        switch (bits_per_pixel) {
        case 8:
          processors.push_back(processors_array[ModeToInt(parameters["mode"].toString())]);
          // processor = lut_c;
          break;
        case 10:
          processors16.push_back(processors10_array[ModeToInt(parameters["mode"].toString())]);
          //processor16 = lut10_c;
          break;
        case 12:
          processors16.push_back(processors12_array[ModeToInt(parameters["mode"].toString())]);
          //processor16 = lut12_c;
          break;
        case 14:
          processors16.push_back(processors14_array[ModeToInt(parameters["mode"].toString())]);
          //processor16 = lut14_c;
          break;
        case 16:
          // 64bit only
          processors16.push_back(processors16_array[ModeToInt(parameters["mode"].toString())]);
          //processor16 = lut16_c;
          break;
        }
      }
   }

   ~Lutf()
   {
     for (int i = 0; i < 4+1; ++i) {
       if (luts[i].used) {
         delete[] luts[i].ptr;
       }
     }
     for (int i = 0; i < 4; i++) {
       delete parsed_expressions[i];
     }
   }

   InputConfiguration &input_configuration() const override { return InPlaceTwoFrame(); }

   static Signature filter_signature()
   {
      Signature signature = "mt_lutf";

      signature.add( Parameter( TYPE_CLIP, "", false) );
      signature.add( Parameter( TYPE_CLIP, "", false) );
      signature.add( Parameter( String( "average" ), "mode", false) );
      signature.add( Parameter( String( "y" ), "expr", false) );
      signature.add( Parameter( String( "y" ), "yExpr", false) );
      signature.add( Parameter( String( "y" ), "uExpr", false) );
      signature.add( Parameter( String( "y" ), "vExpr", false) );

      add_defaults( signature );

      signature.add(Parameter(false, "realtime", false));
      signature.add(Parameter(String("y"), "aExpr", false));
      signature.add(Parameter(String("none"), "scale_inputs", false));
      signature.add(Parameter(false, "clamp_float", false));
      signature.add(Parameter(false, "clamp_float_UV", false));

      return signature;
   }
};

} } } } }

#endif