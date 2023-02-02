#include "plugin.hpp"
#include <unistd.h>

struct Concat : Module {
  enum ParamId { MULTIPLIER_PARAM, DIVIDER_PARAM, PARAMS_LEN };
  enum InputId {
    CONCAT_IN_1,
    CONCAT_IN_2,
    CONCAT_IN_3,
    CONCAT_IN_4,
    INPUTS_LEN
  };
  enum OutputId {
    CONCAT_OUT_1,
    CONCAT_OUT_2,
    CONCAT_OUT_3,
    CONCAT_OUT_4,
    OUTPUTS_LEN
  };
  enum LightIds {
    ENUMS(OUTPUT_1_LIGHTS, 16 * 2),
    ENUMS(OUTPUT_2_LIGHTS, 16 * 2),
    ENUMS(OUTPUT_3_LIGHTS, 16 * 2),
    ENUMS(OUTPUT_4_LIGHTS, 16 * 2),
    LIGHTS_LEN
  };

  Concat() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    configOutput(CONCAT_OUT_1, "Concat 1 Out");
  }

  void process(const ProcessArgs &args) override {
    int numChannels1 = inputs[CONCAT_IN_1].getChannels();
    int numChannels2 = inputs[CONCAT_IN_2].getChannels();
    int numChannels3 = inputs[CONCAT_IN_3].getChannels();

    // if (numChannels1 != 5) {
    //   return;
    // }
    // if (numChannels2 != 6) {
    //   return;
    // }

    // if (numChannels3 != 12) {
    //   return;
    // }

    // int overlap = 4;
    outputs[CONCAT_OUT_1].setChannels(16);
    for (int i = 0; i < numChannels1; i++) {
      outputs[CONCAT_OUT_1].setVoltage(inputs[CONCAT_IN_1].getVoltage(i), i);
    }
    outputs[CONCAT_OUT_1].setVoltage(inputs[CONCAT_IN_2].getVoltage(0), 14);
    outputs[CONCAT_OUT_1].setVoltage(inputs[CONCAT_IN_2].getVoltage(1), 15);

    outputs[CONCAT_OUT_2].setChannels(10);
    for (int i = 0; i < numChannels2 - 2; i++) {
      outputs[CONCAT_OUT_2].setVoltage(inputs[CONCAT_IN_2].getVoltage(i+2), i);
    }

    // for (auto chn = 0; chn < 16; chn++) {
    // numChannels2 = 12
    // numChannels1 = 14
    // overlap = 4
    // openSlots = 2
    // newChannels2 = 10
    // newChannels1 = 16
    // remaining2 = 10
    // copy input2 0, 1 to 14, 15
    // adjust 2-11 to 0-9
    // int overflowChannels2 = numChannels2 % 16;

    // if (overflowChannels2 > 0 && numChannels1 < 16) {
    //   int openSlots1 = 16 - numChannels1;

    // }
    // float v1 = inputs[0].getVoltage(chn);
    // float v2 = inputs[1].getVoltage(chn);
    // float v3 = inputs[2].getVoltage(chn);
    // float v4 = inputs[3].getVoltage(chn);
    // lights[OUTPUT_1_LIGHTS + chn * 2].setBrightness(v1);
    // lights[OUTPUT_2_LIGHTS + chn * 2].setBrightness(v2);
    // lights[OUTPUT_3_LIGHTS + chn * 2].setBrightness(v3);
    // lights[OUTPUT_4_LIGHTS + chn * 2].setBrightness(v4);
    // }
  }
};

#define KC_LABEL_FONT_SIZE 12
#define COLOR_KC_GREY nvgRGB(0x19, 0x19, 0x19)
#define COLOR_KC_DARK_BG nvgHSLA(329, 1.0, 0.09, 1)

struct KCLabel : LedDisplay {
  int fontSize;
  std::shared_ptr<Font> font;
  std::string text;
  NVGcolor color;

  KCLabel(int x, int y, const char *str = "", int fontSize = 10,
          const NVGcolor &colour = COLOR_KC_GREY) {
    font = APP->window->loadFont(
        asset::plugin(pluginInstance, "res/Larabiefont_Regular.otf"));
    box.pos = Vec(x, y);
    box.size = Vec(120, 12);
    text = str;
    color = colour;
    this->fontSize = fontSize;
  }

  void draw(const DrawArgs &args) override {
    if (font->handle >= 0) {
      bndSetFont(font->handle);

      nvgFontSize(args.vg, fontSize);
      nvgFontFaceId(args.vg, font->handle);
      nvgTextLetterSpacing(args.vg, 0);

      nvgBeginPath(args.vg);
      nvgFillColor(args.vg, color);
      nvgText(args.vg, 0, 0, text.c_str(), NULL);
      nvgStroke(args.vg);

      bndSetFont(APP->window->uiFont->handle);
    }
  }
};

