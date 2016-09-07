# Speaker management system
* [License](#license)

Speakerman (speaker management) can help venues that need to keep sound levels beneath certain limits. While a lot of commercial and professional solutions exists: they are either too expensive or based on very old techniques &ndash; even though these might have been "redone" in the digital domain to sound modern.

Speakerman hae these goals:
- to limit sound _[perceptually](#perception)_: not with simple peak or RMS measurement done wrong
- to limit too loud input input without _horribly imposing_ that fact on the audio. DJs want to be loud but the neighbours don't want them to be. That does not mean we have to make the sound horrible if the DJ turns the volume up. We just make it less loud :-) 
- to support multiple groups of speakers. A group means that all speakers in that group will have the same gain applied for all channels in that group. Sounds that move from left to right because the left channel is too loud, is annoying.
- to function as crossover. For now: all groups share the same sub-low group (< 80 Hz)

## Perception
Human hearing is complex. It doesn't care about peaks &ndash; unless they are damagingly loud &ndash; but rather about the _energy_ in the signal. Which, naturally, depends on both frequency and how that energy is spread over time. A loud hum is pretty annoying. But a punchy kick drum that has the same energy _when measured over certain intervals_ is nice and makes you want to dance! This is all _very Funny_, but darn annoying a limiter. How is it possible to measure the perceived loudness of a signal and how to keep it under control without harming the experience?

In addition, it is necessary to constrain specific frequency ranges to different levels. This means the input will be split into multiple frequency bands, that will be individually limited in perceived loudness. All except the sub-low frequency band will be added back together in the end. 

### Frequency
In the last century, a lot of research has been done into perceived loudness. Depending on the sound pressure, the perceived loudness is vastly dependent on frequency. This is a complex model, but fortunately we are trying to limit the sound _at its maximum_, so we can make an educated guess on that. A comparison of the [equal loudness countour](https://en.wikipedia.org/wiki/Equal-loudness_contour), the [A-curve](https://en.wikipedia.org/wiki/A-weighting),  [ITU-468 noise weighting](https://en.wikipedia.org/wiki/ITU-R_468_noise_weighting) and [replay gain](https://en.wikipedia.org/wiki/ReplayGain) led to the conclusion that 
* the A-curve is _goud enough_ for the job with respect to perceived loudness per frequency 
* the ITU-468 is a nice starting point for determining the perceived loudness of sounds when calculating the average energy over [different time intervals](#time-interval-energy-measuring).
* the very short window "peak" sensitivity of the hearing is about 50 ms
* sound that has a continued long window energy sounds louder

All measurements (in all frequency bands, except the sub-low) will take the A-curve into account. 

### Time interval energy measuring
Roughly speaking, ITU-468 tells us that the energy of a "burst" that is 16 times shorter, needs to be two times larger to be perceived as equally loud. This is unfortunate. Sound energy is calculated by first applying the [frequency dependency](#frequency) and then using RMS measurement over an interval. If an energy window of a certain size is used for measuring, a burst of <sup>1</sup>/<sub>16</sub>, will only measure 0.25, which means that with this measurement, this burst should be 4 times larger. And that is not how we perceive it. We can fix this by using RRMSS (rot rot mean square square) but that has other undesirable properties. Hence, multiple measurement windows are used that have a weight that makes the situation approach the ITU-468 behavior. 

This, however, is short window perception. Like said before, a constant hum (or pink noise) is _very_ annoying. Also, if only a short window is used for perceived loudness, the limter would sound very radio-like if pressed: there is some punch (because of the ITU-468 measurement) but sound becomes very compressed and _thus_ is perceived louder. So _longer intervals should weight higher. That is easily done, but longer intervals also take longer to respond to sudden rises in loudness. Dumbly implementing these weights causes irritating "pumping" of the sound.

The outcome of all this analysis plus a lot of listening is this: 
- For measurement windows of less than 30 millisecond an approximate ITU-468 time-dependency-weighted measuring is done with a [pure RMS measurement](rms-measurement).
- For measurement windows bwtween 30 and 400 millseconds, a time weighted dependency is done such that 400 ms. weights about 1.4 times more than 400 milliseconds and the The RMS measurement is tweaked to detect rises in the signal faster (see [RMS measurement](rms-measurement)).
- The maximum of all these measurements is taken as the perceived loudness
- The perceived loudness will function as input to a reciprocal amplifier (per group and for the sub-low) when it is above the limiter threshold. These is a small trick applied to the loudness measuring so that no weird clicks or pops appear.

## RMS measurement
Measurement of RMS values if done by a moving window containing squared sample values. Naturally, the signal that provides the samples already went through the frequency sensitivity curve filter. The pure measurement calculates the average value of the window, takes the square root and then uses a double integrator to smooth the result. The fast-attack measurement first applies the integrator and then the square root. A little math shows that fast rises in energy level will be tracked better that way.

## Equalizers
Some speakers ore not good. Each group can use two [parametric equalizer](https://en.wikipedia.org/wiki/Equalization_(audio)#Parametric_equalizer) to fix that.

## Limiters
Perceived loudness has little to do with peaks. But amplifiers and digital to analog converters care about peaks. That is why each group also applies a relatively neat limiter as the last stage.

## Summary
* Per group: 
  * The signal is split into (default) three frequency bands: below 80; 80-160; above 160. 
  * The sub low band of all groups is summed and considered a separate group
  * For each frequency band
    * he loudness is measured using above mechanisms. 
    * A reciprocal amplifier is applied so that the output loudness is never above the threshold
  * All frequency bands are summed together
* To each group (ecxept the sub low group) a parametric equalizer can be applied
* To each group a hard limiter is applied

# License
Copyright 2012-2012-2013 Michel Fleur, https://github.com/emmef/speakerman
```
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0
  
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```
