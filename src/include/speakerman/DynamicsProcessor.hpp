/*
 * SignalGroup.hpp
 *
 * Part of 'Speaker management system'
 *
 * Copyright (C) 2013-2014 Michel Fleur.
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

#ifndef SMS_SPEAKERMAN_SIGNALGROUP_GUARD_H_
#define SMS_SPEAKERMAN_SIGNALGROUP_GUARD_H_

#include <cmath>
#include <tdap/Delay.hpp>
#include <tdap/AdvancedRmsDetector.hpp>
#include <tdap/MemoryFence.hpp>
#include <tdap/Weighting.hpp>
#include <tdap/Noise.hpp>
#include <tdap/Crossovers.hpp>
#include <tdap/Transport.hpp>
#include <speakerman/SpeakermanRuntimeData.hpp>
#include <xmmintrin.h>

namespace speakerman {

    using namespace tdap;

// RAII FPU state class, sets FTZ and DAZ and rounding, no exceptions 
// Adapted from code by mystran @ kvraudio
// http://www.kvraudio.com/forum/viewtopic.php?t=312228&postdays=0&postorder=asc&start=0

    class ZFPUState
    {
    private:
        unsigned int sse_control_store;

    public:
        enum Rounding
        {
            kRoundNearest = 0,
            kRoundNegative,
            kRoundPositive,
            kRoundToZero,
        };

        ZFPUState(Rounding mode = kRoundToZero)
        {
            sse_control_store = _mm_getcsr();

            // bits: 15 = flush to zero | 6 = denormals are zero
            // bitwise-OR with exception masks 12:7 (exception flags 5:0)
            // rounding 14:13, 00 = nearest, 01 = neg, 10 = pos, 11 = to zero
            // The enum above is defined in the same order so just shift it up
            _mm_setcsr(0x8040 | 0x1f80 | ((unsigned int) mode << 13));
        }

        ~ZFPUState()
        {
            // clear exception flags, just in case (probably pointless)
            _mm_setcsr(sse_control_store & (~0x3f));
        }
    };


    template<
            typename T, size_t CHANNELS_PER_GROUP, size_t GROUPS, size_t CROSSOVERS>
    class DynamicsProcessor
    {
        static_assert(is_floating_point<T>::value,
                      "expected floating-point value parameter");
    public:
        static constexpr size_t INPUTS = GROUPS * CHANNELS_PER_GROUP;
        // bands are around crossovers
        static constexpr size_t BANDS = CROSSOVERS + 1;
        // multiplex by frequency bands
        static constexpr size_t CROSSOVER_OUPUTS = INPUTS * BANDS;
        // sub-woofer groupChannels summed, so don't process CROSSOVER_OUPUTS groupChannels
        static constexpr size_t PROCESSING_CHANNELS = 1 + CROSSOVERS * INPUTS;
        // RMS detection are per group, not per channel (and only one for sub)
        static constexpr size_t DETECTORS = 1 + CROSSOVERS * GROUPS;
        // Limiters are per group and sub
        static constexpr size_t LIMITERS = 1 + GROUPS;
        // Limiters are per group and sub
        static constexpr size_t DELAY_CHANNELS =
                1 + GROUPS * CHANNELS_PER_GROUP;
        // OUTPUTS
        static constexpr size_t OUTPUTS = INPUTS + 1;

        static constexpr double GROUP_MAX_DELAY = GroupConfig::MAX_DELAY;
        static constexpr double LIMITER_MAX_DELAY = 0.01;
        static constexpr double RMS_MAX_DELAY = 0.01;

        static constexpr size_t GROUP_MAX_DELAY_SAMPLES =
                0.5 + 192000 * GroupConfig::MAX_DELAY;
        static constexpr size_t LIMITER_MAX_DELAY_SAMPLES =
                0.5 + 192000 * LIMITER_MAX_DELAY;
        static constexpr size_t RMS_MAX_DELAY_SAMPLES =
                0.5 + 192000 * RMS_MAX_DELAY;
        static constexpr double CHANNEL_ADD_FACTOR = 1.0 / CHANNELS_PER_GROUP;
        static constexpr double CHANNEL_RMS_FACTOR = (CHANNEL_ADD_FACTOR);


        using CrossoverFrequencies = FixedSizeArray<T, CROSSOVERS>;
        using ThresholdValues = FixedSizeArray<T, LIMITERS>;
        using Configurable = SpeakermanRuntimeConfigurable<T, GROUPS, BANDS,
                                                           CHANNELS_PER_GROUP>;
        using ConfigData = SpeakermanRuntimeData<T, GROUPS, BANDS>;

        class GroupDelay : public MultiChannelAndTimeDelay<T>
        {
        public:
            GroupDelay() : MultiChannelAndTimeDelay<T>(DELAY_CHANNELS,
                                                       GROUP_MAX_DELAY_SAMPLES)
            {}
        };

        class LimiterDelay : public MultiChannelDelay<T>
        {
        public:
            LimiterDelay() : MultiChannelDelay<T>(DELAY_CHANNELS,
                                                  LIMITER_MAX_DELAY_SAMPLES)
            {}
        };

        class RmsDelay : public MultiChannelDelay<T>
        {
        public:
            RmsDelay() : MultiChannelDelay<T>(PROCESSING_CHANNELS,
                                              RMS_MAX_DELAY_SAMPLES)
            {}
        };

    private:
        PinkNoise::Default noise;
        double noiseAvg = 0;
        IntegrationCoefficients<double> noiseIntegrator;
        FixedSizeArray<T, INPUTS> inputWithVolumeAndNoise;
        FixedSizeArray<T, PROCESSING_CHANNELS> processInput;
        FixedSizeArray<T, OUTPUTS> output;
        FixedSizeArray<T, BANDS> relativeBandWeights;

        Crossovers::Filter<double, T, INPUTS, CROSSOVERS> crossoverFilter;
        ACurves::Filter<T, PROCESSING_CHANNELS> aCurve;
        PerceptiveRmsSet<T, DETECTORS, 1048760> rmsDetector;

        RmsDelay rmsDelay;
        GroupDelay groupDelay;
        EqualizerFilter<double, CHANNELS_PER_GROUP> filters_[GROUPS+1];

        Configurable runtime;
        FixedSizeArray<IntegratorFilter<T>, LIMITERS> signalIntegrator;

        T sampleRate_;
        bool bypass = true;

        static constexpr double PERCEIVED_FAST_BURST_POWER = 0.25;
        static constexpr double PERCEIVED_SLOW_BURST_POWER = 0.15;

        static AdvancedRms::UserConfig rmsUserConfig()
        {
            static constexpr double SLOW = 0.4;
            static constexpr double FAST = 0.0005;

            return {FAST, SLOW,
                    pow(FAST / AdvancedRms::PERCEPTIVE_FAST_WINDOWSIZE,
                        PERCEIVED_FAST_BURST_POWER),
                    pow(SLOW / AdvancedRms::PERCEPTIVE_FAST_WINDOWSIZE,
                        PERCEIVED_SLOW_BURST_POWER)};
        }

        static AdvancedRms::UserConfig rmsUserSubConfig()
        {
            static constexpr double SLOW = 0.4;
            static constexpr double FAST = 0.005;

            return {FAST, SLOW,
                    pow(FAST / AdvancedRms::PERCEPTIVE_FAST_WINDOWSIZE,
                        PERCEIVED_FAST_BURST_POWER),
                    pow(SLOW / AdvancedRms::PERCEPTIVE_FAST_WINDOWSIZE,
                        PERCEIVED_SLOW_BURST_POWER)};
        }

    public:
        DynamicProcessorLevels levels;

        DynamicsProcessor() : sampleRate_(0), levels(GROUPS, CROSSOVERS)
        {
            levels.reset();
        }


        void setSampleRate(
                T sampleRate,
                const FixedSizeArray<T, CROSSOVERS> &crossovers,
                const SpeakermanConfig &config)
        {
            noiseAvg = 0.0;
            noiseIntegrator.setCharacteristicSamples(sampleRate / 20);
            aCurve.setSampleRate(sampleRate);
            crossoverFilter.configure(sampleRate, crossovers);
            for (size_t i = 0; i < GROUPS; i++) {
                signalIntegrator[i].coefficients.setCharacteristicSamples(
                        8 * sampleRate);
            }

            size_t rmsDelaySamples =
                    PerceptiveMetrics::PEAK_HOLD_SECONDS * sampleRate;
            rmsDelay.setDelay(rmsDelaySamples);
            rmsDelay.setChannels(PROCESSING_CHANNELS);
            auto weights = Crossovers::weights(crossovers, sampleRate);
            relativeBandWeights[0] = weights[0];
            for (size_t band = 1; band <= CROSSOVERS; band++) {
                relativeBandWeights[band] = weights[2 * band + 1];
            }

            sampleRate_ = sampleRate;
            runtime.init(createConfigData(config));
            noise.setScale(runtime.userSet().noiseScale());
            BandConfig bandConfig = config.band[0];

            rmsDetector.configure(0,
                                  sampleRate, 4,
                                  bandConfig.perceptive_to_peak_steps,
                                  bandConfig.maximum_window_seconds,
                                  bandConfig.perceptive_to_maximum_window_steps,
                                  100.0);
            AdvancedRms::UserConfig rmsConfig = rmsUserConfig();
            for (size_t band = 0, detector = 1; band < CROSSOVERS; band++) {
                bandConfig = config.band[band + 1];
                for (size_t group = 0; group < GROUPS; group++, detector++) {
                    rmsDetector.configure(detector,
                                          sampleRate, 4,
                                          bandConfig.perceptive_to_peak_steps,
                                          bandConfig.maximum_window_seconds,
                                          bandConfig.perceptive_to_maximum_window_steps,
                                          100.0);
                }
            }
        }

        const ConfigData &getConfigData() const
        {
            return runtime.userSet();
        }

        ConfigData createConfigData(const SpeakermanConfig &config)
        {
            ConfigData data;
            data.configure(config, sampleRate_, relativeBandWeights,
                           rmsUserConfig().peakWeight / 1.5);
//            data.dump();
            return data;
        }

        void updateConfig(const ConfigData &data)
        {
            runtime.modify(data);
            noise.setScale(data.noiseScale());
            groupDelay.setDelay(0, data.subDelay());
            for (size_t group = 0, i = 1; group < GROUPS; group++) {
                filters_[group].configure(
                        data.groupConfig(group).filterConfig());
                size_t delaySamples = data.groupConfig(group).delay();
                for (size_t channel = 0;
                     channel < CHANNELS_PER_GROUP; channel++, i++) {
                    groupDelay.setDelay(i, delaySamples);
                }
            }
            filters_[GROUPS].configure(data.filterConfig());
        }

        void process(
                const FixedSizeArray<T, INPUTS> &input,
                FixedSizeArray<T, OUTPUTS> &target)
        {
//            static int count = 0;
            ZFPUState state;
            runtime.approach();
            applyVolumeAddNoise(input);
            moveToProcessingChannels(
                    crossoverFilter.filter(inputWithVolumeAndNoise));
            processSubRms();
            processChannelsRms();
            levels.next();
            rmsDelay.next();
            mergeFrequencyBands();
            processChannelsFilters(target);
            target[0] = output[0];
            groupDelay.next();
//            T sum = 0;
//            for (size_t i = 0; i < OUTPUTS; i++) {
//                sum += target[i] * target[i];
//            }
//            T avg = signalIntegrator[0].integrate(sum);
//            if (count < 2000000) {
//                count++;
//            }
//            else {
//                count = 0;
//                cout << "LVL=" << sqrt(avg) << endl;
//            }
        }

    private:
        double nextNoise()
        {
            double n = noise();
            noiseIntegrator.integrate(n, noiseAvg);
            return n - noiseAvg;
        }

        void applyVolumeAddNoise(const FixedSizeArray<T, INPUTS> &input)
        {
            T ns = nextNoise();
            for (size_t group = 0; group < GROUPS; group++) {
                const GroupRuntimeData<T,
                                       BANDS> &conf = runtime.data().groupConfig(
                        group);
                auto volume = conf.volume();
                for (size_t channel = 0;
                     channel < CHANNELS_PER_GROUP; channel++) {
                    T x = 0.0;
                    for (size_t inGroup = 0; inGroup < GROUPS; inGroup++) {
                        x += volume[inGroup] *
                             input[inGroup * CHANNELS_PER_GROUP + channel];
                    }
                    inputWithVolumeAndNoise[group * CHANNELS_PER_GROUP +
                                            channel] = x + ns;
                }
            }
        }

        void moveToProcessingChannels(
                const FixedSizeArray<T, CROSSOVER_OUPUTS> &multi)
        {
            // Sum all lowest frequency bands
            processInput[0] = 0.0;
            for (size_t channel = 0; channel < INPUTS; channel++) {
                processInput[0] += multi[channel];
            }

            // copy rest of groupChannels
            for (size_t i = 1, channel = INPUTS;
                 i < processInput.size(); i++, channel++) {
                processInput[i] = multi[channel];
            }
        }

        void processSubRms()
        {
            T x = processInput[0];
            T sub = rmsDelay.setAndGet(0, x);
            x *= runtime.data().subRmsScale();
//            x = aCurve.filter(0, x);
            T detect = rmsDetector.add_square_get_detection(0, x * x, 1.0);
            T gain = 1.0 / detect;
            levels.addValues(0, detect);
            sub *= gain;
            processInput[0] = filters_[GROUPS].filter()->filter(0, sub);
        }

        void processChannelsRms()
        {
            for (size_t band = 0, delay = 1, baseOffset = 1, detector = 1;
                 band < CROSSOVERS; band++) {
                for (size_t group = 0; group < GROUPS; group++, detector++) {
                    T squareSum = 0.0;
                    T scaleForUnity = runtime.data().groupConfig(
                            group).bandRmsScale(1 + band);
                    size_t nextOffset = baseOffset + CHANNELS_PER_GROUP;
                    for (size_t offset = baseOffset;
                         offset < nextOffset; offset++, delay++) {
                        T x = processInput[offset];
                        processInput[offset] = rmsDelay.setAndGet(delay, x);
                        T y = aCurve.filter(offset, x);
                        y *= scaleForUnity;
                        squareSum += y * y;
                    }
                    T detect = rmsDetector.add_square_get_detection(
                            detector, squareSum, 1.0);
                    T gain = 1.0 / detect;
                    levels.addValues(1 + group, detect);
                    for (size_t offset = baseOffset;
                         offset < nextOffset; offset++) {
                        processInput[offset] *= gain;
                    }
                    baseOffset = nextOffset;
                }
            }
        }

        void mergeFrequencyBands()
        {
            T sub = processInput[0];
            output[0] = sub;
            sub *= CHANNEL_RMS_FACTOR;
            for (size_t channel = 1; channel <= INPUTS; channel++) {
                T sum = 0.0;
                size_t max = channel + INPUTS * CROSSOVERS;
                for (size_t offset = channel; offset < max; offset += INPUTS) {
                    sum += processInput[offset];
                }
                output[channel] = sum;
            }
            // incorrect way of doing this. Needs to be fixed by actually having separate
            // processor per group.
            for (size_t group = 0, offset = 1;
                 group < GROUPS; group++, offset += CHANNELS_PER_GROUP) {
                if (getConfigData().groupConfig(group).isMono()) {
                    T sum = 0;
                    for (size_t channel = 0;
                         channel < CHANNELS_PER_GROUP; channel++) {
                        sum += output[offset + channel];
                    }
                    sum *= CHANNEL_ADD_FACTOR;
                    for (size_t channel = 0;
                         channel < CHANNELS_PER_GROUP; channel++) {
                        output[offset + channel] = sum;
                    }
                }
                if (!getConfigData().groupConfig(group).useSub()) {
                    for (size_t channel = 0;
                         channel < CHANNELS_PER_GROUP; channel++) {
                        output[offset + channel] += sub;
                    }
                }
            }
        }

        void processChannelsFilters(FixedSizeArray<T, OUTPUTS> &target)
        {
            for (size_t group = 0, offs = 1; group < GROUPS; group++) {
                auto filter = filters_[group].filter();
                for (size_t channel = 0;
                     channel < CHANNELS_PER_GROUP; channel++, offs++) {
                    target[offs] = filter->filter(channel, output[offs]);
                }
            }
        }

        void processSubLimiter(FixedSizeArray<T, OUTPUTS> &target)
        {
            target[0] = output[0];
        }
    };


} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_SIGNALGROUP_GUARD_H_ */
