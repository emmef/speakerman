//
// Created by michel on 10-9-16.
//

#include <iostream>
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


int main(int c, const char *args[])
{
	testMultiBucketMean();
	return 0;
}