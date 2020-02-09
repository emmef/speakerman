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
#include <tdap/Crossovers.hpp>
#include <tdap/Delay.hpp>
#include <tdap/MemoryFence.hpp>
#include <tdap/Noise.hpp>
#include <tdap/Followers.hpp>
#include <tdap/Limiter.hpp>
#include <tdap/PerceptiveRms.hpp>
#include <tdap/Transport.hpp>
#include <tdap/Weighting.hpp>
#include <speakerman/SpeakermanRuntimeData.hpp>

namespace speakerman {

    using namespace tdap;

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
        static constexpr size_t DETECTORS = CROSSOVERS * GROUPS;
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
        static constexpr double LIMITER_PREDICTION_SECONDS = 0.003;
        static constexpr double peakThreshold = 1.0;


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

        enum class LimiterClass { SMOOTH_TRIANGULAR, CRUDE };

        class Limiters {
          using LimiterPtr = Limiter<T>*;
          LimiterPtr limiters[LIMITERS];

          void free()  {
            for (size_t i = 0; i < LIMITERS; i++) {
              LimiterPtr p = limiters[i];
              if (p) {
                limiters[i] = nullptr;
                delete p;
              }
            }
          }

        public:

          Limiters() {
            for (size_t i = 0; i < LIMITERS; i++) {
              limiters[i] = nullptr;
            }
          }

          void setPredictionAndThreshold(size_t prediction, T threshold, T sampleRate,
              LimiterClass limiterClass) {
            free();
            for (size_t i = 0; i < LIMITERS; i++) {
              limiters[i] = limiterClass == LimiterClass::SMOOTH_TRIANGULAR
                  ? dynamic_cast<LimiterPtr>(new TriangularLimiter<T>)
                  : dynamic_cast<LimiterPtr>(new ZeroPredictionHardAttackLimiter<T>);
              limiters[i]->setPredictionAndThreshold(prediction, threshold, sampleRate);
            }
            std::cout << "PEAK limiter: type=";
            if (limiterClass == LimiterClass::SMOOTH_TRIANGULAR) {
              std::cout << "Inaudible smooth";
              std::cout << "; prediction=" << prediction;
            }
            else {
              std::cout << "Fast";
            }
            std::cout << "; threshold=" << threshold <<
                         "; sample-rate=" << sampleRate <<
                         "; latency=" << (1.0 * getLatency() / sampleRate) <<
                         std::endl;
          }

          size_t getLatency() const {
            LimiterPtr p = limiters[0];
            if (!p) {
              throw std::runtime_error("Limiters::getLatency(): not initialized!");
            }
            return p->latency();
          }

          T getGain(size_t channel, T sample) noexcept {
            return limiters[channel]->getGain(sample);
          }

          ~Limiters() {
            free();
          }
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
        using Detector = PerceptiveRms<T, (size_t)(0.5 + 192000 * DetectionConfig::MAX_MAXIMUM_WINDOW_SECONDS), 16>;
        using DetectorGroup = PerceptiveRmsGroup<T, (size_t)(0.5 + 192000 * DetectionConfig::MAX_MAXIMUM_WINDOW_SECONDS), 16, CHANNELS_PER_GROUP>;

        Detector subDetector;
        DetectorGroup *groupDetector;
        Limiters limiter;
        IntegrationCoefficients<T> limiterRelease;

        GroupDelay groupDelay;
        GroupDelay predictionDelay;
        EqualizerFilter<double, CHANNELS_PER_GROUP> filters_[GROUPS+1];


        Configurable runtime;

        T sampleRate_;
        bool bypass = true;

        static constexpr double PERCEIVED_FAST_BURST_POWER = 0.25;
        static constexpr double PERCEIVED_SLOW_BURST_POWER = 0.15;

    public:
        DynamicProcessorLevels levels;

        DynamicsProcessor() : sampleRate_(0),
                              noise(1.0, 9600),
                              levels(GROUPS, CROSSOVERS),
                              groupDetector(new DetectorGroup[DETECTORS])
        {
            levels.reset();
        }

