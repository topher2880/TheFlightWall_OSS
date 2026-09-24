#pragma once

#include <stdint.h>
#include <vector>
#include <FastLED.h>
#include "interfaces/BaseDisplay.h"

class FastLED_NeoMatrix;

class NeoMatrixDisplay : public BaseDisplay
{
public:
    NeoMatrixDisplay();
    ~NeoMatrixDisplay() override;

    bool initialize() override;
    void clear() override;
    void displayFlights(const std::vector<FlightInfo> &flights) override;
    void displayMessage(const String &message);
    void showLoading();

private:
    FastLED_NeoMatrix *_matrix = nullptr;
    CRGB *_leds = nullptr;

    uint16_t _matrixWidth = 0;
    uint16_t _matrixHeight = 0;
    uint32_t _numPixels = 0;
    uint16_t _logoBuffer[32 * 32];

    size_t _currentFlightIndex = 0;
    unsigned long _lastCycleMs = 0;

    void drawTextLine(int16_t x, int16_t y, const String &text, uint16_t color);
    bool drawAirlineLogo(const FlightInfo &f);
    String makeFlightLine(const FlightInfo &f);
    String truncateToColumns(const String &text, int maxColumns);
    void displaySingleFlightCard(const FlightInfo &f);
    void displayLoadingScreen();
};
