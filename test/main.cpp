//
// Created by michel on 10-9-16.
//

#define TDAP_FOLLOWERS_DEBUG_LOGGING 3

#include <jack/jack.h>
#include <iostream>
#include <cstdio>
#include <tdap/Delay.hpp>
#include <tdap/PerceptiveRms.hpp>
#include <tdap/PeakDetection.hpp>
#include <tdap/TrueFloatingPointWindowAverage.hpp>
#include <atomic>

using namespace tdap;
using namespace std;



static void testTrueAverage()
{
    const size_t maxWindowSize = 100;
    const size_t errorTimeConstant = maxWindowSize * 100;
    const double relativeErrorNoise = 1e-6;
    const double amplitude = 1.0;
    const size_t largeWindow = 100;
    const size_t smallWindow = 10;
    const size_t printInterval = 1;

    TrueFloatingPointWeightedMovingAverage<double> smallAverage(
            maxWindowSize, errorTimeConstant);
    TrueFloatingPointWeightedMovingAverage<double> largeAverage(
            maxWindowSize, errorTimeConstant);
    TrueFloatingPointWeightedMovingAverageSet<double> set(
            maxWindowSize, errorTimeConstant, 2, 0.0);

    smallAverage.setAverage(0);
    largeAverage.setAverage(0);
    set.setAverages(0);

    printf("\nSetting small average:\n");
    smallAverage.setWindowSize(smallWindow);
    printf("\nSetting large average:\n");
    largeAverage.setWindowSize(largeWindow);
    printf("\nSetting set, small average:\n");
    set.setWindowSizeAndScale(0, smallWindow, 1.0);
    printf("\nSetting set, large average:\n");
    set.setWindowSizeAndScale(1, largeWindow, 2.0);

    printf("Start....\n");
    for (size_t i = 0; i < largeWindow * 5; i++) {
        const double input = i > largeWindow && i <= 2*largeWindow ? amplitude : 0.0;
        smallAverage.addInput(input);
        largeAverage.addInput(input);
        double setAvg = set.addInputGetMax(input, 0.0);
        if (i % printInterval == 0) {
            printf("[%5zu] input=%8.3lf ; avg1=%18.16lf ; avg2=%18.16lf ; set1=%18.16lf ; set2=%18.16lf ; setMax=%18.16lf\n",
                    i,
                    input,
                    smallAverage.getAverage(), largeAverage.getAverage(),
                    set.getAverage(0), set.getAverage(1),
                    setAvg);
        }
    }
}

static constexpr size_t RANGE = 100;
static constexpr size_t THRESHOLD = 25;
static constexpr size_t WINDOW = 40;
static constexpr size_t PRINT_INTERVAL = 1;
static constexpr size_t INTERVAL = Value<size_t >::min(PRINT_INTERVAL, WINDOW);
static constexpr size_t RUNLENGTH = WINDOW * 10;
static constexpr size_t PERIODS = 5;
static constexpr size_t RANDOM_PEAK = 73;

static const double * getInput()
{
    static double * input = new double[RUNLENGTH];
    static std::atomic_flag generated;

    if (!generated.test_and_set()) {
        for (size_t i = 0; i < RUNLENGTH; i++) {
            input[i] = THRESHOLD * (1.0 + sin(M_2_PI * i / PERIODS) * rand() / RAND_MAX);
            if (i % RANDOM_PEAK == 0) {
                input[i] += 50;
            }
        }
    }

    return input;
}

static const double * getSpikedInput()
{
    static double * input = new double[RUNLENGTH];
    static std::atomic_flag generated;

    if (!generated.test_and_set()) {
        bool flip = false;
        int peak1 = -1;
        int peak2 = -1;
        size_t i;
        for (i = 0; i < 8 * RUNLENGTH / 10; i++) {
            if (i % RANDOM_PEAK == 0) {
                flip = !flip;
                input[i] == 75;
                peak1 = RANDOM_PEAK / 10;
            }
            else if (peak1 == 0) {
                input[i] = flip ? 80 : 70;
                peak2 = RANDOM_PEAK / 10;
            }
            else if (peak2 == 0) {
                input[i] = flip ? 70 : 80;
            }
            else {
                input[i] = THRESHOLD *
                           (1.0 + sin(M_2_PI * i / PERIODS));
            }
            peak1--;
            peak2--;
        }
        for (; i < RUNLENGTH; i++) {
            input[i] = 0;
        }
    }
    return input;
}

template<typename T, bool IS_NUMBER>
struct Zero;

template<typename T>
struct Zero<T, false>
{
    static const T zero() { T z; return z; }
};

template<typename T>
struct Zero<T, true>
{
    static const T zero() { return 0; }
};


template<typename T>
class Scenario
{
    using Zeroer = Zero<T, is_floating_point<T>::value || is_integral<T>::value>;

    T *data_ = nullptr;
    size_t capacity_ = 0;
    size_t size_ = 0;