        ~DynamicsProcessor()
        {
            delete[] groupDetector;
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
            // Rms detector confiuration
            DetectionConfig detection = config.detection;
            subDetector.configure(
                    sampleRate, 3,
                    detection.perceptive_to_peak_steps,
                    detection.maximum_window_seconds,
                    detection.perceptive_to_maximum_window_steps,
                    100.0);
            for (size_t band = 0, detector = 0; band < CROSSOVERS; band++) {
                for (size_t group = 0; group < GROUPS; group++, detector++) {
                    groupDetector[detector].configure(
                            sampleRate, 3,
                            detection.perceptive_to_peak_steps,
                            detection.maximum_window_seconds,
                            detection.perceptive_to_maximum_window_steps,
                            100.0);

                }
            }
            auto weights = Crossovers::weights(crossovers, sampleRate);
            cout << "Band weights: sub=" << weights[0];
            relativeBandWeights[0] = weights[0];
            for (size_t band = 1; band <= CROSSOVERS; band++) {
              const T &bw = weights[2 * band + 1];
              cout << " band-" << band << "=" << bw;
              relativeBandWeights[band] = bw;
            }
            cout << endl;
            size_t predictionSamples = 0.5 + sampleRate * LIMITER_PREDICTION_SECONDS;
            limiterRelease.setCharacteristicSamples(10 * predictionSamples);
            printf("Prediction samples: %zu for rate %lf\n", predictionSamples, sampleRate);
            limiter.setPredictionAndThreshold(predictionSamples, peakThreshold, sampleRate,
                detection.useBrickWallPrediction == 0 ? LimiterClass::SMOOTH_TRIANGULAR :
                LimiterClass::CRUDE);
            size_t latency = limiter.getLatency();
            for (size_t l = 0; l < DELAY_CHANNELS; l++) {
                predictionDelay.setDelay(l, latency);
            }
            sampleRate_ = sampleRate;
            runtime.init(createConfigData(config));
            noise.setScale(runtime.userSet().noiseScale());
            noise.setIntegrationSamples(sampleRate_ * 0.05);
        }

        const ConfigData &getConfigData() const
        {
            return runtime.userSet();
        }

        ConfigData createConfigData(const SpeakermanConfig &config)
        {
            ConfigData data;
            data.configure(config, sampleRate_, relativeBandWeights,
                           0.25 / 1.5);
            return data;
        }

        void updateConfig(const ConfigData &data)
        {
            static const double sizeFactor = 0.8;
            runtime.modify(data);
            noise.setScale(data.noiseScale());
            size_t predictionSamples = 0.5 + sampleRate_ * LIMITER_PREDICTION_SECONDS;
            size_t subDelay = data.subDelay();
            size_t minGroupDelay = subDelay;
            for (size_t group = 0; group < GROUPS; group++) {
                size_t groupDelay = data.groupConfig(group).delay();
                minGroupDelay = Sizes::min(minGroupDelay, groupDelay);
            }
            if (minGroupDelay > predictionSamples) {
                minGroupDelay = predictionSamples;
            }

            for (size_t group = 0, i = 1; group < GROUPS; group++) {
                filters_[group].configure(data.groupConfig(group).filterConfig());
                size_t groupDelaySamples = data.groupConfig(group).delay() - minGroupDelay;
                for (size_t channel = 0; channel < CHANNELS_PER_GROUP; channel++, i++) {
                    groupDelay.setDelay(i, groupDelaySamples);
                }
            }
            groupDelay.setDelay(0, subDelay - minGroupDelay);
            filters_[GROUPS].configure(data.filterConfig());
        }

        void process(
                const FixedSizeArray<T, INPUTS> &input,
                FixedSizeArray<T, OUTPUTS> &target)
        {
            runtime.approach();
            applyVolumeAddNoise(input);
            moveToProcessingChannels(
                    crossoverFilter.filter(inputWithVolumeAndNoise));
            processSubRms();
            processChannelsRms();
            levels.next();
            mergeFrequencyBands();
            processChannelsFilters(target);
            processSubLimiter(target);
            groupDelay.next();
            predictionDelay.next();
        }

