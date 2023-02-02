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
  int outputChannels = 8;
  dsp::SchmittTrigger clockTrigger;
  dsp::SchmittTrigger runTrigger;

  int dividend = 1;
  int multiplicand = 1;

  ClockChannel(int _channelIndex) {
    // printf("initing channel: %d\n", _channelIndex);
    channelIndex = _channelIndex;
  }

  void reset() {
    clockTrigger.reset();
    runTrigger.reset();
  }
};

struct Lagomorph : Module {
  enum ParamId {
    COL1_MULTIPLIER_PARAM,
    COL1_DIVIDER_PARAM,
    COL2_MULTIPLIER_PARAM,
    COL2_DIVIDER_PARAM,
    COL3_MULTIPLIER_PARAM,
    COL3_DIVIDER_PARAM,
    COL4_MULTIPLIER_PARAM,
    COL4_DIVIDER_PARAM,
    COL5_MULTIPLIER_PARAM,
    COL5_DIVIDER_PARAM,
    COL6_MULTIPLIER_PARAM,
    COL6_DIVIDER_PARAM,
    COL7_MULTIPLIER_PARAM,
    COL7_DIVIDER_PARAM,
    COL8_MULTIPLIER_PARAM,
    COL8_DIVIDER_PARAM,
    PARAMS_LEN
  };
  enum InputId { CLOCK_IN_INPUT, RESET_INPUT, INPUTS_LEN };
  enum OutputId {
    ENUMS(COL1_OUTS, 8),
    ENUMS(COL2_OUTS, 8),
    ENUMS(COL3_OUTS, 8),
    ENUMS(COL4_OUTS, 8),
    ENUMS(COL5_OUTS, 8),
    ENUMS(COL6_OUTS, 8),
    ENUMS(COL7_OUTS, 8),
    ENUMS(COL8_OUTS, 8),
    OUTPUTS_LEN
  };
  enum LightId { LIGHTS_LEN };

  int previousStep = 0;
  int currentStep = 0;
  int sampleTime = -1;
  float multiplicand = 2.0f;
  float dividend = 1.0f;
  int secondsSinceLastClock = 0;
  int previousChannelCount = 0;
  float defaultGateLength = 0.5f;
  bool isSync = false;
  bool areOutputsPolyphonic = false;
  ClockChannel *_clockChannels[PORT_MAX_CHANNELS];