    void ensureCapacity(size_t capacity)
    {
        if (capacity < capacity_) {
            return;
        }
        size_t newCapacity;
        if (capacity_ == 0) {
            newCapacity = Sizes::min(10, capacity + 1);
        }
        else {
            newCapacity = Sizes::max(capacity_ * 3 / 2, capacity + 1);
        }
        T * newData = new T[newCapacity];
        size_t i;
        for (i = 0; i < capacity_; i++) {
            newData[i] = data_[i];
        }
        for (; i < newCapacity; i++) {
            newData[i] = Zeroer::zero();
        }
        if (data_) {
            delete[] data_;
        }
        data_ = newData;
        capacity_ = newCapacity;
    }

    void ensureCapacityForIndex(size_t index)
    {
        ensureCapacity(index + 1);
    }
public:
    const size_t size() const { return size_; }

    T &operator[](size_t index)
    {
        ensureCapacityForIndex(index);
        if (index >= size_) {
            size_ = index + 1;
        }
        return data_[index];
    }

    const T operator()(size_t index) const {
        return data_[IndexPolicy::force(index, size_)];
    }

    void clear()
    {
        size_ = 0;
    }

    void enlarge(size_t with)
    {
        ensureCapacity(size_ + with);
        size_ += with;
    }

    void foreach(void (*action) (const T))
    {
        for (size_t i = 0; i < size(); i++)
        {
            action(operator()(i));
        }
    }

    ~Scenario()
    {
        if (data_) {
            delete[] data_;
        }
        capacity_ = 0;
        size_ = 0;
    }
};

struct Measurement
{
    ssize_t t = 0;
    double in = 0;
    double out1 = 0;
    double out2 = 0;

    void print() const {
        printf("%zi   %lf   %lf   %lf\n", t, in, out1, out2);
    }

};

static void printMeasurement(const Measurement m)
{
    m.print();
}

static void testTriangularFollower()
{
//    TriangularFollower<double> fast;

    static constexpr size_t ATTACK = 100;
    static constexpr size_t RELEASE = 200;
    static constexpr size_t SIZE = 1600;
    static constexpr double THRESHOLD = 100;
    static constexpr size_t SMOOTHING = 10;
    static constexpr double INTEGRATION = ATTACK / SMOOTHING;
    static constexpr size_t TOTAL_DELAY = ATTACK + INTEGRATION;

    TriangularFollower<double> follower1(SIZE);
    TriangularFollower<double> follower2(SIZE);

    follower1.setTimeConstantAndSamples(ATTACK, RELEASE, THRESHOLD);
    follower2.setTimeConstantAndSamples(ATTACK, RELEASE, THRESHOLD);

    IntegrationCoefficients<double> smooth(ATTACK / SMOOTHING);
    Scenario<double> input;
    Scenario<double> inputShort;

//    size_t scenario;
//
//    scenario = 5;
//    input[scenario] = 200;
//    input[scenario + 3] = 300;
//
//    scenario = 25;
//    input[scenario] = 200;
//    input[scenario + 3] = 185;
//    input[scenario + 5] = 120;
//
//    scenario = 45;
//    input[scenario] = 200;
//    input[scenario + 2] = 185;
//    input[scenario + 4] = 175;
//
//    scenario = 65;
//    input[scenario] = 200;
//    input[scenario + 1] = 201.3;
//    input[scenario + 2] = 202.2;
//    input[scenario + 3] = 203.1;
//    input[scenario + 4] = 204;

    for (size_t i = 0; i < SIZE; i++) {
        input[i] = 0.5 * THRESHOLD + THRESHOLD * rand() / RAND_MAX;
    }
    for (size_t i = 0; i < input.size() - 1; i++) {
        inputShort[i] = input(i);
    }

//    input[0] = 2 * THRESHOLD;

    input.enlarge(ATTACK + RELEASE);
    inputShort.enlarge(input.size());

    printf("\n# Triangular follower ***\n\n");

    Scenario<Measurement> output;

    double int1 = THRESHOLD;
    double int2 = THRESHOLD;
    double int3 = THRESHOLD;
    double max = THRESHOLD;

    for (size_t i = 0; i < input.size(); i++) {
//        double out1 = follower2.follow(inputShort(i));
//        double out2 = follower1.follow(input(i));
        double out1 = follower1.follow(input(i));
        smooth.integrate(out1, int1);
        smooth.integrate(int1, int2);
        smooth.integrate(int2, int3);
        double in = i < TOTAL_DELAY ? 0 : input(i - TOTAL_DELAY);
        ssize_t time = i;
        time -= ATTACK;
        double effective = THRESHOLD * in / int1;
        max = Floats::max(max, effective);
        output[i] = {time, max, int1, effective};
    }

    output.foreach(printMeasurement);

}

