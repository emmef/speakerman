/*
 * tdap/IirButterworth.hpp
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

#ifndef TDAP_IIRBUTTERWORTH_HEADER_GUARD
#define TDAP_IIRBUTTERWORTH_HEADER_GUARD

#include <tdap/Frequency.hpp>
#include <tdap/IirCoefficients.hpp>

namespace tdap {

struct Butterworth {
  enum class Pass { LOW, HIGH };

  static constexpr size_t MAX_ORDER = 8;
  /**
   * The number of feedback _or_ feed forward coefficients.
   */
  static constexpr size_t COEFFICIENTS = IirCoefficients::coefficientsForOrder(MAX_ORDER);

  static bool isValidOrder(size_t order) {
    return order > 0 && order <= MAX_ORDER;
  }

  static size_t checkValidOrder(size_t order) {
    if (isValidOrder(order)) {
      return order;
    }
    throw std::invalid_argument("Butterworth order must be between 1 and 20");
  }

  template <typename Coefficient, typename Freq>
  static Coefficient getHighPassGain(const Freq inputFrequency,
                                     const Freq filterFrequency,
                                     const size_t order) {
    if (filterFrequency < Value<double>::min_positive()) {
      return 1.0;
    }
    if (inputFrequency < Value<double>::min_positive()) {
      return 0.0;
    }
    return sqrt(1.0 / (1.0 + pow(filterFrequency / inputFrequency, 2 * order)));
  }

  template <typename Coefficient, typename Freq>
  static Coefficient getLowPassGain(Freq inputFrequency, Freq filterFrequency,
                                    size_t order) {
    if (filterFrequency < Value<double>::min_positive()) {
      return 0.0;
    }
    if (inputFrequency < Value<double>::min_positive()) {
      return 1.0;
    }
    return sqrt(1.0 / (1.0 + pow(inputFrequency / filterFrequency, 2 * order)));
  }

  template <typename Coefficient, typename Freq>
  static void create(IirCoefficients &coefficients, Freq sampleRate,
                     Freq frequency, Pass pass, Coefficient scale = 1.0) {
    create(coefficients,
           Frequency<Freq>::relativeNycquistLimited(frequency, sampleRate),
           pass, scale);
  }

  template <typename Coefficient>
  static void create(IirCoefficients &coefficients,
                     const double relativeFrequency, Pass pass,
                     Coefficient scale = 1.0) {
    switch (pass) {
    case Pass::LOW:
      getLowPassCoefficients(coefficients, relativeFrequency, scale);
      return;
    case Pass::HIGH:
      getHighPassCoefficients(coefficients, relativeFrequency, scale);
      return;
    default:
      throw std::invalid_argument("Unknown filter pass (must be low or high");
    }
  }

  template <typename Coefficient>
  static void getLowPassCoefficients(IirCoefficients &coefficients,
                                     double relativeFrequency,
                                     Coefficient scale = 1.0) {
    size_t order = coefficients.order();
    checkValidOrder(order);
    int unscaledCCoefficients[COEFFICIENTS];

    fill_with_zero(unscaledCCoefficients, sizeof(unscaledCCoefficients));

    getDCoefficients(order, relativeFrequency, coefficients);

    getUnscaledLowPassCCoefficients(order, unscaledCCoefficients);

    double scaleOfC = scale * getLowPassScalingFactor(order, relativeFrequency);

    for (size_t i = 0; i <= order; i++) {
      coefficients.setC(i, scaleOfC * unscaledCCoefficients[i]);
    }
  }

  template <typename Coefficient>
  static void getHighPassCoefficients(IirCoefficients &coefficients,
                                      double relativeFrequency,
                                      Coefficient scale = 1.0) {
    size_t order = coefficients.order();
    checkValidOrder(order);
    int unscaledCCoefficients[COEFFICIENTS];
    fill_with_zero(unscaledCCoefficients, sizeof(unscaledCCoefficients));

    getDCoefficients(order, relativeFrequency, coefficients);

    getUnscaledHighPassCCoefficients(order, unscaledCCoefficients);

    double scaleOfC =
        scale * getHighPassScalingFactor(order, relativeFrequency);

    for (size_t i = 0; i <= order; i++) {
      coefficients.setC(i, scaleOfC * unscaledCCoefficients[i]);
    }
  }