  Lagomorph() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configParam(COL1_MULTIPLIER_PARAM, 0.f, 1.f, 0.f, "Multiplicand", "", 0.0f,
                20.0f, 0.0f);
    configParam(COL1_DIVIDER_PARAM, 0.f, 1.f, 0.f, "Divided", "", 0.0f, 20.0f,
                0.0f);
    configInput(CLOCK_IN_INPUT, "");
    configOutputColumn(COL1_OUTS);
    configOutputColumn(COL2_OUTS);
    configOutputColumn(COL3_OUTS);
    configOutputColumn(COL4_OUTS);
    configOutputColumn(COL5_OUTS);
    configOutputColumn(COL6_OUTS);
    configOutputColumn(COL7_OUTS);
    configOutputColumn(COL8_OUTS);
  }

  void configOutputColumn(OutputId COL_OUT) {
    configOutput(COL_OUT + 0, "");
    configOutput(COL_OUT + 1, "");
    configOutput(COL_OUT + 2, "");
    configOutput(COL_OUT + 3, "");
    configOutput(COL_OUT + 4, "");
    configOutput(COL_OUT + 5, "");
    configOutput(COL_OUT + 6, "");
    configOutput(COL_OUT + 7, "");
  }

  void processChannel(const ProcessArgs &args, int channelIndex,
                      float inputVoltage) {
    float timeBetweenEvents = APP->engine->getSampleTime();
    ClockChannel &channel = *_clockChannels[channelIndex];
    auto clock = channel.clockTrigger.process(inputVoltage);
    float out = -1.0f;

    if (channel.runTrigger.process(
            inputs[RESET_INPUT].getPolyVoltage(channel.channelIndex))) {
      channel.reset();
    }

    if (clock) {
      channel.totalPulseCount++;
      if (channel.secondsSinceLastClock > 0.0f) {
        // Record the elapsed time from the timer
        channel.clockSeconds = channel.secondsSinceLastClock;
      }
      // Reset the timer
      channel.secondsSinceLastClock = 0.0f;
    }

    if (channel.secondsSinceLastClock >= 0.0f) {
      // Start/increment the timer
      channel.secondsSinceLastClock += timeBetweenEvents;
      channel.adjustedPulseSecondsDivision =
          channel.clockSeconds * (float)channel.dividend;
      channel.adjustedPulseSeconds =
          channel.adjustedPulseSecondsDivision / (float)channel.multiplicand;
      channel.gateSeconds =
          std::max(0.001f, channel.adjustedPulseSeconds * defaultGateLength);
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
        if (channel.pulseCount < 1) {
          channel.adjustedPulseTimer = 0.0f;
        } else {
          channel.adjustedPulseTimer += timeBetweenEvents;
        }
        ++channel.pulseCount;

        // This is a pulse on the original clock, so reset the counter
        if (channel.pulseCount >= dividend) {
          channel.pulseCount = 0;
        }
      } else {
        // Not a clock pulse so just increment the timer
        channel.adjustedPulseTimer += timeBetweenEvents;
      }

      // Keep the pulse high until adjusted timer expires
      if (channel.adjustedPulseTimer < channel.adjustedPulseSecondsDivision) {
        float multipliedAdjustedSeconds =
            channel.adjustedPulseTimer / channel.adjustedPulseSeconds;
        // Casts to int then to float . handles < 1 vs. >=1 scenarios?
        multipliedAdjustedSeconds -= (float)(int)multipliedAdjustedSeconds;
        multipliedAdjustedSeconds *= channel.adjustedPulseSeconds;

        if (multipliedAdjustedSeconds <= channel.gateSeconds) {
          out = 1;
        } else {
          out = 0;
        }
      }
    }

    out *= 10.0f;

    for (int o = 0; o < OUTPUTS_LEN; o++) {
      if (areOutputsPolyphonic) {
        outputs[o].setChannels(channel.outputChannels);
        for (int c = 0; c < channel.outputChannels; c++) {
          outputs[o].setVoltage(out, c);
        }
      } else {
        outputs[o].setVoltage(out, channel.channelIndex);
      }
    }

    dividend = clamp(params[COL1_DIVIDER_PARAM].getValue(), 0.0f, 1.0f);
    dividend *= dividend;
    dividend *= 63.0f;
    dividend += 1.0f;
    channel.dividend = clamp((int)roundf(dividend), 1, 64);

    multiplicand = clamp(params[COL1_MULTIPLIER_PARAM].getValue(), 0.0f, 1.0f);
    multiplicand *= multiplicand;
    multiplicand *= 63.0f;
    multiplicand += 1.0f;
    channel.multiplicand = clamp((int)roundf(multiplicand), 1, 64);
  }

  void process(const ProcessArgs &args) override {
    Input clockInput = inputs[CLOCK_IN_INPUT];
    if (!clockInput.isConnected()) {
      return;
    }
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
    for (int o = 0; o < OUTPUTS_LEN; o++) {
      outputs[o].setChannels(channels);
    }

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

  json_t *dataToJson() override {
    json_t *rootJ = json_object();

    json_object_set_new(rootJ, "areOutputsPolyphonic",
                        json_boolean(areOutputsPolyphonic));

    return rootJ;
  }

  void dataFromJson(json_t *rootJ) override {
    json_t *areOutputsPolyphonicJ =
        json_object_get(rootJ, "areOutputsPolyphonic");
    if (areOutputsPolyphonicJ) {
      areOutputsPolyphonic = json_boolean_value(areOutputsPolyphonicJ);
    }
  }
};

struct KokoKnob : SvgKnob {
  KokoKnob() {
    minAngle = -0.8 * M_PI;
    maxAngle = 0.8 * M_PI;
    setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/knob.svg")));
  };
};

