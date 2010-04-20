// TestSample.h

#include <cxxtest/TestSuite.h>

#include <cstdio>
#include "Collection.h"
#include "MarSystemManager.h"
#include "CommandLineOptions.h"

#include "RunningStatistics.h"

// Include TestRunningAutocorrelation.h for helper functions for sum of ints, etc.
#include "TestRunningAutocorrelation.h"

#include <vector>

using namespace std;
using namespace Marsyas;

const mrs_real EPSILON = 1e-6;

class RunningStatistics_runner: public CxxTest::TestSuite
{
public:
	realvec in, out;
	MarSystemManager mng;
	RunningStatistics * rs;

	void setUp()
	{
		// Use the normal way for getting a Marsystem from the MarSystemManager
		// to make sure we don't bypass crucial things that should work
		// (e.g. the copy constructor).
		rs = (RunningStatistics*) mng.create("RunningStatistics",
				"runningstatistics");
	}

	/**
	 * Test the default flow settings.
	 */
	void test_default_flow_settings()
	{
		// Set up the input flow.
		rs->updctrl("mrs_natural/inObservations", 3);
		rs->updctrl("mrs_natural/inSamples", 128);
		rs->updctrl("mrs_string/inObsNames", "foo0,foo1,foo2,");
		// Check the output flow.
		TS_ASSERT_EQUALS(rs->getControl("mrs_natural/onObservations")->to<mrs_natural>(), 6);
		TS_ASSERT_EQUALS(rs->getControl("mrs_natural/onSamples")->to<mrs_natural>(), 1);
		TS_ASSERT_EQUALS(rs->getControl("mrs_string/onObsNames")->to<mrs_string>(), "RunningMean_foo0,RunningMean_foo1,RunningMean_foo2,RunningStddev_foo0,RunningStddev_foo1,RunningStddev_foo2,");
	}

	/**
	 * Helper function for customized flows.
	 */
	void _test_flow_settings(mrs_bool enable_mean = true,
			mrs_bool enable_stddev = true, mrs_bool enable_skewness = false)
	{
		mrs_natural inObservations = 3;
		// Set up the input flow.
		rs->updctrl("mrs_natural/inObservations", inObservations);
		rs->updctrl("mrs_natural/inSamples", 128);
		rs->updctrl("mrs_string/inObsNames", "foo0,foo1,foo2,");
		rs->updctrl("mrs_bool/enableMean", enable_mean);
		rs->updctrl("mrs_bool/enableStddev", enable_stddev);
		rs->updctrl("mrs_bool/enableSkewness", enable_skewness);
		// Check the output flow.
		mrs_natural fanout = (mrs_natural) enable_mean
				+ (mrs_natural) enable_stddev + (mrs_natural) enable_skewness;
		TS_ASSERT_EQUALS(rs->getControl("mrs_natural/onObservations")->to<mrs_natural>(), fanout * inObservations);
		TS_ASSERT_EQUALS(rs->getControl("mrs_natural/onSamples")->to<mrs_natural>(), 1);
		mrs_string onObsNames("");
		if (enable_mean)
		{
			onObsNames += "RunningMean_foo0,RunningMean_foo1,RunningMean_foo2,";
		}
		if (enable_stddev)
		{
			onObsNames
					+= "RunningStddev_foo0,RunningStddev_foo1,RunningStddev_foo2,";
		}
		if (enable_skewness)
		{
			onObsNames
					+= "RunningSkewness_foo0,RunningSkewness_foo1,RunningSkewness_foo2,";
		}
		TS_ASSERT_EQUALS(rs->getControl("mrs_string/onObsNames")->to<mrs_string>(), onObsNames);
	}

	void test_flow_settings()
	{
		for (mrs_natural enable_mean = 0; enable_mean < 2; enable_mean++)
		{
			for (mrs_natural enable_stddev = 0; enable_stddev < 2; enable_stddev++)
			{
				for (mrs_natural enable_skewness = 0; enable_skewness < 2; enable_skewness++)
				{
					_test_flow_settings(enable_mean, enable_stddev,
							enable_skewness);
				}
			}
		}
	}

	/**
	 * Helper function for setting up the MarSystem and its processing.
	 */
	void _test_process(mrs_natural inObservations, mrs_natural inSamples,
			mrs_natural onObservations, mrs_bool enable_mean,
			mrs_bool enable_stddev, mrs_bool enable_skewness)
	{
		mrs_natural onSamples = 1;
		// Set up the input flow.
		rs->updctrl("mrs_natural/inObservations", inObservations);
		rs->updctrl("mrs_natural/inSamples", inSamples);
		rs->updctrl("mrs_bool/enableMean", enable_mean);
		rs->updctrl("mrs_bool/enableStddev", enable_stddev);
		rs->updctrl("mrs_bool/enableSkewness", enable_skewness);
		// Check the output flow.
		TS_ASSERT_EQUALS(rs->getControl("mrs_natural/onObservations")->to<mrs_natural>(), onObservations);
		TS_ASSERT_EQUALS(rs->getControl("mrs_natural/onSamples")->to<mrs_natural>(), onSamples);

		// Allocate the input and output slices.
		in.create(inObservations, inSamples);
		out.create(onObservations, onSamples);

		// Fill the input slice.
		for (mrs_natural o = 0; o < inObservations; o++)
		{
			for (mrs_natural t = 0; t < inSamples; t++)
			{
				in(o, t) = mrs_natural(o > t);
			}
		}

		// Process.
		rs->myProcess(in, out);
	}

