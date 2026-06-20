#pragma once

#if defined(DEVICE_ILI9341_SCREEN)

#include "Interfaces/IDeviceView.h"
#include "States/GlobalState.h"

#include <Arduino.h>
#include <M5GFX.h>
#include <lgfx/v1/panel/Panel_LCD.hpp>

// ===== Pin defaults (Cardputer EXT port) — override via build flags =====
#ifndef PIN_ILI9341_SCK
  #define PIN_ILI9341_SCK  40  // EXT PIN 7
#endif
#ifndef PIN_ILI9341_MOSI
  #define PIN_ILI9341_MOSI 14  // EXT PIN 9
#endif
#ifndef PIN_ILI9341_DC
  #define PIN_ILI9341_DC    6  // EXT PIN 5
#endif
#ifndef PIN_ILI9341_CS
  #define PIN_ILI9341_CS    5  // EXT PIN 13
#endif
#ifndef PIN_ILI9341_RST
  #define PIN_ILI9341_RST   3  // EXT PIN 1
#endif
#ifndef PIN_ILI9341_BL
  #define PIN_ILI9341_BL   -1  // -1 = no software backlight control
#endif

#define ILI9341_DARK_GREY 0x4208

// ILI9341 panel with explicit init sequence (lgfx::Panel_ILI9341 is not in M5GFX).
struct Panel_ILI9341_Local : public lgfx::v1::Panel_LCD {
    Panel_ILI9341_Local() {
        _cfg.memory_width  = _cfg.panel_width  = 240;
        _cfg.memory_height = _cfg.panel_height = 320;
    }

protected:
    static constexpr uint8_t CMD_PWCTR1  = 0xC0;
    static constexpr uint8_t CMD_PWCTR2  = 0xC1;
    static constexpr uint8_t CMD_VMCTR1  = 0xC5;
    static constexpr uint8_t CMD_VMCTR2  = 0xC7;
    static constexpr uint8_t CMD_FRMCTR1 = 0xB1;
    static constexpr uint8_t CMD_DFUNCTR = 0xB6;
    static constexpr uint8_t CMD_GMCTRP1 = 0xE0;
    static constexpr uint8_t CMD_GMCTRN1 = 0xE1;
    static constexpr uint8_t CMD_PIXFMT  = 0x3A;

    const uint8_t* getInitCommands(uint8_t listno) const override {
        static constexpr uint8_t list0[] = {
            CMD_PWCTR1,  1, 0x23,
            CMD_PWCTR2,  1, 0x10,
            CMD_VMCTR1,  2, 0x3E, 0x28,
            CMD_VMCTR2,  1, 0x86,
            CMD_PIXFMT,  1, 0x55,        // 16-bit RGB565
            CMD_FRMCTR1, 2, 0x00, 0x18,  // 79 Hz
            CMD_DFUNCTR, 3, 0x08, 0x82, 0x27,
            CMD_GMCTRP1, 15, 0x0F,0x31,0x2B,0x0C,0x0E,0x08,0x4E,0xF1,
                             0x37,0x07,0x10,0x03,0x0E,0x09,0x00,
            CMD_GMCTRN1, 15, 0x00,0x0E,0x14,0x03,0x11,0x07,0x31,0xC1,
                             0x48,0x08,0x0F,0x0C,0x31,0x36,0x0F,
            CMD_SLPOUT, 0 + CMD_INIT_DELAY, 120,
            CMD_IDMOFF, 0,
            CMD_DISPON, 0 + CMD_INIT_DELAY, 100,
            0xFF, 0xFF,
        };
        switch (listno) {
        case 0: return list0;
        default: return nullptr;
        }
    }
};

class LGFX_ILI9341 : public lgfx::v1::LGFX_Device {
    Panel_ILI9341_Local  _panel;
    lgfx::v1::Bus_SPI    _bus;
#if PIN_ILI9341_BL >= 0
    lgfx::v1::Light_PWM  _light;
#endif

public:
    LGFX_ILI9341() {
        {
            auto b = _bus.config();
            b.spi_host    = SPI3_HOST;
            b.spi_mode    = 0;
            b.freq_write  = 40000000;
            b.freq_read   = 16000000;
            b.spi_3wire   = true;
            b.use_lock    = true;
            b.dma_channel = 1;
            b.pin_sclk    = PIN_ILI9341_SCK;
            b.pin_mosi    = PIN_ILI9341_MOSI;
            b.pin_miso    = -1;
            b.pin_dc      = PIN_ILI9341_DC;
            _bus.config(b);
            _panel.setBus(&_bus);
        }
        {
            auto p = _panel.config();
            p.pin_cs          = PIN_ILI9341_CS;
            p.pin_rst         = PIN_ILI9341_RST;
            p.bus_shared      = true;
            p.readable        = false;
            p.invert          = false;
            p.rgb_order       = false;
            p.dlen_16bit      = false;
            p.memory_width    = 240;
            p.memory_height   = 320;
            p.panel_width     = 240;
            p.panel_height    = 320;
            p.offset_x        = 0;
            p.offset_y        = 0;
            p.offset_rotation = 4;
            p.dummy_read_pixel = 8;
            p.dummy_read_bits  = 1;
            _panel.config(p);
        }
#if PIN_ILI9341_BL >= 0
        {
            auto l = _light.config();
            l.pin_bl      = PIN_ILI9341_BL;
            l.invert      = false;
            l.freq        = 44100;
            l.pwm_channel = 7;
            _light.config(l);
            _panel.setLight(&_light);
        }
#endif
        setPanel(&_panel);
    }
};

class LGFXILI9341ScreenView : public IDeviceView {
public:
    void initialize() override;
    SPIClass& getSharedSpiInstance() override;
    void* getScreen() override;
    void logo() override;
    void welcome(TerminalTypeEnum& terminalType, std::string& terminalInfos) override;
    void show(PinoutConfig& config) override;
    void loading() override;
    void adapterMode(const std::string& adapterName, const std::string& description,
                     const std::vector<std::string>& details) override;
    void clear() override;
    void drawLogicTrace(uint8_t pin, const std::vector<uint8_t>& buffer, uint8_t step) override;
    void drawAnalogicTrace(uint8_t pin, const std::vector<uint8_t>& buffer, uint8_t step) override;
    void drawWaterfall(const std::string& title, float startValue, float endValue,
                       const char* unit, int rowIndex, int rowCount, int level) override;
    void setRotation(uint8_t rotation) override;
    void setBrightness(uint8_t brightness) override;
    uint8_t getBrightness() override;
    void topBar(const std::string& title, bool submenu, bool searchBar) override;
    void horizontalSelection(const std::vector<std::string>& options, uint16_t selectedIndex,
                             const std::string& description1,
                             const std::string& description2) override;

private:
    LGFX_ILI9341 tft;
    uint8_t brightnessPct = 100;
    SPIClass sharedSpi{HSPI};

    void drawCenterText(const std::string& text, int y, int fontSize);
    void welcomeSerial(const std::string& baud);
    void welcomeWeb(const std::string& ip);
    void welcomeHotspot(const std::string& ip);
};

#endif // DEVICE_ILI9341_SCREEN
