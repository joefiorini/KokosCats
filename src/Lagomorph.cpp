#include "plugin.hpp"
#include <samplerate.h>

#define HISTORY_SIZE (1 << 21)

struct ClockChannel {
  int channelIndex;
  float secondsSinceLastClock = -1.0f;
  float clockSeconds = -1.0f;
  float gateSeconds = 0.1f;
  float adjustedPulseSeconds = -1.0f;
  float adjustedPulseSecondsDivision = -1.0f;
  float inputVoltage;
  float adjustedPulseTimer = 0.0f;
  int totalPulseCount = 0;
  int pulseCount = 0;
  dsp::SchmittTrigger clockTrigger;

  ClockChannel(int _channelIndex) {
    // printf("initing channel: %d\n", _channelIndex);
    channelIndex = _channelIndex;
  }
};

struct Lagomorph : Module {
  enum ParamId { PARAMS_LEN };
  enum InputId { _INPUT_CLOCK, INPUTS_LEN };
  enum OutputId { _OUTPUT_OUTPUT, OUTPUTS_LEN };
  enum LightId { LIGHTS_LEN };

  int previousStep = 0;
  int currentStep = 0;
  int sampleTime = -1;
  float multiplier = 2.0f;
  float divider = 1.0f;
  int secondsSinceLastClock = 0;
  int previousChannelCount = 0;
  float defaultGateLength = 0.5f;
  bool isSync = false;
  ClockChannel *_clockChannels[PORT_MAX_CHANNELS];