struct KokoPortOut : SvgPort {
  KokoPortOut() {
    setSvg(APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/port-output.svg")));
  }
};

struct KokoPortIn : SvgPort {
  KokoPortIn() {
    setSvg(APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/port-input.svg")));
  }
};

struct LagomorphWidget : ModuleWidget {
  void addOutputPortColumn(Lagomorph::OutputId column, int index, int x,
                           int y) {
    addOutput(
        createOutputCentered<KokoPortOut>(Vec(x, y), module, column + index));
  };

  LagomorphWidget(Lagomorph *module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/panel.svg")));

    auto knobWidth = mm2px(6.714);
    auto knobHeight = mm2px(6.717);
    auto componentSpacing = 12;
    auto portWidth = mm2px(7.66);
    auto portHeight = mm2px(7.66);
    auto inputXStart = componentSpacing;
    auto colXStart = componentSpacing * 3;
    auto portXStart = colXStart + (knobWidth - portWidth);

    printf("knobHeight: %f\n", knobHeight);

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(
        createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(
        Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(
        box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    addInput(createInput<KokoPortIn>(
        Vec(inputXStart,
            RACK_GRID_HEIGHT - (portHeight * 3 + componentSpacing * 3)),
        module, Lagomorph::CLOCK_IN_INPUT));
    addInput(createInput<KokoPortIn>(
        Vec(inputXStart,
            RACK_GRID_HEIGHT - (portHeight * 2 + componentSpacing * 2)),
        module, Lagomorph::RESET_INPUT));

    addParam(createParamCentered<KokoKnob>(
        Vec(knobWidth + colXStart + componentSpacing, knobHeight), module,
        Lagomorph::COL1_MULTIPLIER_PARAM));
    addParam(createParamCentered<KokoKnob>(
        Vec(knobWidth + colXStart + componentSpacing,
            (knobHeight * 2) + componentSpacing),
        module, Lagomorph::COL1_DIVIDER_PARAM));

    addParam(createParamCentered<KokoKnob>(
        Vec(knobWidth + colXStart * 2 + componentSpacing,
            knobHeight + componentSpacing),
        module, Lagomorph::COL2_MULTIPLIER_PARAM));
    addParam(createParamCentered<KokoKnob>(
        Vec(knobWidth + colXStart * 2 + componentSpacing,
            (knobHeight * 2) + componentSpacing),
        module, Lagomorph::COL2_DIVIDER_PARAM));

    addParam(createParamCentered<KokoKnob>(
        Vec(knobWidth + colXStart * 3 + componentSpacing,
            knobHeight + componentSpacing),
        module, Lagomorph::COL3_MULTIPLIER_PARAM));
    addParam(createParamCentered<KokoKnob>(
        Vec(knobWidth + colXStart * 3 + componentSpacing,
            (knobHeight * 2) + componentSpacing),
        module, Lagomorph::COL3_DIVIDER_PARAM));

    addParam(createParamCentered<KokoKnob>(
        Vec(knobWidth + colXStart * 4 + componentSpacing,
            knobHeight + componentSpacing),
        module, Lagomorph::COL4_MULTIPLIER_PARAM));
    addParam(createParamCentered<KokoKnob>(
        Vec(knobWidth + colXStart * 4 + componentSpacing,
            (knobHeight * 2) + componentSpacing),
        module, Lagomorph::COL4_DIVIDER_PARAM));

    addParam(createParamCentered<KokoKnob>(
        Vec(knobWidth + colXStart * 5 + componentSpacing,
            knobHeight + componentSpacing),
        module, Lagomorph::COL5_MULTIPLIER_PARAM));
    addParam(createParamCentered<KokoKnob>(
        Vec(knobWidth + colXStart * 5 + componentSpacing,
            (knobHeight * 2) + componentSpacing),
        module, Lagomorph::COL5_DIVIDER_PARAM));

    addParam(createParamCentered<KokoKnob>(
        Vec(knobWidth + colXStart * 6 + componentSpacing,
            knobHeight + componentSpacing),
        module, Lagomorph::COL6_MULTIPLIER_PARAM));
    addParam(createParamCentered<KokoKnob>(
        Vec(knobWidth + colXStart * 6 + componentSpacing,
            (knobHeight * 2) + componentSpacing),
        module, Lagomorph::COL6_DIVIDER_PARAM));

    addParam(createParamCentered<KokoKnob>(
        Vec(knobWidth + colXStart * 7 + componentSpacing,
            knobHeight + componentSpacing),
        module, Lagomorph::COL7_MULTIPLIER_PARAM));
    addParam(createParamCentered<KokoKnob>(
        Vec(knobWidth + colXStart * 7 + componentSpacing,
            knobHeight * 2 + componentSpacing),
        module, Lagomorph::COL7_DIVIDER_PARAM));

    addParam(createParamCentered<KokoKnob>(
        Vec(knobWidth + colXStart * 8 + componentSpacing,
            knobHeight + componentSpacing),
        module, Lagomorph::COL8_MULTIPLIER_PARAM));
    addParam(createParamCentered<KokoKnob>(
        Vec(knobWidth + colXStart * 8 + componentSpacing,
            knobHeight * 2 + componentSpacing),
        module, Lagomorph::COL8_DIVIDER_PARAM));

    for (int i = 0; i < 8; i++) {
      addOutputPortColumn(Lagomorph::COL1_OUTS, i,
                          portWidth + portXStart + componentSpacing,
                          knobHeight * 4 + (portHeight + componentSpacing) * i);
      addOutputPortColumn(Lagomorph::COL2_OUTS, i,
                          portWidth + portXStart * 2 + componentSpacing,
                          (knobHeight * 4) +
                              (portHeight + componentSpacing) * i);
      addOutputPortColumn(Lagomorph::COL3_OUTS, i,
                          portWidth + portXStart * 3 + componentSpacing,
                          (knobHeight * 4) +
                              (portHeight + componentSpacing) * i);
      addOutputPortColumn(Lagomorph::COL4_OUTS, i,
                          portWidth + portXStart * 4 + componentSpacing,
                          (knobHeight * 4) +
                              (portHeight + componentSpacing) * i);
      addOutputPortColumn(Lagomorph::COL5_OUTS, i,
                          portWidth + portXStart * 5 + componentSpacing,
                          (knobHeight * 4) +
                              (portHeight + componentSpacing) * i);
      addOutputPortColumn(Lagomorph::COL6_OUTS, i,
                          portWidth + portXStart * 6 + componentSpacing,
                          (knobHeight * 4) +
                              (portHeight + componentSpacing) * i);
      addOutputPortColumn(Lagomorph::COL7_OUTS, i,
                          portWidth + portXStart * 7 + componentSpacing,
                          (knobHeight * 4) +
                              (portHeight + componentSpacing) * i);
      addOutputPortColumn(Lagomorph::COL8_OUTS, i,
                          portWidth + portXStart * 8 + componentSpacing,
                          (knobHeight * 4) +
                              (portHeight + componentSpacing) * i);
    }
  }

  struct LagomorphMenuItem : MenuItem {
    Lagomorph *module;

    void onAction(const event::Action &e) override {
      module->areOutputsPolyphonic = !module->areOutputsPolyphonic;
      printf("option: %d", (module->areOutputsPolyphonic));
    }

    void step() override {
      rightText = (module->areOutputsPolyphonic) ? "✔" : "";
    }
  };

  void appendContextMenu(Menu *menu) override {
    menu->addChild(new MenuSeparator());

    Lagomorph *lago = dynamic_cast<Lagomorph *>(module);
    assert(lago);

    LagomorphMenuItem *label = new LagomorphMenuItem();
    label->module = lago;
    label->text = "Make outputs polyphonic";
    menu->addChild(label);
  }
};

struct LagQuantity : Quantity {
  Lagomorph *module;

  LagQuantity(Lagomorph *m) : module(m) {}
};

Model *modelLagomorph = createModel<Lagomorph, LagomorphWidget>("Lagomorph");
