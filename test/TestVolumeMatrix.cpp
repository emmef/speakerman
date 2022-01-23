//
// Created by michel on 16-02-20.
//

#include "boost-unit-tests.h"
#include <tdap/VolumeMatrix.hpp>

#include <iomanip>
#include <sstream>

namespace {
static constexpr size_t INPUTS = 4;
static constexpr size_t OUTPUTS = 3;
static constexpr size_t ALIGN = 32;

typedef tdap::DefaultVolumeMatrix<double, ALIGN> Matrix;
typedef tdap::FixedVolumeMatrix<double, INPUTS, OUTPUTS, ALIGN> FixedMatrix;

bool same(double x, double y) {
  double norm = fabs(x) + fabs(y);
  if (norm < 1e-24) {
    return true;
  }
  return fabs(x - y) / norm < 1e-12;
}

template <size_t C, size_t A>
void testEqual(const tdap::AlignedArray<double, C, A> &actual,
               const double *expected) {
  std::ostringstream out;
  out << std::setw(9) << std::scientific;

  for (size_t i = 0; i < actual.size(); i++) {
    if (!same(actual[i], expected[i])) {
      out << "Expected\n\t";
      for (size_t j = 0; j < actual.size(); j++) {
        out << "  " << expected[j];
      }
      out << "\n\t!=\n\t";
      for (size_t j = 0; j < actual.size(); j++) {
        out << "  " << actual[j];
      }
      break;
    }
  }
  if (out.str().length() > 0) {
    BOOST_FAIL(out.str());
  }
}

template <size_t I, size_t O, size_t A>
void testEqualMatrix(const tdap::DefaultVolumeMatrix<double, A> &actual,
                     const char *expected, const char *description) {
  std::ostringstream out;
  out << "{";
  for (size_t output = 0; output < actual.outputs; output++) {
    if (output) {
      out << ", ";
    }
    out << "{";
    for (size_t input = 0; input < actual.volumes; input++) {
      if (input) {
        out << ", ";
      }
      int val = 0.5 + 10 * actual.get(output, input);
      out << (val / 10) << "." << (val % 10);
    }
    out << "}";
  }
  out << "}";
  if (out.str() != expected) {
    std::ostringstream message;
    message << description << ": expected \"" << expected << "\" != actual \""
            << out.str() << "\"";
    BOOST_FAIL(message.str());
  }
}

tdap::AlignedArray<double, INPUTS, ALIGN> generateInput() {
  tdap::AlignedArray<double, INPUTS, ALIGN> inputs;
  for (size_t i = 0; i < inputs.size(); i++) {
    inputs[i] = i + 1;
  }
  return inputs;
}

tdap::AlignedArray<double, INPUTS, ALIGN> getNumberedInput() {
  tdap::AlignedArray<double, INPUTS, ALIGN> frame = generateInput();
  return frame;
}

tdap::AlignedArray<double, INPUTS, ALIGN> getRandomInput() {
  tdap::AlignedArray<double, INPUTS, ALIGN> frame;
  for (double &x : frame) {
    x = (RAND_MAX - 2 * rand()) / (2 * RAND_MAX);
  }
  return frame;
}

tdap::AlignedArray<double, OUTPUTS, ALIGN> getRandomOutput() {
  tdap::AlignedArray<double, OUTPUTS, ALIGN> frame;
  for (double &x : frame) {
    x = (RAND_MAX - 2 * rand()) / (2 * RAND_MAX);
  }
  return frame;
}

} // end of anonymous namespace

BOOST_AUTO_TEST_SUITE(test_tdap_VolumeMatrix)

BOOST_AUTO_TEST_CASE(testVolumeMatrixZero) {
  const double expected[] = {0, 0, 0};
  const tdap::AlignedArray<double, INPUTS, ALIGN> inputs = getNumberedInput();
  tdap::AlignedArray<double, OUTPUTS, ALIGN> outputs;

  {
    Matrix matrix(INPUTS, OUTPUTS);
    outputs = getRandomOutput();
    matrix.zero();
    matrix.apply(outputs, inputs);
    testEqual(outputs, expected);
  }
  {
    FixedMatrix matrix;
    outputs = getRandomOutput();
    matrix.zero();
    matrix.apply(outputs, inputs);
    testEqual(outputs, expected);
  }
}

BOOST_AUTO_TEST_CASE(testVolumeMatrixIdentity) {
  const double expected[] = {1, 2, 3};
  const tdap::AlignedArray<double, INPUTS, ALIGN> inputs = getNumberedInput();
  tdap::AlignedArray<double, OUTPUTS, ALIGN> outputs;

  {
    Matrix matrix(INPUTS, OUTPUTS);
    outputs = getRandomOutput();
    matrix.identity();
    matrix.apply(outputs, inputs);
    testEqual(outputs, expected);
  }

  {
    FixedMatrix matrix;
    outputs = getRandomOutput();
    matrix.identity();
    matrix.apply(outputs, inputs);
    testEqual(outputs, expected);
  }
}
BOOST_AUTO_TEST_CASE(testVolumeMatrixScaledIdentity) {
  double expected[] = {2, 4, 6};
  const tdap::AlignedArray<double, INPUTS, ALIGN> inputs = getNumberedInput();
  tdap::AlignedArray<double, OUTPUTS, ALIGN> outputs;

  {
    Matrix matrix(INPUTS, OUTPUTS);
    outputs = getRandomOutput();
    matrix.identity(2);
    matrix.apply(outputs, inputs);
    testEqual(outputs, expected);
  }
  {
    FixedMatrix matrix;
    outputs = getRandomOutput();
    matrix.identity(2);
    matrix.apply(outputs, inputs);
    testEqual(outputs, expected);
  }
}
BOOST_AUTO_TEST_CASE(testVolumeMatrixWrappedIdentity) {
  double expected[] = {5, 2, 3};
  const tdap::AlignedArray<double, INPUTS, ALIGN> inputs = getNumberedInput();
  tdap::AlignedArray<double, OUTPUTS, ALIGN> outputs;

  {
    Matrix matrix(INPUTS, OUTPUTS);
    outputs = getRandomOutput();
    matrix.identityWrapped();
    matrix.apply(outputs, inputs);
    testEqual(outputs, expected);
  }
  {
    FixedMatrix matrix;
    outputs = getRandomOutput();
    matrix.identityWrapped();
    matrix.apply(outputs, inputs);
    testEqual(outputs, expected);
  }
}
BOOST_AUTO_TEST_SUITE_END()