  Lagomorph() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configInput(_INPUT_CLOCK, "");
    configOutput(_OUTPUT_OUTPUT, "");
  }

  void processChannel(const ProcessArgs &args, int channelIndex,
                      float inputVoltage) {
    float timeBetweenEvents = APP->engine->getSampleTime();
    // printf("processing voltage: %f\n", channel.inputVoltage);
    ClockChannel &channel = *_clockChannels[channelIndex];
    auto clock = channel.clockTrigger.process(inputVoltage);
    float out = -1.0f;

    if (clock) {
      channel.totalPulseCount++;
      // printf("PULSED %d\n", channel.totalPulseCount);
      // if (e.secondsSinceLastClock > 0.0f) {
      if (channel.secondsSinceLastClock > 0.0f) {
        // Record the elapsed time from the timer
        // e.clockSeconds = e.secondsSinceLastClock;
        channel.clockSeconds = channel.secondsSinceLastClock;
      }
      // Reset the timer
      // e.secondsSinceLastClock = 0.0f;
      channel.secondsSinceLastClock = 0.0f;
    }

    // if (e.secondsSinceLastClock >= 0.0f) {
    if (channel.secondsSinceLastClock >= 0.0f) {
      // Start/increment the timer
      // timeBetweenEvents = 0.000023
      // e.secondsSinceLastClock += _sampleTime;
      channel.secondsSinceLastClock += timeBetweenEvents;
      // e.dividedSeconds = e.clockSeconds * (float)e.division;
      channel.adjustedPulseSecondsDivision =
          channel.clockSeconds * (float)divider;
      // e.multipliedSeconds = e.dividedSeconds / (float)e.multiplication;
      channel.adjustedPulseSeconds =
          channel.adjustedPulseSecondsDivision / (float)multiplier;
      // e.gateSeconds = std::max(0.001f, e.multipliedSeconds *
      // e.gatePercentage);
      channel.gateSeconds =
          std::max(0.01f, channel.adjustedPulseSeconds * defaultGateLength);
      // printf("pulseSeconds: %f / 2 = adjustedPulseSeconds: %f\n",
      //        channel.pulseSeconds, channel.adjustedPulseSeconds);
      // |----____|----____|----____|----____|
      //  <-500ms.
      //  High: 250
      //  Low: 250
      // |--__|--__|--__|--__|--__|--__|--__|
      // ^250ms^
      // Pulse Time / 2
      // We want to emit 10v for (pulse time / multiplier) seconds
      // then emit 0 for (pulse time / multiplier) seconds
      // So...
      // What is "pulse time"? It's the summation of sample times between pulses
      // (/ 2) When the clock goes high and we have a running timer then we can
      // record that as the pulse time and reset the timer. Then we can record
      // (pulse time / multiplier) as the time for each pulse We start a new
      // timer for the adjusted time between pulses
      //
      if (clock) {

        // The clock pulse is high, but this is the first pulse in this period
        // start the timer
        // if (e.dividerCount < 1) {
        if (channel.pulseCount < 1) {
          // e.dividedProgressSeconds = 0.0f;
          channel.adjustedPulseTimer = 0.0f;
        } else {
          // e.dividedProgressSeconds += _sampleTime;
          channel.adjustedPulseTimer += timeBetweenEvents;
        }
        // ++e.dividerCount;
        ++channel.pulseCount;

        // This is a pulse on the original clock, so reset the counter
        // if (e.dividerCount >= e.division) {
        if (channel.pulseCount >= divider) {
          // e.dividerCount = 0;
          channel.pulseCount = 0;
        }
      } else {
        // Not a clock pulse so just increment the timer
        // e.dividedProgressSeconds += _sampleTime;
        channel.adjustedPulseTimer += timeBetweenEvents;
      }

      // Keep the pulse high until adjusted timer expires
      // if (e.dividedProgressSeconds < e.dividedSeconds) {
      if (channel.adjustedPulseTimer < channel.adjustedPulseSecondsDivision) {
        // float multipliedProgressSeconds = e.dividedProgressSeconds /
        // e.multipliedSeconds;
        float multipliedAdjustedSeconds =
            channel.adjustedPulseTimer / channel.adjustedPulseSeconds;
        // Casts to int then to float . handles < 1 vs. >=1 scenarios?
        // multipliedProgressSeconds -= (float)(int)multipliedProgressSeconds;
        // printf("multipliedAdjustedSeconds before: %f\n",
        //        multipliedAdjustedSeconds);
        multipliedAdjustedSeconds -= (float)(int)multipliedAdjustedSeconds;
        // printf("multipliedAdjustedSeconds after: %f\n",
        //        multipliedAdjustedSeconds);
        // multipliedProgressSeconds *= e.multipliedSeconds;
        multipliedAdjustedSeconds *= channel.adjustedPulseSeconds;
        // out += 2.0f * (float)(multipliedProgressSeconds <= e.gateSeconds);
        // printf("casted boolean: %f\nout: %f\n",
        //        (float)(multipliedAdjustedSeconds <= channel.gateSeconds),
        //        out);
        if (multipliedAdjustedSeconds <= channel.gateSeconds) {
          out = 1;
        } else {
          out = 0;
        }
        // out += 2.0f * (float)(multipliedAdjustedSeconds <=
        // channel.gateSeconds); printf("adjustedPulseTimer =
        // %f\nadjustedPulseSeconds = "
        //        "%f\nmultipliedAdjustedSeconds = %f\nout "
        //        "= %f\n",
        //        channel.adjustedPulseTimer, channel.adjustedPulseSeconds,
        //        multipliedAdjustedSeconds, out);
      }
    }
    out *= 10.0f;
    outputs[_OUTPUT_OUTPUT].setVoltage(out, channel.channelIndex);
  }

  void process(const ProcessArgs &args) override {
    Input clockInput = inputs[_INPUT_CLOCK];
    int channels = clockInput.getChannels();
    if (previousChannelCount < channels) {
      while (previousChannelCount < channels) {
        _clockChannels[previousChannelCount] =
            new ClockChannel(previousChannelCount);
        ++previousChannelCount;
      }
    } else {
      while (channels < previousChannelCount) {
        delete _clockChannels[previousChannelCount];
        _clockChannels[previousChannelCount] = NULL;
        --previousChannelCount;
      }
    }

    // std::vector<ClockChannel> clockChannels;
    // bool previousStep = 0;
    outputs[_OUTPUT_OUTPUT].setChannels(channels);

    if (!clockInput.isConnected()) {
      // printf("not connected, returning...\n");
      return;
    }

    for (int c = 0; c < channels; c++) {
      // ClockChannel channel = _clockChannels
      currentStep++;
      float signal = clockInput.getPolyVoltage(c);
      // printf("seconds since last before process %f\n",
      //        _clockChannels[c]->secondsSinceLastClock);
      processChannel(args, c, signal);
    }
  }
};

struct LagomorphWidget : ModuleWidget {
  LagomorphWidget(Lagomorph *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/8hp.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(
        createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(
        Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(
        box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(5.1, 90.0)), module,
                                             Lagomorph::_INPUT_CLOCK));

    addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(5.1, 101.0)), module,
                                               Lagomorph::_OUTPUT_OUTPUT));
  }
};

Model *modelLagomorph = createModel<Lagomorph, LagomorphWidget>("Lagomorph");