private:
  template <typename T>
  static void fill_with_zero(T *const location, size_t size) {
    for (size_t i = 0; i < size / sizeof(T); i++) {
      location[i] = 0;
    }
  }

  static void getDCoefficients(int order, double relativeFrequency,
                               IirCoefficients &d_coefficients) {
    double theta; // M_PI * relativeFrequency / 2.0
    double st;    // sine of theta
    double ct;    // cosine of theta
    double parg;  // pole angle
    double sparg; // sine of the pole angle
    double cparg; // cosine of the pole angle
    double a;     // workspace variable

    double dcof[MAX_ORDER * 2 + 1];
    fill_with_zero(dcof, sizeof(dcof));
    // binomial coefficients
    double binomials[2 * MAX_ORDER + 2];
    fill_with_zero(binomials, sizeof(binomials));

    theta = M_PI * relativeFrequency * 2;
    st = sin(theta);
    ct = cos(theta);

    for (int k = 0; k < order; ++k) {
      parg = M_PI * (double)(2 * k + 1) / (double)(2 * order);
      sparg = sin(parg);
      cparg = cos(parg);
      a = 1.0 + st * sparg;
      binomials[2 * k] = -ct / a;
      binomials[2 * k + 1] = -st * cparg / a;
    }

    for (int i = 0; i < order; ++i) {
      for (int j = i; j > 0; --j) {
        dcof[2 * j] += binomials[2 * i] * dcof[2 * (j - 1)] -
                       binomials[2 * i + 1] * dcof[2 * (j - 1) + 1];
        dcof[2 * j + 1] += binomials[2 * i] * dcof[2 * (j - 1) + 1] +
                           binomials[2 * i + 1] * dcof[2 * (j - 1)];
      }
      dcof[0] += binomials[2 * i];
      dcof[1] += binomials[2 * i + 1];
    }

    dcof[1] = dcof[0];
    dcof[0] = 1.0;
    for (int k = 3; k <= order; ++k) {
      dcof[k] = dcof[2 * k - 2];
    }
    for (int i = 0; i <= order; i++) {
      /*
       * Negate coefficients as this calculus was meant for recursive equations
       * where they where subtracted instead of added. We do adds only, so we
       * need to negate them here.
       */
      d_coefficients.setD(i, -dcof[i]);
    }
  }

  static void getUnscaledLowPassCCoefficients(int order, int *ccof) {
    ccof[0] = 1;
    ccof[1] = order;

    for (int m = order / 2, i = 2; i <= m; ++i) {
      ccof[i] = (order - i + 1) * ccof[i - 1] / i;
      ccof[order - i] = ccof[i];
    }
    ccof[order - 1] = order;
    ccof[order] = 1;
  }

  static double getLowPassScalingFactor(int order, double relativeFrequency) {
    int k;         // loop variables
    double omega;  // M_PI * relativeFrequency
    double fomega; // function of omega
    double parg0;  // zeroth pole angle
    double sf;     // scaling factor

    omega = M_PI * relativeFrequency * 2;
    fomega = sin(omega);
    parg0 = M_PI / (double)(2 * order);

    sf = 1.0;
    for (k = 0; k < order / 2; ++k) {
      sf *= 1.0 + fomega * sin((double)(2 * k + 1) * parg0);
    }

    fomega = sin(omega / 2.0);

    if (order % 2) {
      sf *= fomega + cos(omega / 2.0);
    }
    sf = pow(fomega, order) / sf;

    return (sf);
  }

  static double getHighPassScalingFactor(int order, double relativeFrequency) {
    int k;         // loop variables
    double omega;  // M_PI * relativeFrequency
    double fomega; // function of omega
    double parg0;  // zeroth pole angle
    double sf;     // scaling factor

    omega = M_PI * relativeFrequency * 2;
    fomega = sin(omega);
    parg0 = M_PI / (double)(2 * order);

    sf = 1.0;
    for (k = 0; k < order / 2; ++k) {
      sf *= 1.0 + fomega * sin((double)(2 * k + 1) * parg0);
    }

    fomega = cos(omega / 2.0);

    if (order % 2) {
      sf *= fomega + sin(omega / 2.0);
    }
    sf = pow(fomega, order) / sf;

    return (sf);
  }

  static void getUnscaledHighPassCCoefficients(size_t order, int *ccof) {
    getUnscaledLowPassCCoefficients(order, ccof);

    for (size_t i = 0; i <= order; ++i) {
      if (i % 2) {
        ccof[i] = -ccof[i];
      }
    }
  }
};

} // namespace tdap

#endif /* TDAP_IIRBUTTERWORTH_HEADER_GUARD */
