//
// Created by michel on 16-02-20.
//

#include <tdap/GroupVolume.hpp>
#include <tdap/VolumeMatrix.hpp>

#include <sstream>

namespace {
static constexpr size_t INPUTS = 4;
static constexpr size_t OUTPUTS = 3;

typedef tdap::VolumeMatrix<double, INPUTS, OUTPUTS> Matrix;
typedef tdap::GroupVolumeMatrix<double, INPUTS, OUTPUTS> Groups;

bool same(double x, double y) {
  double norm = fabs(x) + fabs(y);
  if (norm < 1e-24) {
    return true;
  }
  return fabs(x - y) / norm < 1e-12;
}

template <size_t C, size_t A>
void testEqual(const tdap::AlignedFrame<double, C, A> &actual,
               const double *expected, const char *message) {
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

template <size_t I, size_t O, size_t A>
void testEqualMatrix(const tdap::VolumeMatrix<double, I, O, A> &actual,
                     const char *expected, const char *message) {
  std::ostringstream out;
  out << "{";
  for (size_t output = 0; output < actual.outputs; output++) {
    if (output) {
      out << ", ";
    }
    out << "{";
    for (size_t input = 0; input < actual.inputs; input++) {
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
    printf("%-20s:\n\texpected %s != actual %s\n", message, expected,
           out.str().c_str());
  }
}

} // end of anonymous namespace

void testVolumeMatrix() {
  Matrix matrix;
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

void testGroupVolumeMatrix() {
  Matrix volumes;
  Groups groups;
  {
    groups.volumes.zero();
    groups.apply(volumes);
    const char *expected = "{{0.0, 0.0, 0.0, 0.0}, "
                           "{0.0, 0.0, 0.0, 0.0}, "
                           "{0.0, 0.0, 0.0, 0.0}}";
    testEqualMatrix(volumes, expected, "Zero groups, zero volume compare");
  }
  {
    groups.volumes.setAll(1.0);
    groups.apply(volumes);
    const char *expected = "{{0.0, 0.0, 0.0, 0.0}, "
                           "{0.0, 0.0, 0.0, 0.0}, "
                           "{0.0, 0.0, 0.0, 0.0}}";
    testEqualMatrix(volumes, expected, "Zero groups, all volumes");
  }
  {
    groups.volumes.identity(1);
    groups.inputGroups.map(0, 1);
    groups.inputGroups.map(0, 2);
    groups.outputGroups.map(0, 2);

    groups.apply(volumes);
    const char *expected = "{{0.0, 0.0, 0.0, 0.0}, "
                           "{0.0, 0.0, 0.0, 0.0}, "
                           "{0.0, 1.0, 1.0, 0.0}}";
    testEqualMatrix(volumes, expected, "ID groups, stereo to mono");
  }
  {
    groups.volumes.identity(1);
    groups.inputGroups.map(0, 1);
    groups.inputGroups.map(0, 2);
    groups.inputGroups.mapUnmapped(1);
    groups.inputGroups.map(1, 0);
    groups.inputGroups.map(1, 3);

    groups.outputGroups.map(0, 2);
    groups.outputGroups.map(1, 0);
    groups.outputGroups.map(1, 1);
    // group 0 has channel 2
    // group 1 has channel 0 and 1

    groups.apply(volumes);
    const char *expected = "{{1.0, 0.0, 0.0, 0.0}, "
                           "{0.0, 0.0, 0.0, 1.0}, "
                           "{0.0, 1.0, 1.0, 0.0}}";
    testEqualMatrix(volumes, expected, "i0->(1,2); i1->(0,3); o0->(2); o1->(0,1); v=ID");

    groups.volumes.set(1, 0, 5);
    groups.apply(volumes);
    expected = "{{1.0, 5.0, 0.0, 0.0}, "
               "{0.0, 0.0, 5.0, 1.0}, "
               "{0.0, 1.0, 1.0, 0.0}}";
    testEqualMatrix(volumes, expected, "i0->(1,2); i1->(0,3); o0->(2); o1->(0,1); v=ID + o1+=5*i0");
  }
}
