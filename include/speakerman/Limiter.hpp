/*
 * Limiter.hpp
 *
 * Part of 'Speaker management system'
 *
 * Copyright (C) 2013 Michel Fleur.
 * https://github.com/emmef/simpledsp
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

#ifndef SMS_SPEAKERMAN_LIMITER_GUARD_H_
#define SMS_SPEAKERMAN_LIMITER_GUARD_H_

#include <simpledsp/SingleReadDelay.hpp>
#include <simpledsp/Integrator.hpp>
#include <simpledsp/SampleAndHold.hpp>
#include <simpledsp/AttackReleaseIntegrator.hpp>
#include <simpledsp/Values.hpp>
#include <simpledsp/ReciprocalAmplifier.hpp>

namespace speakerman {

class LimiterSettings
{
	accurate_t peak_attack = 0.003;
	accurate_t peak_hold = 0.010;
	accurate_t peak_release = 0.006;
	accurate_t fast_rc = 0.150;
	accurate_t slow_rc = 2.000;
	accurate_t slow_scale = 2.000;
	accurate_t hard_threshold = 0.9;

	inline static accurate_t bound(accurate_t value, accurate_t min, accurate_t max) {
		return Values::Accurate::clamp(value, min, max);
	}

	LimiterSettings &setConstraints()
	{
		peak_hold = bound(peak_hold, peak_attack * 5, 0.050);
		peak_release = bound(peak_release, peak_attack, 0.050);

		fast_rc = bound(fast_rc, peak_hold, 0.5);
		slow_rc = bound(slow_rc, fast_rc * 2, 4.0);
		slow_scale = bound(slow_scale, 1.0, 4.0);

		return *this;
	}

	void assign(const LimiterSettings &source)
	{
		peak_attack = source.peak_attack;
		peak_hold = source.peak_hold;
		peak_release = source.peak_release;
		fast_rc = source.fast_rc;
		slow_rc = source.slow_rc;
		slow_scale = source.slow_scale;
		hard_threshold = source.hard_threshold;
	}

public:
	LimiterSettings() {}
	LimiterSettings(const LimiterSettings &source)
	{
		assign(source);
	}
	void operator = (const LimiterSettings &source)
	{
		assign(source);
	}
	accurate_t peakAttack() const
	{
		return peak_attack;
	}
	accurate_t peakRelease() const
	{
		return peak_release;
	}
	accurate_t peakHold() const
	{
		return peak_hold;
	}
	accurate_t fastRc() const
	{
		return fast_rc;
	}
	accurate_t slowRc() const
	{
		return slow_rc;
	}
	accurate_t slowScale() const
	{
		return slow_scale;
	}
	accurate_t hardThreshold() const
	{
		return hard_threshold;
	}
	LimiterSettings &setPeakAttack(accurate_t newValue)
	{
		peak_attack = bound(newValue, 0.001, 0.05);
		return setConstraints();
	}
	LimiterSettings &setPeakRelease(accurate_t newValue)
	{
		peak_release = bound(newValue, 0.001, 0.05);
		return setConstraints();
	}
	LimiterSettings &setPeakHold(accurate_t newValue)
	{
		peak_attack = bound(newValue, 0.005, 0.05);
		return setConstraints();
	}
	LimiterSettings &setFastRc(accurate_t newValue)
	{
		peak_attack = bound(newValue, 0.005, 0.5);
		return setConstraints();
	}
	LimiterSettings &setSlowRc(accurate_t newValue)
	{
		peak_attack = bound(newValue, 0.5, 4.0);
		return setConstraints();
	}
	LimiterSettings &setSlowScale(accurate_t newValue)
	{
		peak_attack = bound(newValue, 1, 4);
		return setConstraints();
	}
	LimiterSettings &setHardThreshold(accurate_t newValue)
	{
		hard_threshold = bound(newValue, 0.001, 1.0);

		return setConstraints();
	}
};

class LimiterSettingsWithThreshold
{
	accurate_t soft_threshold = 0.5;

public:
	const LimiterSettings &settings;

	LimiterSettingsWithThreshold(const LimiterSettings &linked) :
		settings(linked)
	{

	}
	LimiterSettingsWithThreshold(const LimiterSettingsWithThreshold &source) :
		settings(source.settings), soft_threshold(source.soft_threshold)
	{

	}
	void setSoftThreshold(accurate_t newValue)
	{
		soft_threshold = Values::Accurate::clamp(newValue, 0.001, settings.hardThreshold());
	}
	accurate_t softThreshold() const
	{
		return soft_threshold;
	}
	accurate_t peakAttack() const
	{
		return settings.peakAttack();
	}
	accurate_t peakRelease() const
	{
		return settings.peakRelease();
	}
	accurate_t peakHold() const
	{
		return settings.peakHold();
	}
	accurate_t fastRc() const
	{
		return settings.fastRc();
	}
	accurate_t slowRc() const
	{
		return settings.slowRc();
	}
	accurate_t slowScale() const
	{
		return settings.slowScale();
	}
	accurate_t hardThreshold() const
	{
		return settings.hardThreshold();
	}
};

class Limiter
{
	const LimiterSettingsWithThreshold &config;

	frequency_t sampleFrequency;
	accurate_t hardThreshold = 0.9;

	accurate_t softThreshold = 0.5;
	Integrator softThresholdIntegrator;

	accurate_t slowScale = 2.0;
	Integrator slowScaleIntegrator;

	AttackReleaseIntegrator peakIntegrator;
	SampleAndHold peakSampleAndHold;

	Integrator fastIntegrator;
	Integrator slowIntegrator;

	ReciprocalAmplifier reciprocalAmplifier;
	accurate_t amplification;
	accurate_t minRamp;
	accurate_t maxRamp;

public:
	Limiter(const LimiterSettingsWithThreshold &settings) :
		config(settings),
		softThresholdIntegrator(1),
		slowScaleIntegrator(1),
		peakIntegrator(1, 1),
		peakSampleAndHold(1),
		fastIntegrator(1),
		slowIntegrator(1),
		reciprocalAmplifier(0.5, 1.0)
	{
		LimiterSettingsWithThreshold cfg = settings;
		softThresholdIntegrator.value = cfg.softThreshold();
		slowScale = cfg.settings.slowScale();
	}

	/**
	 * Reconfigure according to updated configuration
	 */
	void reconfigure()
	{
		// Integrators have half-time-factors, because: square -> integrate -> root
		hardThreshold = config.hardThreshold();
		softThreshold = config.softThreshold();
		softThresholdIntegrator.multipliers.setCharacteristicSample(Frequency::numberOfSamples(sampleFrequency, 0.1));
		slowScale = config.slowScale();
		slowScaleIntegrator.multipliers.setCharacteristicSample(0.5 * Frequency::numberOfSamples(sampleFrequency, 0.1));

		peakIntegrator.attack.setCharacteristicSample(0.5 * Frequency::numberOfSamples(sampleFrequency, config.peakAttack()));
		peakIntegrator.release.setCharacteristicSample(0.5 * Frequency::numberOfSamples(sampleFrequency, config.peakRelease()));
		peakSampleAndHold.setHoldSamples(Frequency::numberOfSamples(sampleFrequency, config.peakHold()));

		fastIntegrator.multipliers.setCharacteristicSample(Frequency::numberOfSamples(sampleFrequency, config.fastRc()));
		slowIntegrator.multipliers.setCharacteristicSample(Frequency::numberOfSamples(sampleFrequency, config.slowRc()));

		reciprocalAmplifier.setMaxAmplification(1.0);
		maxRamp = config.hardThreshold();
		minRamp = -maxRamp;
	}

	/**
	 * Reconfigure according to updated sampel rate
	 */
	void reconfigure(frequency_t sampleRate)
	{
		sampleFrequency = Frequency::validRate(sampleRate);

		reconfigure();
	}

	void detect(sample_t squaredDetection)
	{
		reciprocalAmplifier.setThreshhold(softThresholdIntegrator.integrate(softThreshold));

		accurate_t hold = peakSampleAndHold.sampleAndHold(squaredDetection);
		accurate_t fast = fastIntegrator.integrate(squaredDetection);
		accurate_t slow = slowIntegrator.integrate(squaredDetection) * slowScaleIntegrator.integrate(slowScale);

		accurate_t detection = sqrt(peakIntegrator.integrate(max(max(hold, fast), slow)));

		amplification = reciprocalAmplifier.getAmplification(detection);
	}

	sample_t getLimited(sample_t input)
	{
		return Values::Sample::clamp(amplification * input, minRamp, maxRamp);
	}
};

} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_LIMITER_GUARD_H_ */