static void reachIngForFactors()
{
    static constexpr size_t RC = 100;
    static constexpr size_t MUL = 20;
    static constexpr size_t COUNT = RC * MUL;

    IntegrationCoefficients<double> integration(RC);
    double mem1 = 0.0;
    double mem2 = 0.0;

    for (size_t i = 1; i <= COUNT; i++) {
        double input = (-1.0 + i) / COUNT;
        integration.integrate(input, mem1);
        integration.integrate(mem1, mem2);
        printf("# \tin=%lf, mem1=%lf, mem2=%lf\n", input, mem1, mem2);
    }

    printf("# Double integration of linear [0..1] slope with %zu times integration time yields %lf/%lf\n", MUL, mem2, 1/mem2);
}


static void testPeakDetector()
{

    PeakMemory<double> memory(288);
    PeakDetector<double> detector(288, 0.5, 0.3, 1);

    char line[RANGE + 1];

    const double * input = getSpikedInput();
    size_t samples = WINDOW;
    memory.setSampleCount(WINDOW);
    static constexpr const char * DIGIT = " iP*";

//    for (size_t i = 0; i < RUNLENGTH; i++) {
//        double in = input[i];
//        double peak = memory.addSampleGetPeak(in);
//        double delayed = (i >= samples) ? input[i - samples] : 0.0;
//        double fault = delayed / peak;
//        if (fault > 1 || (i % INTERVAL == 0)) {
//            for (size_t at = 0; at < RANGE; at++) {
//                int dig = 0;
//                if (at == static_cast<size_t>(delayed)) {
//                    dig |= 1;
//                }
//                if (at == static_cast<size_t>(peak)) {
//                    dig |= 2;
//                }
//                line[at] = DIGIT[dig];
//            }
//            line[RANGE] = '\0';
//            if (fault > 0) {
//                printf("[%5zu]\t[%s] %6.04lf\n", i, line, fault);
//            }
//            else {
//                printf("[%5zu]\t[%s] %7s\n", i, line, "");
//            }
//        }
//    }

    memory.setSampleCount(WINDOW);
    samples = detector.setSamplesAndThreshold(WINDOW, THRESHOLD);

    printf("\n# Using LIMITER\n\n");
    printf("Sample Input Detect Output\n");
    static constexpr const char * DIGIT_LIMIT = " io*dDdD";
    double maxFault = 0;
    for (size_t i = 0; i < RUNLENGTH; i++) {
        double detect = detector.addSampleGetDetection(input[i]);
        double in = i >= samples ? input[i - samples] : 0.0;
        double gain = static_cast<double>(THRESHOLD) / detect;
        double out = in * gain;
        double fault = in / detect;
        printf("%5zu %6.02lf %6.02lf\n",
                       i, in, detect);

    }
    printf("Maximum fault: %lf\n", maxFault);
}


static void printDelayEntry(const typename MultiChannelAndTimeDelay<int>::Entry &entry)
{
	cout << "\tdelay=" << entry.delay_ << "; end=" << entry.end_ << "; write=" << entry.write_ << "; read=" << entry.read_ << endl;
}


static void printDelayState(const MultiChannelAndTimeDelay<int> &delay)
{
	cout << "Delay channels=" << delay.channels_ << "; max-channels=" << delay.maxChannels_ << endl;
	for (size_t i = 0, t = 0; t <= delay.maxDelay_; t++)  {
		cout << "\t[" << t << "]";
		for (size_t channel = 0; channel < delay.channels_; channel++, i++) {
			cout << " " << delay.buffer_[i];
		}
		cout << endl;
	}
	for (size_t channel = 0; channel < delay.channels_; channel++) {
		printDelayEntry(delay.entry_[channel]);
	}
};

void testMultiTimeDelay()
{
	MultiChannelAndTimeDelay<int> delay(4, 4);
	delay.setChannels(4);
	printDelayState(delay);
	delay.setDelay(0, 0);
	printDelayState(delay);
	delay.setDelay(1, 1);
	printDelayState(delay);
	delay.setDelay(2, 2);
	printDelayState(delay);
	delay.setDelay(3, 3);

	for (int i = 0; i < 100; i++) {
		cout << "channel 0: getAndSet(" << (i + 100) << ") = " << delay.setAndGet(0, i + 100) << endl;
		cout << "channel 1: getAndSet(" << (i + 200) << ") = " << delay.setAndGet(1, i + 200) << endl;
		cout << "channel 2: getAndSet(" << (i + 300) << ") = " << delay.setAndGet(2, i + 300) << endl;
		cout << "channel 3: getAndSet(" << (i + 400) << ") = " << delay.setAndGet(3, i + 400) << endl;
		printDelayState(delay);
		delay.next();
	}
}

int main(int c, const char *args[])
{
//	testMultiTimeDelay();
//	testTrueAverage();
        testTriangularFollower();
//        testPeakDetector();
        reachIngForFactors();
	return 0;
}