//
// Created by michel on 10-9-16.
//

#include <iostream>
#include <tdap/Delay.hpp>
#include <tdap/Rms.hpp>

using namespace tdap;
using namespace std;

void testMultiBucketMean()
{
	static constexpr size_t LEVELS = 3;
	static constexpr size_t BUCKETS = 4;
	MultiBucketMean<double, BUCKETS, LEVELS> means;
	cout << "Reset to zero" << endl;
	means.setValue(0);
	cout << "Add bucket value of one iteratively" << endl;

	for (size_t i = 0; i < 10; i++) {
		means.addBucketValue(1);
		FixedSizeArray<FixedSizeArray<double, BUCKETS>, LEVELS> buckets = means.getBuckets();

		cout << "[" << i << "]" << endl;

		for (size_t level = 0; level < LEVELS; level++) {
			cout << "\t[" << level << "]";
			for (size_t bucket = 0; bucket < BUCKETS; bucket++) {
				cout << " " << buckets[level][bucket];
			}
			cout << " (mean=" << means.getMean(level) << ")" << endl;
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
	testMultiBucketMean();
	testMultiTimeDelay();
	return 0;
}