//
// Created by michel on 16-02-20.
//

#include <tdap/VolumeMatrix.hpp>
#include <tdap/GroupVolume.hpp>

static constexpr size_t INPUTS = 4;
static constexpr size_t OUTPUTS = 3;

typedef tdap::VolumeMatrix<double, INPUTS, OUTPUTS> Matrix;

Matrix matrix;

bool same(double x, double y) {
  double norm = fabs(x) + fabs(y);
  if (norm < 1e-24) {
    return true;
  }
  return fabs(x - y) / norm < 1e-12;
}

void testEqual(const Matrix::OutputFrame &actual, const double *expected, const char *message) {
  for (size_t i = 0; i < actual.channels; i++) {
    if (!same(actual[i], expected[i])) {
      printf("%-20s: expected {", message);
      for (size_t j = 0; j < actual.channels; j++) {
        printf(" %5.2le", expected[j]);
      }
      printf(" } !- actual {");
      for (size_t j = 0; j < actual.channels; j++) {
        printf(" %5.2le", actual[j]);
      }
      printf(" }\n");
      return;
    }
  }
}

void testVolumeMatrix() {
  Matrix::InputFrame inputs;
  for (size_t i = 0; i < inputs.channels; i++) {
    inputs[i] = i + 1;
  }
  Matrix::OutputFrame outputs;
  {
    matrix.zero();
    outputs = matrix.apply(inputs);
    double expected[] = {0, 0, 0};
    testEqual(outputs, expected, "Zero");
  }
  {
    matrix.identity();
    outputs = matrix.apply(inputs);
    double expected[] = {1, 2, 3};
    testEqual(outputs, expected, "Identity");
  }
  {
    matrix.identity(2);
    outputs = matrix.apply(inputs);
    double expected[] = {2, 4, 6};
    testEqual(outputs, expected, "Identity");
  }
  {
    matrix.identityWrapped();
    outputs = matrix.apply(inputs);
    double expected[] = {5, 2, 3};
    testEqual(outputs, expected, "Identity");
  }
  {
    matrix.identity();
    outputs = matrix.applySeeded(inputs, 1);
    double expected[] = {2, 3, 4};
    testEqual(outputs, expected, "Identity");
  }
  {
    matrix.identityWrapped();
    outputs = matrix.applySeeded(inputs, 1);
    double expected[] = {6, 3, 4};
    testEqual(outputs, expected, "Identity");
  }
}
