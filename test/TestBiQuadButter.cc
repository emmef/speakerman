/*
 * speakerman/TestBiQuadButter.h
 *
 * Added by michel on 2023-12-09
 * Copyright (C) 2015-2023 Michel Fleur.
 * Source https://github.com/emmef/speakerman
 * Email speakerman@emmef.org
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

#include "boost-unit-tests.h"
#include <tdap/IirBiquad.hpp>
#include <tdap/IirButterworth.hpp>
#include <cmath>

std::ostream &operator<<(std::ostream &out, const   tdap::FixedSizeIirCoefficients<double, 2> &coeffs) {
  out << "Coeff{order=" << coeffs.order();
  for (size_t i = 0; i <= coeffs.order(); i++) {
    out << "\tC" << i << "=" << coeffs.getC(i) << "  ";
  }
  for (size_t i = 1; i <= coeffs.order(); i++) {
    out << "\tD" << i << "=" << coeffs.getD(i) << "  ";
  }
  out << "}";
  return out;
}



class BiQuadButterScenario {
  static constexpr size_t POINTS = 11;
  static constexpr double SAMPLE_RATE = 256 * 256;
  static constexpr double PI_2 = M_PI * 2.0;
  double center_;
  double bandwidth_;
  tdap::FixedSizeIirCoefficients<double, 2> butter;
  tdap::FixedSizeIirCoefficients<double, 2> bi;

  void measure(double f, double *bi_result, double *bw_result) const {
    static constexpr size_t COUNT = SAMPLE_RATE * 3L;
    static constexpr size_t START = SAMPLE_RATE;
    tdap::FixedSizeIirCoefficientFilter<double, 1, 2> bi_filter;
    bi_filter.reset();
    bi_filter.coefficients_ = bi;
    tdap::FixedSizeIirCoefficientFilter<double, 1, 2> bw_filter;
    bw_filter.reset();
    bw_filter.coefficients_ = butter;

    double bi_maxOut = 0.0;
    double bw_maxOut = 0.0;
    double w = PI_2 * f;
    double t = 0;
    const size_t length = SAMPLE_RATE * SAMPLE_RATE * f;
    const size_t end = START + length;
//    const size_t interval = length / 30;
//    std::cout << "w=" << w << "; fc=" << center_ << "; f=" << f << "; f/fc=" << (f/center_) << std::endl;
    for (size_t i = 0; i < COUNT; i++) {
      const double x = sin(t);
      const double bi_y = bi_filter.filter(0, x);
      const double bw_y = bw_filter.filter(0, x);
      if (i > START) {
        if (fabs(bi_y) > bi_maxOut) {
          bi_maxOut = fabs(bi_y);
        }
        if (fabs(bw_y) > bw_maxOut) {
          bw_maxOut = fabs(bw_y);
        }
        if (i > end) {
          break;
        }
//        if ((i % interval) == 0) {
//          std::cout << t << " => " << x << " => " << y << " max " << maxOut << std::endl;
//        }
      }
      t += w;
      if (t > PI_2) {
        t -= PI_2;
      }
    }
    *bw_result = bw_maxOut;
    *bi_result = bi_maxOut;
  }

  double frequency(int index) const {
    return center_ * pow(2.0, index - 4);
  }

  void generate(double *bi_results, double *bw_results) const {
    for (size_t i = 0; i < POINTS; i++) {
      measure(frequency(i), bi_results + i, bw_results + i);
    }
  }

public:
  BiQuadButterScenario(double center, double bandwidth)
      : center_(center), bandwidth_(bandwidth) {
    auto bww = butter.wrap();
    tdap::Butterworth::create(bww, SAMPLE_RATE, SAMPLE_RATE * center_,
                              tdap::Butterworth::Pass::HIGH, 1.0);
    auto biw = bi.wrap();
    tdap::BiQuad::setHighPass(biw, SAMPLE_RATE, SAMPLE_RATE * center_, bandwidth_);
  }

  void test() const {
    double bi_results[POINTS];
    double bw_results[POINTS];
    generate(bi_results, bw_results);
    double prev_bi_result = 1.0;
    double prev_bw_result = 1.0;
    for (size_t i = 0; i < POINTS; i++) {
      double bi_ratio = bi_results[i] / prev_bi_result;
      double bw_ratio = bw_results[i] / prev_bw_result;
      double ratio_ratio = bi_ratio / bw_ratio;
      std::cout << "[" << i << "]=" << bi_results[i] << "\t(" << bi_ratio << "; " << bw_ratio << "; " << ratio_ratio << ")" << std::endl;
      prev_bi_result = bi_results[i];
      prev_bw_result = bw_results[i];
    }

    BOOST_CHECK_EQUAL(butter.getC(0), bi.getC(0));
    BOOST_CHECK_EQUAL(butter.getC(1), bi.getC(1));
    BOOST_CHECK_EQUAL(butter.getC(2), bi.getC(2));
    BOOST_CHECK_EQUAL(butter.getD(1), bi.getD(1));
    BOOST_CHECK_EQUAL(butter.getD(2), bi.getD(2));
  }

  void writeTo(std::ostream &out) const {
    out << "Scenario{" << std::endl;
    out << "\tbandwidth=" << bandwidth_ << ";" << std::endl;
    out << "\tbutter=" << butter << ";" << std::endl;
    out << "\tbiquad=" << bi << ";" << std::endl;
    out << "}";
  }
};

static std::ostream &operator<<(std::ostream &out, const BiQuadButterScenario &reader) {
  reader.writeTo(out);
  return out;
}

static const double get_relative_frequency_bandwidth_warp(double f_c) {
  static constexpr double frequency_fudge = 0.311971724033356;
  static constexpr double low_correction = 1.209553281779139;

  const double f_corrected = frequency_fudge / (f_c < 1e-8 ? 1e-8 : f_c);
  return low_correction * atan(f_corrected * f_corrected);
}

static std::vector<BiQuadButterScenario> generateBiquadScenarios() {
  std::vector<BiQuadButterScenario> result;
  const double f_c = 1.0 / 16;

//  const double fudge_bandwidth = 1.899955;
//
//
//  const double f_c_factor =  0.313488781452179;
//  const double warp = atan(f_c_factor / f_c);
//  const double warp_fudge = 0.742084789326878;
//  const double fudge_warp = warp * warp_fudge * fudge_bandwidth;
//  const double fudge_bandwidth = 1.90268;
//  const double f_c = 1.0 / 4;
//  const double tan_pi_3 = tan(M_PI / 3);
//  const double f_c_factor = tan_pi_3 / 4;
//  const double warp = atan(f_c_factor / f_c) / M_PI_2;
//  const double fudge_warp = warp * fudge_bandwidth;

  const double warp = get_relative_frequency_bandwidth_warp(f_c);
//  std::cout << "f_c=" << f_c << "; factor=" << f_c_factor << "; warp=" << warp << "; fudge=" << fudge_warp << std::endl;
  std::cout << "f_c=" << f_c << "; warp=" << warp << std::endl;
  result.push_back({ f_c, warp}); // 1.89995     1.898999   1.89838 ///*1.775*/  1.7774});

  // 1/1024     -> 1.89995
  // 1/16       -> 1.85155
  // 1/4        -> 1.20956

  // 1              7.79192e-17
  // 1.2            7.20065e-17
  // 1.205          7.1866e-17
  // 1.208          7.17818e-17
  // 1.209          7.17538e-17
  // 1.2095         7.17398e-17
  // 1.20955        7.17384e-17
  // 1.20956        7.17381e-17
  //            *** 7.17381e-17 ***
  // 1.20958        7.17375e-17
  // 1.2096         7.1737e-17
  // 1.2098         7.17314e-17
  // 1.21           7.17258e-17
  // 1.23           7.11681e-17
  // 1.25           7.06156e-17
  // 1.3            6.9256e-17
  // 1.5            6.41034e-17
  // 2              5.28541e-17

  return result;
}

BOOST_AUTO_TEST_SUITE(test_speakerman_TestBiQuadButter)

BOOST_DATA_TEST_CASE(testBandWidthOrderRelation, generateBiquadScenarios()) { sample.test(); }

BOOST_AUTO_TEST_SUITE_END()