	/**
	 * Test mean.
	 */
	void test_process_mean()
	{
		mrs_natural inObservations = 10;
		mrs_natural inSamples = 10;
		mrs_natural onObservations = inObservations;
		_test_process(inObservations, inSamples, onObservations, true, false,
				false);

		// Check output slice.
		for (mrs_natural i = 0; i < inObservations; i++)
		{
			mrs_real mean = (mrs_real) i / inSamples;
			TS_ASSERT_EQUALS(out(i, 0), mean);
		}
	}

	/**
	 * Test stddev.
	 */
	void test_process_stddev()
	{
		mrs_natural inObservations = 10;
		mrs_natural inSamples = 10;
		mrs_natural onObservations = inObservations;
		_test_process(inObservations, inSamples, onObservations, false, true,
				false);

		// Check output slice.
		mrs_real mean, var, stddev;
		for (mrs_natural i = 0; i < inObservations; i++)
		{
			mean = (mrs_real) i / inSamples;
			var = (i * (1 - mean) * (1 - mean)
					+ ((inSamples - i) * mean * mean)) / inSamples;
			stddev = sqrt(var);
			TS_ASSERT_DELTA(out(i, 0), stddev, EPSILON);
		}
	}

	/**
	 * Test skewness.
	 */
	void test_process_skewness()
	{
		mrs_natural inObservations = 10;
		mrs_natural inSamples = 10;
		mrs_natural onObservations = inObservations;
		_test_process(inObservations, inSamples, onObservations, false, false,
				true);

		// Check output slice.
		mrs_real mean, var, stddev, skewness;
		for (mrs_natural i = 0; i < inObservations; i++)
		{
			mean = (mrs_real) i / inSamples;
			var = (i * (1 - mean) * (1 - mean)
					+ ((inSamples - i) * mean * mean)) / inSamples;
			stddev = sqrt(var);
			skewness = (i * (1 - mean) * (1 - mean) * (1 - mean) + ((inSamples
					- i) * -mean * -mean * -mean)) / inSamples;
			skewness = var > 0.0 ? skewness / (var * stddev) : 0.0;
			TS_ASSERT_DELTA(out(i, 0), skewness, EPSILON);
		}
	}

	void test_process_mean_and_stddev()
	{
		mrs_natural inObservations = 10;
		mrs_natural inSamples = 10;
		mrs_natural onObservations = 2 * inObservations;
		_test_process(inObservations, inSamples, onObservations, true, true,
				false);

		// Check output slice.
		mrs_real mean, var, stddev;
		for (mrs_natural i = 0; i < inObservations; i++)
		{
			mean = (mrs_real) i / inSamples;
			var = (i * (1 - mean) * (1 - mean)
					+ ((inSamples - i) * mean * mean)) / inSamples;
			stddev = sqrt(var);
			TS_ASSERT_DELTA(out(i, 0), mean, EPSILON);
			TS_ASSERT_DELTA(out(inObservations + i, 0), stddev, EPSILON);
		}
	}

	void test_process_stddev_and_skewness()
	{
		mrs_natural inObservations = 10;
		mrs_natural inSamples = 10;
		mrs_natural onObservations = 2 * inObservations;
		_test_process(inObservations, inSamples, onObservations, false, true,
				true);

		// Check output slice.
		mrs_real mean, var, stddev, skewness;
		for (mrs_natural i = 0; i < inObservations; i++)
		{
			mean = (mrs_real) i / inSamples;
			var = (i * (1 - mean) * (1 - mean)
					+ ((inSamples - i) * mean * mean)) / inSamples;
			stddev = sqrt(var);
			skewness = (i * (1 - mean) * (1 - mean) * (1 - mean) + ((inSamples
					- i) * -mean * -mean * -mean)) / inSamples;
			skewness = var > 0.0 ? skewness / (var * stddev) : 0.0;
			TS_ASSERT_DELTA(out(i, 0), stddev, EPSILON);
			TS_ASSERT_DELTA(out(inObservations + i, 0), skewness, EPSILON);
		}
	}

	void test_process_mean_and_stddev_and_skewness()
	{
		mrs_natural inObservations = 10;
		mrs_natural inSamples = 10;
		mrs_natural onObservations = 3 * inObservations;
		_test_process(inObservations, inSamples, onObservations, true, true,
				true);

		// Check output slice.
		mrs_real mean, var, stddev, skewness;
		for (mrs_natural i = 0; i < inObservations; i++)
		{
			mean = (mrs_real) i / inSamples;
			var = (i * (1 - mean) * (1 - mean)
					+ ((inSamples - i) * mean * mean)) / inSamples;
			stddev = sqrt(var);
			skewness = (i * (1 - mean) * (1 - mean) * (1 - mean) + ((inSamples
					- i) * -mean * -mean * -mean)) / inSamples;
			skewness = var > 0.0 ? skewness / (var * stddev) : 0.0;
			TS_ASSERT_DELTA(out(i, 0), mean, EPSILON);
			TS_ASSERT_DELTA(out(inObservations + i, 0), stddev, EPSILON);
			TS_ASSERT_DELTA(out(2*inObservations + i, 0), skewness, EPSILON);
		}
	}

};
