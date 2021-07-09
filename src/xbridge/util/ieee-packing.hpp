
#include <cassert>
#include <cmath>
#include <cstdio>
#include <type_traits>

namespace detail
{
// ------------------------------------------------------------------ pack-float

// F ==> floating point type
// I ==> unsigned integer type
// EXP_DIG ==> digits in the exponent.
//             Should be std::numeric_limits<F>::exponent
template<typename F, int EXP_DIG> auto pack_float(const F value)
{
   using I =
       typename std::conditional<sizeof(F) == 8, uint64_t, uint32_t>::type;

   constexpr int total_bits      = sizeof(F) * 8;
   constexpr int exponent_bits   = EXP_DIG;
   constexpr int fraction_bits   = total_bits - exponent_bits - 1;
   constexpr I fraction_mask     = (I(1) << fraction_bits) - 1;
   constexpr I exponent_mask     = ~fraction_mask & ~(I(1) << (total_bits - 1));
   constexpr int exponent_offset = ((1 << (exponent_bits - 1)) - 1);

   constexpr int exponent_max = (1 << (EXP_DIG - 1));
   constexpr int exponent_min = 2 - exponent_max;

   bool sign_bit = false;
   F fraction    = F(0.0);
   int exponent  = 0;
   I packed      = 0;

   if(!std::isfinite(value)) {
      if(std::isnan(value)) {
         packed = exponent_mask | (I(1) << (fraction_bits - 1));
      } else {
         sign_bit = std::signbit(value);
         packed   = exponent_mask;
         if(sign_bit) packed |= (I(1) << (total_bits - 1));
      }

   } else {
      // Unpack the value
      exponent = 0;
      fraction = std::frexp(value, &exponent);
      if(fraction != F(0.0)) exponent -= 1;
      sign_bit = std::signbit(fraction);

      const bool is_denorm = exponent < exponent_min;
      if(is_denorm) { // Handle denormalized numbers
         while(exponent < exponent_min) {
            fraction /= F(2.0);
            exponent += 1;
         }
      }

      const bool is_zero = (fraction == F(0.0));

      if(is_zero) {
         packed = 0; // all good
      } else {
         constexpr int shift = fraction_bits;
         constexpr F mult    = F(I(1) << shift);
         auto y              = F(2.0) * (sign_bit ? -fraction : fraction);
         packed              = I(mult * (y - I(y)));
         assert(packed >= 0 and packed < (I(1) << shift));

         // Remove any excess precision
         packed &= fraction_mask;

         // Add the exponent
         I out_exp = 0;
         if(!is_denorm) {
            if(exponent >= exponent_max) exponent = exponent_max - 1;
            if(exponent < -exponent_max + 2) exponent = -exponent_max + 2;
            out_exp = (I(exponent + exponent_offset) << fraction_bits);
         }

         assert(out_exp == (out_exp & exponent_mask));
         assert((out_exp ^ exponent_mask) != 0); // that would be infinity

         packed |= out_exp;
      }

      // Add sign bit
      if(sign_bit) packed |= (I(1) << (total_bits - 1));
   }

   return packed;
}

// ---------------------------------------------------------------- unpack-float

template<typename I, int EXP_DIG> auto unpack_float(const I packed)
{
   using F = typename std::conditional<sizeof(I) == 8, double, float>::type;

   constexpr int total_bits      = sizeof(I) * 8;
   constexpr int exponent_bits   = EXP_DIG;
   constexpr int fraction_bits   = total_bits - exponent_bits - 1;
   constexpr I fraction_mask     = (I(1) << fraction_bits) - 1;
   constexpr I sign_mask         = I(1) << (total_bits - 1);
   constexpr I exponent_mask     = ~fraction_mask & ~sign_mask;
   constexpr int exponent_offset = ((1 << (exponent_bits - 1)) - 1);

   const bool sign_bit = (packed & sign_mask) != 0;

   // infinity and NAN
   const bool is_finite = ((packed & exponent_mask) ^ exponent_mask) != 0;
   if(!is_finite) {
      if((packed & fraction_mask) != 0)
         return std::numeric_limits<F>::quiet_NaN();
      else if(!sign_bit)
         return std::numeric_limits<F>::infinity();
      else
         return -std::numeric_limits<F>::infinity();
   }

   // Onto finite values
   int exponent0 = ((packed & exponent_mask) >> fraction_bits);
   int exponent  = exponent0 - exponent_offset;

   const bool is_denorm = (exponent0 == 0);

   // Handle 0.0 and -0.0
   if(exponent0 == 0 and (packed & fraction_mask) == 0)
      return sign_bit ? -F(0.0) : F(0.0);

   // Handle 1.0 and -1.0
   if(exponent == 0 and !is_denorm) return sign_bit ? -F(1.0) : F(1.0);

   const I packed_fraction0 = (packed & fraction_mask) << (exponent_bits + 1);

   constexpr int shift    = fraction_bits;
   constexpr I shift_mask = ((I(1) << shift) - 1) << (total_bits - shift);
   constexpr F mult       = F(I(1) << shift);
   constexpr F div        = F(1.0) / mult;

   auto frac_int = I((packed_fraction0 & shift_mask) >> (total_bits - shift));

   F fraction = F(0.5) * (F(frac_int) * div + (is_denorm ? F(0.0) : F(1.0)));

   F out = std::ldexp(fraction, exponent + 1);

   return sign_bit ? -out : out;
}

} // namespace detail

template<typename F> auto pack_float(F x)
{
   using I = typename std::conditional_t<sizeof(F) == 8, uint64_t, uint32_t>;
#ifdef __STDC_IEC_559__
   return *reinterpret_cast<I*>(&x);
#else
   if constexpr(sizeof(F) == 8) return detail::pack_float<F, 11>(x);
   return detail::pack_float<F, 8>(x);
#endif
}

template<typename I> auto unpack_float(I x)
{
   using F = typename std::conditional_t<sizeof(I) == 8, double, float>;
#ifdef __STDC_IEC_559__
   return *reinterpret_cast<F*>(&x);
#else
   if constexpr(sizeof(F) == 8) return detail::unpack_float<F, 11>(x);
   return detail::unpack_float<T, 8>(x);
#endif
}

inline uint32_t pack_f32(float x) { return pack_float(x); }
inline uint64_t pack_f64(double x) { return pack_float(x); }
inline float unpack_f32(uint32_t x) { return unpack_float(x); }
inline double unpack_f64(uint64_t x) { return unpack_float(x); }
