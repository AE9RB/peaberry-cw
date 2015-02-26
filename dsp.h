// Peaberry CW - Transceiver for Peaberry SDR
// Copyright (C) 2015 David Turnbull AE9RB
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#ifndef DSP_H
#define DSP_H

#include <complex>
#include <cmath>
#include <qmath.h>

// Demodulation decimates by 2 so that we can process
// a large FIR filter with a minimum of latency.
#define PEABERRYSIZE (2048)
#define PEABERRYRATE (96000)
#define DEMODSIZE (1024)
#define DEMODRATE (48000)

// Types used for DSP/FFT bulk work.
// Note that we'll use qreal where we want the most accurate number
// the platform does natively. Useful for NCOs and other non-array things.
typedef float REAL;
typedef std::complex<REAL> COMPLEX;

// Disable special (slow) handling of denormal floating point types.
#ifdef __SSE__
#include <xmmintrin.h>
#endif
#ifdef __SSE3__
#include <pmmintrin.h>
#endif
class FastDenormals {
public:
    static inline void enable() {
        #ifdef __SSE__
        _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
        #endif
        #ifdef __SSE3__
        _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
        #endif
    }
    static inline void disable() {
        #ifdef __SSE__
        _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_OFF);
        #endif
        #ifdef __SSE3__
        _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_OFF);
        #endif
    }
};


// The ISO specification for C++ requires that complex multiplication
// check for NaN results and adjust for mathematical correctness.
// This is a significant performance problem for DSP algorithms
// which never use NaN or Inf. GCC has an option (<tt>-fcx-limited-range</tt>)
// to disable this but clang currently does not. Since we are allowed
// to specialize existing functions in std we can fix this for all
// compilers.
namespace std {
#define DSP_SPECIALIZE_COMPLEX_MULTIPLICATION(T1, T2) \
std::complex<decltype(T1()+T2())> \
inline operator*(const std::complex<T1>& z, const std::complex<T2>& w) \
{   return std::complex<decltype(T1()+T2())>( \
    z.real()*w.real() - z.imag()*w.imag(), \
    z.imag()*w.real() + z.real()*w.imag() \
);}
/// Limited range complex multiplication.
DSP_SPECIALIZE_COMPLEX_MULTIPLICATION(float, float)
/// Limited range complex multiplication.
DSP_SPECIALIZE_COMPLEX_MULTIPLICATION(double, float)
/// Limited range complex multiplication.
DSP_SPECIALIZE_COMPLEX_MULTIPLICATION(float, double)
/// Limited range complex multiplication.
DSP_SPECIALIZE_COMPLEX_MULTIPLICATION(double, double)
} /* namespace std */
#undef DSP_SPECIALIZE_COMPLEX_MULTIPLICATION


#endif // DSP_H
