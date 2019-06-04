//
// Created by michel on 10-9-16.
//

#include <jack/jack.h>
#include <iostream>
#include <cstdio>
#include <tdap/Delay.hpp>
#include <tdap/PerceptiveRms.hpp>
#include <tdap/TrueFloatingPointWindowAverage.hpp>

using namespace tdap;
using namespace std;

static void testTrueAverage()
{
    const size_t maxWindowSize = 1000 * 1000;
    const size_t errorTimeConstant = maxWindowSize * 100;
    const double relativeErrorNoise = 1e-6;
    const double amplitude = 1.0;
    const size_t largeWindow = 10000;
    const size_t smallWindow = 1000;
    const size_t printInterval = 100;

    TrueFloatingPointMovingAverage<double> average(
            maxWindowSize, errorTimeConstant, 10, relativeErrorNoise);

    average.setUsedWindows(2);
    average.setAverage(amplitude);

    average.setWindowSizeAndScale(0, smallWindow, 1.0);
    average.setWindowSizeAndScale(1, largeWindow, 4.0);
    average.setAverage(amplitude);

    printf("Start....\n");
    for (size_t i = 0; i < largeWindow * 4; i++) {
        const double input = i < largeWindow ? amplitude : 0.0;
        average.addInput(input);
        if (i % printInterval == 0) {
            printf("[%8zu] input=%8.3lf ; avg1=%18.16lf ; avg2=%18.16lf\n",
                    i, input, average.getAverage(0), average.getAverage(1));
        }
    }
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
	testTrueAverage();
	return 0;
}