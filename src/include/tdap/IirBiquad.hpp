#ifndef TDAP_M_IIR_BIQUAD_HPP
#define TDAP_M_IIR_BIQUAD_HPP
/*
 * tdap/IirBiquad.hpp
 *
 * Part of TdAP
 * Time-domain Audio Processing
 * Copyright (C) 2015-2016 Michel Fleur.
 * Source https://bitbucket.org/emmef/tdap
 * Email  tdap@emmef.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <tdap/Frequency.hpp>
#include <tdap/IirCoefficients.hpp>
#include <tdap/Value.hpp>

namespace tdap {

struct BiQuad {
  static constexpr double PARAMETRIC_BANDWIDTH_MINIMUM = 0.0001;
  static constexpr double PARAMETRIC_BANDWIDTH_MAXIMUM = 16.0;

  static constexpr double SHELVE_SLOPE_MINIMUM = 0.0001;
  static constexpr double SHELVE_SLOPE_MAXIMUM = 1.0;

  static constexpr double limitedSlope(double slope) {
    return Value<double>::force_between(slope, SHELVE_SLOPE_MINIMUM,
                                        SHELVE_SLOPE_MAXIMUM);
  }

  static constexpr double limitedBandwidth(double bandwidth) {
    return Value<double>::force_between(bandwidth, PARAMETRIC_BANDWIDTH_MINIMUM,
                                        PARAMETRIC_BANDWIDTH_MAXIMUM);
  }

  static void setParametric(IirCoefficients &coefficients, double sampleRate,
                            double centerFrequency, double gain,
                            double bandwidth) {

    setCoefficients(
        coefficients,
        getParametricParameters(Frequency<double>::relativeNycquistLimited(
                                    centerFrequency, sampleRate),
                                gain, limitedBandwidth(bandwidth)));
  }

  static void setLowShelve(IirCoefficients &coefficients, double sampleRate,
                           double centerFrequency, double gain, double slope) {
    setCoefficients(
        coefficients,
        getLowShelveParameters(Frequency<double>::relativeNycquistLimited(
                                   centerFrequency, sampleRate),
                               gain, limitedSlope(slope)));
  }

  static void setHighShelve(IirCoefficients &coefficients, double sampleRate,
                            double centerFrequency, double gain, double slope) {
    setCoefficients(
        coefficients,
        getHighShelveParameters(Frequency<double>::relativeNycquistLimited(
                                    centerFrequency, sampleRate),
                                gain, limitedSlope(slope)));
  }

  static void setLowPass(IirCoefficients &coefficients, double sampleRate,
                         double centerFrequency, double bandwidth) {
    setCoefficients(
        coefficients,
        getLowPassParameters(Frequency<double>::relativeNycquistLimited(
                                 centerFrequency, sampleRate),
                             limitedBandwidth(bandwidth)));
  }

  static void setHighPass(IirCoefficients &coefficients, double sampleRate,
                          double centerFrequency, double bandwidth) {
    setCoefficients(
        coefficients,
        getHighPassParameters(Frequency<double>::relativeNycquistLimited(
                                  centerFrequency, sampleRate),
                              limitedBandwidth(bandwidth)));
  }

  static void setBandPass(IirCoefficients &coefficients, double sampleRate,
                          double centerFrequency, double bandwidth) {
    setCoefficients(
        coefficients,
        getBandPassParameters(Frequency<double>::relativeNycquistLimited(
                                  centerFrequency, sampleRate),
                              limitedBandwidth(bandwidth)));
  }

private:
  struct BiQuadCoefficients {
    double C0;
    double C1;
    double C2;
    double D1;
    double D2;
  };

  static void setCoefficients(IirCoefficients &builder,
                              const BiQuadCoefficients bqc) {
    if (builder.order() != 2) {
      if (builder.hasFixedOrder()) {
        throw std::invalid_argument(
            "Builder must have filter order two (or order can be set)");
      }
      builder.setOrder(2);
    }
    builder.setC(0, bqc.C0);
    builder.setC(1, bqc.C1);
    builder.setC(2, bqc.C2);
    builder.setD(0, 0);
    builder.setD(1, bqc.D1);
    builder.setD(2, bqc.D2);
  }

  static BiQuadCoefficients
  getParametricParameters(double relativeCenterFrequency, double gain,
                          double bandwidth) {
    static constexpr double LN_2_2 = (M_LN2 / 2);
    double w = Frequency<double>::angularSpeed(relativeCenterFrequency);
    double cw = cosf(w);
    double sw = sinf(w);
    double J = sqrt(gain);
    double g = sw * sinhf(LN_2_2 * limitedBandwidth(bandwidth) * w / sw);
    double a0r = 1.0f / (1.0f + (g / J));

    BiQuadCoefficients result;
    result.C0 = (1.0f + (g * J)) * a0r;
    result.C1 = (-2.0f * cw) * a0r;
    result.C2 = (1.0f - (g * J)) * a0r;
    result.D1 = -result.C1;
    result.D2 = ((g / J) - 1.0f) * a0r;

    return result;
  }

  static BiQuadCoefficients
  getLowShelveParameters(double relativeCenterFrequency, double gain,
                         double slope) {
    double w = Frequency<double>::angularSpeed(relativeCenterFrequency);
    double cw = cosf(w);
    double sw = sinf(w);
    double A = sqrt(gain);
    double b =
        sqrt(((1.0f + A * A) / limitedSlope(slope)) - ((A - 1.0) * (A - 1.0)));
    double apc = cw * (A + 1.0f);
    double amc = cw * (A - 1.0f);
    double bs = b * sw;
    double a0r = 1.0f / (A + 1.0f + amc + bs);

    BiQuadCoefficients result;
    result.C0 = a0r * A * (A + 1.0f - amc + bs);
    result.C1 = a0r * 2.0f * A * (A - 1.0f - apc);
    result.C2 = a0r * A * (A + 1.0f - amc - bs);
    result.D1 = a0r * 2.0f * (A - 1.0f + apc);
    result.D2 = a0r * (-A - 1.0f - amc + bs);
    return result;
  }

  static BiQuadCoefficients
  getHighShelveParameters(double relativeCenterFrequency, double gain,
                          double slope) {
    double w = Frequency<double>::angularSpeed(relativeCenterFrequency);
    double cw = cosf(w);
    double sw = sinf(w);
    double A = sqrt(gain);
    double b = sqrt(((1.0f + A * A) / limitedSlope(slope)) -
                    ((A - 1.0f) * (A - 1.0f)));
    double apc = cw * (A + 1.0f);
    double amc = cw * (A - 1.0f);
    double bs = b * sw;
    double a0r = 1.0f / (A + 1.0f - amc + bs);

    BiQuadCoefficients result;
    result.C0 = a0r * A * (A + 1.0f + amc + bs);
    result.C1 = a0r * -2.0f * A * (A - 1.0f + apc);
    result.C2 = a0r * A * (A + 1.0f + amc - bs);
    result.D1 = a0r * -2.0f * (A - 1.0f - apc);
    result.D2 = a0r * (-A - 1.0f + amc + bs);
    return result;
  }

  static BiQuadCoefficients getLowPassParameters(double relativeCenterFrequency,
                                                 double bandwidth) {
    double omega = Frequency<double>::angularSpeed(relativeCenterFrequency);
    double sn = sin(omega);
    double cs = cos(omega);
    double alpha = sn * sinh(M_LN2 / 2.0 * bandwidth * omega / sn);
    const double a0r = 1.0 / (1.0 + alpha);

    BiQuadCoefficients result;
    result.C0 = a0r * (1.0 - cs) * 0.5;
    result.C1 = a0r * (1.0 - cs);
    result.C2 = a0r * (1.0 - cs) * 0.5;
    result.D1 = a0r * (2.0 * cs);
    result.D2 = a0r * (alpha - 1.0);
    return result;
  }

  static BiQuadCoefficients
  getHighPassParameters(double relativeCenterFrequency, double bandwidth) {
    double omega = Frequency<double>::angularSpeed(relativeCenterFrequency);
    double sn = sin(omega);
    double cs = cos(omega);
    double alpha = sn * sinh(M_LN2 / 2.0 * bandwidth * omega / sn);
    const double a0r = 1.0 / (1.0 + alpha);

    BiQuadCoefficients result;
    result.C0 = a0r * (1.0 + cs) * 0.5;
    result.C1 = a0r * -(1.0 + cs);
    result.C2 = a0r * (1.0 + cs) * 0.5;
    result.D1 = a0r * (2.0 * cs);
    result.D2 = a0r * (alpha - 1.0);
    return result;
  }

  static BiQuadCoefficients
  getBandPassParameters(double relativeCenterFrequency, double bandwidth) {
    double omega = Frequency<double>::angularSpeed(relativeCenterFrequency);
    double sn = sin(omega);
    double cs = cos(omega);
    double alpha = sn * sinh(M_LN2 / 2.0 * bandwidth * omega / sn);
    const double a0r = 1.0 / (1.0 + alpha);

    BiQuadCoefficients result;
    result.C0 = a0r * alpha;
    result.C1 = 0.0;
    result.C2 = a0r * -alpha;
    result.D1 = a0r * (2.0 * cs);
    result.D2 = a0r * (alpha - 1.0);
    return result;
  }
};

template <typename Coefficient, size_t CHANNELS>
struct BiquadFilter
    : public FixedSizeIirCoefficientFilter<Coefficient, CHANNELS, 2> {
  using Coefficients =
      typename FixedSizeIirCoefficientFilter<Coefficient, CHANNELS,
                                             2>::Coefficients;

  BiquadFilter() = default;

  BiquadFilter(const Coefficients &coeffs)
      : FixedSizeIirCoefficientFilter<Coefficient, CHANNELS, 2>(coeffs) {}
};

} // namespace tdap

#endif // TDAP_M_IIR_BIQUAD_HPP
