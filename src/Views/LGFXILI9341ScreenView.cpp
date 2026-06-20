#if defined(DEVICE_ILI9341_SCREEN)

#include "LGFXILI9341ScreenView.h"
#include "Data/WelcomeScreen.h"

SPIClass& LGFXILI9341ScreenView::getSharedSpiInstance() {
    return sharedSpi;
}

void* LGFXILI9341ScreenView::getScreen() {
    return &tft;
}

void LGFXILI9341ScreenView::initialize() {
    tft.init();
    tft.setRotation(1);  // landscape: 320 wide x 240 tall
    tft.setSwapBytes(true);
    setBrightness(brightnessPct);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
}

void LGFXILI9341ScreenView::logo() {
    clear();

    tft.setSwapBytes(true);
    int logoX = (tft.width()  - WELCOME_IMAGE_WIDTH)  / 2;
    int logoY = (tft.height() - WELCOME_IMAGE_HEIGHT) / 2 - 10;
    if (logoX < 0) logoX = 0;
    if (logoY < 0) logoY = 0;
    tft.pushImage(logoX, logoY, WELCOME_IMAGE_WIDTH, WELCOME_IMAGE_HEIGHT, WelcomeScreen);
    tft.setSwapBytes(false);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    GlobalState& state = GlobalState::getInstance();
    auto version = std::string("ESP32 Bit Pirate - ") + state.getVersion();
    drawCenterText(version, tft.height() - 12, 2);
}

void LGFXILI9341ScreenView::welcome(TerminalTypeEnum& terminalType, std::string& terminalInfos) {
    if      (terminalType == TerminalTypeEnum::WiFiAp)     welcomeHotspot(terminalInfos);
    else if (terminalType == TerminalTypeEnum::WiFiClient) welcomeWeb(terminalInfos);
    else                                                    welcomeSerial(terminalInfos);
}

void LGFXILI9341ScreenView::loading() {
    tft.fillScreen(TFT_BLACK);
    tft.fillRoundRect(20, 20, tft.width() - 40, tft.height() - 40, 5, ILI9341_DARK_GREY);
    tft.drawRoundRect(20, 20, tft.width() - 40, tft.height() - 40, 5, TFT_GREEN);
    tft.setTextColor(TFT_WHITE);
    tft.setTextFont(1);
    tft.setTextSize(2);
    tft.drawString("Loading...", (tft.width() - tft.textWidth("Loading...")) / 2,
                   tft.height() / 2 - 8);
}

void LGFXILI9341ScreenView::clear() {
    tft.fillScreen(TFT_BLACK);
}

void LGFXILI9341ScreenView::setRotation(uint8_t rotation) {
    tft.setRotation(rotation);
}

void LGFXILI9341ScreenView::setBrightness(uint8_t brightness) {
    if (brightness > 100) brightness = 100;
    brightnessPct = brightness;
#if PIN_ILI9341_BL >= 0
    tft.setBrightness((brightness * 255) / 100);
#endif
}

uint8_t LGFXILI9341ScreenView::getBrightness() {
    return brightnessPct;
}

