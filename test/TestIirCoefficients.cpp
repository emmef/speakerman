//
// Created by michel on 13-02-20.
//
#include <chrono>
#include <iostream>
#include <tdap/Denormal.hpp>
#include <tdap/IirBiquad.hpp>
#include <tdap/IirCoefficients.hpp>

using Sample = double;
static constexpr size_t BUFFERSIZE = 10240;
static constexpr double sampleRate = 96000;
static constexpr double center = 1000;
static constexpr double gain = 2;
static constexpr double bandwidth = 1;

template <size_t CHANNELS, size_t ORDER> struct CoefficientMeasurements {
  using FSF = tdap::FixedSizeIirCoefficientFilter<Sample, CHANNELS, ORDER>;
  using FFF = tdap::FixedOrderIirFrameFilter<Sample, ORDER, CHANNELS>;
  using Frame =
      typename tdap::FixedOrderIirFrameFilter<Sample, ORDER, CHANNELS>::Frame;


  FSF multiFilter;
  FFF frameFilter;
  Frame inputBuffer[BUFFERSIZE];
  Frame outputBuffer[BUFFERSIZE];
  Frame refOutputBuffer[BUFFERSIZE];
  alignas(Frame::alignBytes) double inputArray[BUFFERSIZE * Frame::frameSize];
  alignas(Frame::alignBytes) double outputArray[BUFFERSIZE * Frame::frameSize];

  static bool compareFrames(Frame &f1, Frame &f2) {
    size_t digits = std::numeric_limits<Sample>::digits * 2 / 3;
    double epsilon = pow(0.5, digits);
    for (size_t channel = 0; channel < CHANNELS; channel++) {
      const Sample v1 = f1[channel];
      const Sample v2 = f2[channel];
      double delta = fabs(v1 - v2);
      double size = fabs(v1) + fabs(v2);
      double distance = size > 0 ? delta / size : delta > 0 ? 0 : 1;
      if (distance > 100 * epsilon) {
        return false;
      }
    }
    return true;
  }

  static constexpr size_t BUFFERSIZE = 10240;

  void randomizeInput() {
    for (size_t i = 0 ; i < BUFFERSIZE; i++) {
      double x = rand();
      for (size_t c = 0; c < CHANNELS; c++) {
        inputBuffer[i][c] = -0.5 + x / RAND_MAX;
      }
    }
    for (size_t i = 0, j = 0; i < BUFFERSIZE; i++, j += Frame::frameSize) {
      for (size_t n = 0; n < Frame::frameSize; n++) {
        inputArray[j + n] = inputBuffer[i][n];
      }
    }
  }

  void clearOutput() {
    for (size_t i = 0; i < BUFFERSIZE; i++) {
      inputBuffer[i].zero();
    }
    for (size_t n = 0; n < BUFFERSIZE * Frame::frameSize; n++) {
      outputArray[n] = 0;
    }
  }

  void calculateMulti() {
    multiFilter.reset();
    for (size_t sample = 0; sample < BUFFERSIZE; sample++) {
      Frame &input = inputBuffer[sample];
      Frame &output = outputBuffer[sample];
      for (size_t channel = 0; channel < CHANNELS; channel++) {
        output[channel] = multiFilter.filter(channel, input[channel]);
      }
    }
  }

  void calculateFrameShift() {
    frameFilter.clearHistory();
    for (size_t sample = 0; sample < BUFFERSIZE; sample++) {
      frameFilter.filter_history_shift(outputBuffer[sample], inputBuffer[sample]);
    }
  }

  void calculateFrameBlock() {
    frameFilter.clearHistory();
    frameFilter.filterHistoryZero(outputBuffer, inputBuffer, BUFFERSIZE);
  }

  void calculateBlock() {
    auto coeffs = frameFilter.coeffs;
    coeffs.template filterHistoryZero<CHANNELS>(outputArray, inputArray, BUFFERSIZE);
  }

  static void statistics(double *measurements, size_t repetitions,
                         double &average, double &spread) {
    average = 0;
    for (size_t rep = 0; rep < repetitions; rep++) {
      average += measurements[rep];
    }
    average /= repetitions;
    spread = 0;
    for (size_t rep = 0; rep < repetitions; rep++) {
      double d = measurements[rep] - average;
      spread += d * d;
    }
    spread /= (repetitions - 1);
    spread = 2.3 * sqrt(spread); // coarse student-t approach
  }

  static void printStats(const char *what, const double &average,
                         const double &spread) {
    printf("%-20s (%.1le +/- %.1le) (%.1lf%%)\n", what, average, spread,
           100 * spread / average);
  }

  class Calculation {
  protected:
    CoefficientMeasurements &owner_;

  public:
    Calculation(CoefficientMeasurements &owner) : owner_(owner) {
      owner.clearOutput();
    }
    virtual void calculate() = 0;
    virtual ~Calculation() = default;
  };

  class CalculateMulti : public Calculation {
  public:
    CalculateMulti(CoefficientMeasurements &owner) : Calculation(owner) {}
    void calculate() override { Calculation::owner_.calculateMulti(); }
  } calcMulti;
  class CalculateFrameBlock : public Calculation {
  public:
    CalculateFrameBlock(CoefficientMeasurements &owner)
        : Calculation(owner) {}
    void calculate() override { Calculation::owner_.calculateFrameBlock(); }
  } calcFrameBlock;
  class CalculateRawBlock : public Calculation {
  public:
    CalculateRawBlock(CoefficientMeasurements &owner) : Calculation(owner) {}
    void calculate() override { Calculation::owner_.calculateBlock(); }
  } calcRawBlock;
  class CalculateFrameShift : public Calculation {
  public:
    CalculateFrameShift(CoefficientMeasurements &owner)
        : Calculation(owner) {}
    void calculate() override { Calculation::owner_.calculateFrameShift(); }
  } calcFrameShift;

  double measure(Calculation &calculation) {
    clearOutput();
    auto now = std::chrono::system_clock::now();
    auto start = now;
    while ((start = std::chrono::system_clock::now()) == now)
      ;
    std::chrono::milliseconds approximateDuration(300);
    auto end = start;
    size_t iterations = 0;
    while ((end = std::chrono::system_clock::now()) - start <
           approximateDuration) {
      calculation.calculate();
      iterations++;
    }
    auto duration = end - start;
    return 1e-9 * duration.count() / iterations;
  }

  CoefficientMeasurements() :
  calcMulti(*this),
  calcFrameShift(*this),
  calcRawBlock(*this),
  calcFrameBlock(*this)
  {

  }

  void test() {
    tdap::ZFPUState state;
    auto multiwrap = multiFilter.coefficients_.wrap();

    tdap::BiQuad::setParametric(multiwrap, sampleRate, center, gain, bandwidth);
    tdap::BiQuad::setParametric(frameFilter.coeffs, sampleRate, center, gain, bandwidth);

    bool fail = false;

    struct Experiment {
      const char *name;
      Calculation &calculation;
      bool useFrames = true;
    };

    static constexpr size_t repetitions = 25;
    printf("\n *** Compare computation algorithms to filter sample-blocks with %zu "
           "channels. *** \n",
           CHANNELS);

    Experiment experiment[] = {{"Multi-channel naive", calcMulti},
//                               {"Raw data block", calcRawBlock, false},
                               {"Block of frames", calcFrameBlock},
//                               {"Frame history shift", calcFrameShift},
//                               {"Frame with time ptr", calcFrameTimePtr}
    };
    size_t count = sizeof(experiment) / sizeof(Experiment);

    randomizeInput();
    calculateMulti();
    for (size_t i = 0; i < BUFFERSIZE; i++) {
      refOutputBuffer[i] = outputBuffer[i];
    }
    for (size_t e = 1; e < count; e++) {
      experiment[e].calculation.calculate();
      if (!experiment[e].useFrames) {
        for (int i = 0, j = 0; i < BUFFERSIZE; i++, j += Frame::frameSize) {
          for (size_t channel = 0; channel < Frame::channels; channel++) {
            outputBuffer[i][channel] = outputArray[j + channel];
          }
        }
      }
      ptrdiff_t fault = -1;
      for (size_t n = ORDER; n < BUFFERSIZE; n++) {
        if (!compareFrames(outputBuffer[n], refOutputBuffer[n])) {
          fault = n;
          break;
        }
      }
      if (fault >= 0) {
        printf("Calculation via %s does not yield same result as %s.\n",
               experiment[e].name, experiment[0].name);
//        for (ptrdiff_t frame = fault - 2; frame < fault + ORDER + 1; frame++) {
//          if (frame < 0) {
//            continue;
//          }
//          printf("frame[%zu]", frame);
//          if (!compareFrames(outputBuffer[frame], refOutputBuffer[frame])) {
//            printf(" * ");
//          }
//          printf("\n\tref={");
//          for (size_t channel = 0; channel < Frame::channels; channel++) {
//            printf(" %10.3le", refOutputBuffer[frame][channel]);
//          }
//          printf(" }\n\tout={");
//          for (size_t channel = 0; channel < Frame::channels; channel++) {
//            printf(" %10.3le", outputBuffer[frame][channel]);
//          }
//          printf(" }\n");
//        }
      }
    }

    double naive[repetitions];
    for (size_t rep = 0; rep < repetitions; rep++) {
      randomizeInput();
      naive[rep] = measure(calcMulti);
    }
    double naiveAvg;
    double naiveSpread;
    statistics(naive, repetitions, naiveAvg, naiveSpread);
    printStats(experiment[0].name, naiveAvg, naiveSpread);

    for (size_t e = 1; e < count; e++) {
      double measurements[repetitions];
      double ratios[repetitions];
      randomizeInput();

      for (size_t rep = 0; rep < repetitions; rep++) {
        randomizeInput();
        double duration = measure(experiment[e].calculation);
        measurements[rep] = duration;
        ratios[rep] = naiveAvg / duration;
      }
      double measureAvg, measureSpread;
      double ratioAvg, ratioSpread;
      statistics(measurements, repetitions, measureAvg, measureSpread);
      statistics(ratios, repetitions, ratioAvg, ratioSpread);
      printf("Calculation-method: %s\n", experiment[e].name);
      printStats("- Absolute", measureAvg, measureSpread);
      printStats("- Relative", ratioAvg, ratioSpread);
    }
  }
};

CoefficientMeasurements<2, 2> x22;
CoefficientMeasurements<3, 2> x32;
CoefficientMeasurements<4, 2> x42;
CoefficientMeasurements<5, 2> x52;
CoefficientMeasurements<6, 2> x62;
CoefficientMeasurements<7, 2> x72;
CoefficientMeasurements<8, 2> x82;

void testIirCoefficientVariants() {

  x82.test();
  x42.test();
  x22.test();
  x32.test();
  x52.test();
  x62.test();
  x72.test();
}