    private:
        void applyVolumeAddNoise(const FixedSizeArray<T, INPUTS> &input)
        {
            T ns = noise();
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
            T sub = x;
            x *= runtime.data().subRmsScale();
            T detect = subDetector.add_square_get_detection(x * x, 1.0);
            T gain = 1.0 / detect;
            levels.addValues(0, detect);
            sub *= gain;
            processInput[0] = filters_[GROUPS].filter()->filter(0, sub);
        }

        void processChannelsRms()
        {
            for (size_t band = 0, delay = 1, baseOffset = 1, detector = 0;
                 band < CROSSOVERS; band++) {
                for (size_t group = 0; group < GROUPS; group++, detector++) {
                    T scaleForUnity = runtime.data().groupConfig(
                            group).bandRmsScale(1 + band);
                    size_t nextOffset = baseOffset + CHANNELS_PER_GROUP;
                    DetectorGroup &gd = groupDetector[detector];
                    gd.reset_frame_detection();
                    for (size_t offset = baseOffset, channel = 0;
                         offset < nextOffset; offset++, delay++, channel++) {
                        T x = processInput[offset];
                        processInput[offset] = x;
                        T y = aCurve.filter(offset, x);
                        y *= scaleForUnity;
                        gd.add_square_for_channel(channel, y*y, 1.0);
                    }
                    T detect = gd.get_detection();
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

#ifdef DYNAMICS_PROCESSOR_LIMITER_ANALYSIS
        struct Analysis {
            const T decay = exp(-1.0 / 1000000);
            const T multiply = 1.0 - decay;
            static constexpr size_t HISTORY = 1000;
            static constexpr size_t MAXCOUNT = 2 * HISTORY;
            int counter = -1;
            T square = 0.0;
            T maxRms = 0.0;
            T maxPeak = 0.0;
            T maxMaxRms = 0.0;
            T maxMaxPeak = 0.0;
            T signalPeak = numeric_limits<T>::lowest();
            T signalDetection = numeric_limits<T>::lowest();
            T limiterGain = numeric_limits<T>::max();
            size_t peakCount = 0;
            size_t peakRmsCount = 0;
            size_t peakGreaterThanDetectionCount = 0;
            T peakHistory[HISTORY];
            T detectHistory[HISTORY];
            T delayedHistory[HISTORY];
            size_t historyPointer = 0;

            void analyseTarget(FixedSizeArray <T, OUTPUTS> &target, size_t offs_start, T prePeak, T detection, size_t delay)
            {
                T maxOut = 0;
                for (size_t channel = 0, offs = offs_start; channel < CHANNELS_PER_GROUP; channel++) {
                    maxOut = Floats::max(maxOut, fabs(target[offs]));
                }
                peakHistory[historyPointer] = prePeak;
                detectHistory[historyPointer] = maxOut;
                T delayedInput = peakHistory[
                        (historyPointer + HISTORY - delay) % HISTORY];
                delayedHistory[historyPointer] = delayedInput;
                bool fault = maxOut > peakThreshold;

                if (fault && counter == -1) {
                    printf("SAMPLE\tPEAK\tDETECTION\tFAULT\n");
                    counter = 0;
                    for (size_t i = historyPointer ; counter < HISTORY; counter++, i = (i + 1) % HISTORY) {
//                        printf("%i\t%lf\t%lf\t%s\n", counter, peakHistory[i], detectHistory[i], peakHistory[i] > detectHistory[i] ? "FAULT" : "");
                        printf("%i\t%lf\t%lf\t%8s %zu\n", counter, peakHistory[i], detectHistory[i], detectHistory[historyPointer] > peakThreshold ? "FAULT" : "", delay);
                    }
                }
                if (counter >= 0 && counter < MAXCOUNT) {
                    printf("%i\t%lf\t%lf\t%8s %zu\n", counter, prePeak, maxOut, fault ? "FAULT" : "", delay);
                    counter ++;
                }
                historyPointer = (historyPointer + 1) % HISTORY;
//                if (counter == 100000) {
//                    counter = -1;
//                }
//
//
//                for (size_t channel = 0, offs = offs_start; channel < CHANNELS_PER_GROUP; channel++, offs++) {
//                    counter++;
//                    signalPeak = Floats::max(signalPeak, prePeak);
//                    signalDetection = Floats::max(signalDetection, detection);
//                    limiterGain = Floats::min(limiterGain, gain);
//                    const T x = target[offs];
//                    T peak = fabs(x);
//                    if (peak > 1.0) {
//                        peakCount++;
//                        maxPeak = Floats::max(maxPeak, peak);
//                    }
//                    square *= decay;
//                    square += multiply * x * x;
//                    T rms = sqrt(square);
//                    if (rms > 0.25) {
//                        peakRmsCount++;
//                        maxRms = Floats::max(maxRms, sqrt(square));
//                    }
//
//
//                    if (counter > 100000) {
//                        if (peakRmsCount > 0 || peakCount > 0) {
//                            maxMaxRms = Floats::max(maxMaxRms, maxRms);
//                            maxMaxPeak = Floats::max(maxMaxPeak, maxPeak);
//                            printf("%8zu PEAKS : %8lg/%8lg; %8zu HIGH-RMS : %8lg/%8lg\n", peakCount, maxPeak, maxMaxPeak, peakRmsCount, maxRms, maxMaxRms);
//                            peakCount = 0;
//                            peakRmsCount = 0;
//                            maxRms = 0;
//                            maxPeak = 0;
//                        }
//                        printf("Limiter %lf peak   %lf detect   %lf gain  %zu OVER (%d)\n", signalPeak, signalDetection, limiterGain, peakGreaterThanDetectionCount, signalPeak > signalDetection);
//                        signalPeak = numeric_limits<T>::lowest();
//                        signalDetection = numeric_limits<T>::lowest();
//                        limiterGain = numeric_limits<T>::max();
//                        peakGreaterThanDetectionCount = 0;
//                        counter = 0;
//                    }
//                }
            }
        } analysis;

#define DO_DYNAMICS_PROCESSOR_LIMITER_ANALYSIS(TARGET,OFFS,MAX,DETECT,OUT) \
    analysis.analyseTarget(TARGET,OFFS,MAX,DETECT,OUT)
#else
#define DO_DYNAMICS_PROCESSOR_LIMITER_ANALYSIS(TARGET,OFFS,MAX,DETECT,GAIN)
#endif
        void processChannelsFilters(FixedSizeArray<T, OUTPUTS> &target)
        {

            for (size_t group = 0, offs_start = 1; group < GROUPS; group++, offs_start += CHANNELS_PER_GROUP) {
                auto filter = filters_[group].filter();

                T maxFiltered = 0;
                for (size_t channel = 0, offs = offs_start; channel < CHANNELS_PER_GROUP; channel++, offs++) {
                    double out = filter->filter(channel, groupDelay.setAndGet(offs, output[offs]));
                    maxFiltered = Floats::max(maxFiltered, fabs(out));
                    target[offs] = predictionDelay.setAndGet(offs, out);
                }
                T limiterGain = limiter.getGain(group, maxFiltered);
                for (size_t channel = 0, offs = offs_start; channel < CHANNELS_PER_GROUP; channel++, offs++) {
                    T outputValue = target[offs] * limiterGain;
                    target[offs] = outputValue;
                    //Floats::force_between(outputValue,-peakThreshold, peakThreshold);
                }
                DO_DYNAMICS_PROCESSOR_LIMITER_ANALYSIS(target, offs_start, maxFiltered, limiterGain, predictionDelay.getDelay(0));
            }
        }


        void processSubLimiter(FixedSizeArray<T, OUTPUTS> &target)
        {
            T value = output[0];
            T maxOut = fabs(value);
            T limiterGain = limiter.getGain(0, maxOut);
            target[0] = groupDelay.setAndGet(0, limiterGain * predictionDelay.setAndGet(0, value));
        }
    };


} /* End of namespace speakerman */

#endif /* SMS_SPEAKERMAN_SIGNALGROUP_GUARD_H_ */