void LGFXILI9341ScreenView::topBar(const std::string& title, bool submenu, bool searchBar) {
    (void)submenu;
    (void)searchBar;

    tft.fillRect(0, 0, tft.width(), 30, TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextFont(2);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(title.c_str(), tft.width() / 2, 20);
    tft.setTextDatum(TL_DATUM);
}

void LGFXILI9341ScreenView::horizontalSelection(
    const std::vector<std::string>& options,
    uint16_t selectedIndex,
    const std::string& description1,
    const std::string& description2)
{
    (void)description2;

    const int originY = 30;
    const std::string& option = options[selectedIndex];

    int boxX = 60;
    int boxW = tft.width() - 120;
    int boxY = originY + 45;
    int boxH = 50;
    int corner = 8;

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(description1.c_str(), tft.width() / 2, originY + 26);

    tft.fillRoundRect(boxX, boxY, boxW, boxH, corner, ILI9341_DARK_GREY);
    tft.drawRoundRect(boxX, boxY, boxW, boxH, corner, TFT_GREEN);

    const int pad = 4;
    tft.fillRoundRect(boxX + pad, boxY + pad, boxW - pad * 2, boxH - pad * 2,
                      std::max(0, corner - pad), ILI9341_DARK_GREY);

    tft.setTextColor(TFT_WHITE, ILI9341_DARK_GREY);
    tft.setTextFont(2);
    tft.setTextSize(2);
    int textW = tft.textWidth(option.c_str());
    int textH = tft.fontHeight();
    tft.setTextDatum(TL_DATUM);
    tft.drawString(option.c_str(), (tft.width() - textW) / 2, boxY + (boxH - textH) / 2);

    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(2);
    tft.setCursor(35, boxY + 19);
    tft.print("<");
    tft.setCursor(tft.width() - 40, boxY + 19);
    tft.print(">");
}

void LGFXILI9341ScreenView::show(PinoutConfig& config) {
    tft.fillScreen(TFT_BLACK);

    const auto& mappings = config.getMappings();
    auto mode = config.getMode();

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString((std::string("MODE ") + mode).c_str(), tft.width() / 2, 20);
    tft.setTextDatum(TL_DATUM);

    if (mappings.empty()) {
        const int frameX = 20, frameY = 45;
        const int frameW = tft.width() - 40, frameH = tft.height() - 70;
        tft.fillRoundRect(frameX, frameY, frameW, frameH, 5, TFT_BLACK);
        tft.drawRoundRect(frameX, frameY, frameW, frameH, 5, TFT_GREEN);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextFont(2);
        tft.setTextDatum(MC_DATUM);
        tft.drawString("Nothing to display", tft.width() / 2, frameY + frameH / 2);
        tft.setTextDatum(TL_DATUM);
        return;
    }

    const int boxH = 24;
    const int startY = 40;
    for (size_t i = 0; i < mappings.size(); ++i) {
        int y = startY + (int)i * (boxH + 4);
        tft.fillRoundRect(20, y, tft.width() - 40, boxH, 6, ILI9341_DARK_GREY);
        tft.drawRoundRect(20, y, tft.width() - 40, boxH, 6, TFT_GREEN);
        tft.setTextFont(2);
        tft.setTextSize(1);
        tft.setTextColor(TFT_WHITE, ILI9341_DARK_GREY);
        int w = tft.textWidth(mappings[i].c_str());
        tft.setCursor((tft.width() - w) / 2, y + 5);
        tft.print(mappings[i].c_str());
    }
}

void LGFXILI9341ScreenView::adapterMode(
    const std::string& adapterName,
    const std::string& description,
    const std::vector<std::string>& details)
{
    tft.fillScreen(TFT_BLACK);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextFont(2);
    tft.setTextSize(2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(adapterName.c_str(), tft.width() / 2, 28);

    tft.setTextSize(1);
    size_t detailCount = std::min<size_t>(details.size(), 8);
    size_t detailRows  = (detailCount + 1) / 2;
    int offsetY = detailRows > 2 ? -4 : 0;
    for (size_t i = 0; i < detailCount; ++i) {
        int col  = i % 2;
        int row  = i / 2;
        int boxX = 18 + col * 146;
        int boxY = 53 + row * 21 + offsetY;
        int boxW = 138, boxH = 19;
        tft.fillRoundRect(boxX, boxY, boxW, boxH, 6, ILI9341_DARK_GREY);
        tft.drawRoundRect(boxX, boxY, boxW, boxH, 6, TFT_GREEN);
        tft.setTextColor(TFT_WHITE, ILI9341_DARK_GREY);
        tft.drawString(details[i].c_str(), boxX + boxW / 2, boxY + boxH / 2);
    }

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    int descY = (detailRows > 2 ? 148 : 112) + offsetY;
    tft.drawString(description.c_str(), tft.width() / 2, descY);
    tft.setTextColor(0xC618, TFT_BLACK);
    tft.drawString("Press any button to return", tft.width() / 2, descY + 16);
    tft.setTextDatum(TL_DATUM);
}

void LGFXILI9341ScreenView::drawLogicTrace(uint8_t pin, const std::vector<uint8_t>& buffer,
                                            uint8_t step) {
    tft.fillRect(0, 35, tft.width(), tft.height() - 35, TFT_BLACK);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setCursor(10, 10);
    tft.print("GPIO ");
    tft.print(pin);

    const int traceY  = 50;
    const int traceH  = 100;
    const int centerY = traceY + traceH / 2;

    int x = 10;
    for (size_t i = 1; i < buffer.size(); ++i) {
        uint8_t prev = buffer[i - 1];
        uint8_t curr = buffer[i];
        int y1 = prev ? (centerY - 20) : (centerY + 20);
        int y2 = curr ? (centerY - 20) : (centerY + 20);
        if (curr != prev) {
            tft.drawLine(x, y1, x + step, y1, prev ? TFT_GREEN : TFT_WHITE);
            tft.drawLine(x + step, y1, x + step, y2, curr ? TFT_GREEN : TFT_WHITE);
        } else {
            tft.drawLine(x, y1, x + step, y2, curr ? TFT_GREEN : TFT_WHITE);
        }
        x += step;
        if (x > tft.width() - step) break;
    }
}

void LGFXILI9341ScreenView::drawAnalogicTrace(uint8_t pin, const std::vector<uint8_t>& buffer,
                                               uint8_t step) {
    tft.fillRect(0, 35, tft.width(), tft.height() - 35, TFT_BLACK);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(1);
    tft.setTextSize(1);
    tft.setCursor(10, 10);
    tft.print("GPIO ");
    tft.print(pin);

    const int topY = 35;
    const int h    = tft.height() - 40;

    int x = 10;
    for (size_t i = 1; i < buffer.size(); ++i) {
        int prev = topY + (h - 1) - (buffer[i - 1] >> 1);
        int curr = topY + (h - 1) - (buffer[i]     >> 1);
        tft.drawLine(x, prev, x + step, curr, TFT_GREEN);
        x += step;
        if (x > tft.width() - step) break;
    }
}

void LGFXILI9341ScreenView::drawWaterfall(
    const std::string& title,
    float startValue,
    float endValue,
    const char* unit,
    int rowIndex,
    int rowCount,
    int level)
{
    const int W = tft.width();
    const int H = tft.height();
    const int midX = W / 2;

    const int headerH = 12;
    const int footerH = 12;
    const int graphY  = headerH;
    const int graphH  = H - headerH - footerH;

    const int barMaxPixels = midX - 2;

    if (level < 0)   level = 0;
    if (level > 100) level = 100;
    int barPixels = (level * barMaxPixels) / 100;

    if (rowIndex == 0) {
        tft.fillScreen(TFT_BLACK);

        tft.setTextSize(1);
        tft.setTextFont(1);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(2, 2);
        tft.print(title.c_str());

        char bufStart[24], bufEnd[24];
        if (unit && unit[0]) {
            snprintf(bufStart, sizeof(bufStart), "%.2f%s", startValue, unit);
            snprintf(bufEnd,   sizeof(bufEnd),   "%.2f%s", endValue,   unit);
        } else {
            snprintf(bufStart, sizeof(bufStart), "%.2f", startValue);
            snprintf(bufEnd,   sizeof(bufEnd),   "%.2f", endValue);
        }

        tft.setCursor(W - tft.textWidth(bufStart) - 2, 2);
        tft.print(bufStart);
        tft.setCursor(W - tft.textWidth(bufEnd) - 2, H - footerH + 2);
        tft.print(bufEnd);

        tft.fillRect(0, graphY, W, graphH, TFT_BLACK);
        tft.drawFastVLine(midX, graphY, graphH, TFT_DARKGREY);
    }

    if (rowCount <= 1) return;
    if (rowIndex < 0)           rowIndex = 0;
    if (rowIndex > rowCount - 1) rowIndex = rowCount - 1;

    int y = graphY + (int)((int64_t)rowIndex * (graphH - 1) / (rowCount - 1));

    tft.drawFastHLine(0, y, W, TFT_BLACK);
    tft.drawPixel(midX, y, TFT_DARKGREY);

    if (barPixels > 0) {
        int x0 = midX - barPixels;
        int w  = barPixels * 2;
        if (x0 < 0)       { w += x0; x0 = 0; }
        if (x0 + w > W)   w = W - x0;
        if (w > 0) tft.drawFastHLine(x0, y, w, TFT_GREEN);
    }
}

void LGFXILI9341ScreenView::drawCenterText(const std::string& text, int y, int fontSize) {
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(fontSize);
    tft.drawString(text.c_str(), tft.width() / 2, y);
    tft.setTextDatum(TL_DATUM);
}

void LGFXILI9341ScreenView::welcomeSerial(const std::string& baudStr) {
    tft.fillScreen(TFT_BLACK);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Open Serial (USB COM)", tft.width() / 2, 55);

    tft.fillRoundRect(70, 80, 180, 40, 8, ILI9341_DARK_GREY);
    tft.drawRoundRect(70, 80, 180, 40, 8, TFT_GREEN);

    std::string baud = "Baudrate: " + baudStr;
    tft.setTextColor(TFT_WHITE, ILI9341_DARK_GREY);
    tft.drawString(baud.c_str(), tft.width() / 2, 100);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Then press any key in terminal", tft.width() / 2, 135);
    tft.setTextDatum(TL_DATUM);
}

void LGFXILI9341ScreenView::welcomeWeb(const std::string& ipStr) {
    tft.fillScreen(TFT_BLACK);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Open browser to connect", tft.width() / 2, 54);

    tft.fillRoundRect(60, 80, 200, 40, 8, ILI9341_DARK_GREY);
    tft.drawRoundRect(60, 80, 200, 40, 8, TFT_GREEN);

    std::string ip = "http://" + ipStr;
    tft.setTextColor(TFT_WHITE, ILI9341_DARK_GREY);
    tft.drawString(ip.c_str(), tft.width() / 2, 100);
    tft.setTextDatum(TL_DATUM);
}

void LGFXILI9341ScreenView::welcomeHotspot(const std::string& ipStr) {
    GlobalState& state = GlobalState::getInstance();
    PinoutConfig config;
    config.setMode("HOTSPOT");
    config.setMappings({
        state.getActiveApName(),
        std::string("PW ") + state.getApPassword(),
        std::string("IP ") + ipStr,
        "CONNECT TO AP"
    });
    show(config);
}

#endif // DEVICE_ILI9341_SCREEN