struct ConcatWidget : ModuleWidget {
  void addLightRow(int startX, int y, float spacing, Concat::LightIds lights,
                   Concat::OutputId output) {
    int lightSpacing = 1;
    int lightWidth = 2 + lightSpacing;
    // int lightHeight = 2 + lightSpacing;

    addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(
        mm2px(Vec(startX, y)), module, lights + 2 * 0));
    addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(
        mm2px(Vec(startX + lightWidth, y)), module, lights + 2 * 1));
    addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(
        mm2px(Vec(startX + lightWidth * 2, y)), module, lights + 2 * 2));
    addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(
        mm2px(Vec(startX + lightWidth * 3, y)), module, lights + 2 * 3));
    addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(
        mm2px(Vec(startX + lightWidth * 4, y)), module, lights + 2 * 4));
    addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(
        mm2px(Vec(startX + lightWidth * 4, y)), module, lights + 2 * 4));
    addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(
        mm2px(Vec(startX + lightWidth * 5, y)), module, lights + 2 * 5));
    addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(
        mm2px(Vec(startX + lightWidth * 6, y)), module, lights + 2 * 6));
    addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(
        mm2px(Vec(startX + lightWidth * 7, y)), module, lights + 2 * 7));
    addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(
        mm2px(Vec(startX + lightWidth * 8, y)), module, lights + 2 * 8));
    addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(
        mm2px(Vec(startX + lightWidth * 9, y)), module, lights + 2 * 9));
    addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(
        mm2px(Vec(startX + lightWidth * 10, y)), module, lights + 2 * 10));
    addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(
        mm2px(Vec(startX + lightWidth * 11, y)), module, lights + 2 * 11));
    addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(
        mm2px(Vec(startX + lightWidth * 12, y)), module, lights + 2 * 12));
    addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(
        mm2px(Vec(startX + lightWidth * 13, y)), module, lights + 2 * 13));
    addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(
        mm2px(Vec(startX + lightWidth * 14, y)), module, lights + 2 * 14));
    addChild(createLightCentered<SmallSimpleLight<GreenRedLight>>(
        mm2px(Vec(startX + lightWidth * 15, y)), module, lights + 2 * 15));
    addOutput(createOutputCentered<PJ301MPort>(
        mm2px(Vec(startX + lightWidth * 16, y)), module, output));
  }

  ConcatWidget(Concat *module) {
    setModule(module);
    this->module = module;

    // box.size.x = mm2px(5.08 * 16);
    auto left = 4;
    auto top = 8;
    // auto portWidth = mm2px(7.66);
    auto portHeight = mm2px(3.66);
    auto baseSpacing = 8;
    auto rowHeight = portHeight; // + baseSpacing;
    auto portYOffset = rowHeight * 4;
    auto verticalLightRowSpacing = baseSpacing;

    this->setPanel(
        createPanel(asset::plugin(pluginInstance, "res/Concat.svg")));

    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(
        createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(
        Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(
        box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    addChild(new KCLabel(left, box.size.y + 48, "Out 1", KC_LABEL_FONT_SIZE,
                         COLOR_KC_GREY));

    addLightRow(left, top, verticalLightRowSpacing, Concat::OUTPUT_1_LIGHTS,
                Concat::CONCAT_OUT_1);
    addLightRow(left, top + rowHeight, verticalLightRowSpacing,
                Concat::OUTPUT_2_LIGHTS, Concat::CONCAT_OUT_2);
    addLightRow(left, top + rowHeight * 2, verticalLightRowSpacing,
                Concat::OUTPUT_3_LIGHTS, Concat::CONCAT_OUT_3);
    addLightRow(left, top + rowHeight * 3, verticalLightRowSpacing,
                Concat::OUTPUT_4_LIGHTS, Concat::CONCAT_OUT_4);

    addInput(createInput<PJ301MPort>(mm2px(Vec(left, portYOffset + portHeight)),
                                     module, Concat::CONCAT_IN_1));
    addInput(
        createInput<PJ301MPort>(mm2px(Vec(left, portYOffset + portHeight * 2)),
                                module, Concat::CONCAT_IN_2));
    addInput(
        createInput<PJ301MPort>(mm2px(Vec(left, portYOffset + portHeight * 3)),
                                module, Concat::CONCAT_IN_3));
    addInput(
        createInput<PJ301MPort>(mm2px(Vec(left, portYOffset + portHeight * 4)),
                                module, Concat::CONCAT_IN_4));
  }
};

Model *modelConcat = createModel<Concat, ConcatWidget>("Concat");

