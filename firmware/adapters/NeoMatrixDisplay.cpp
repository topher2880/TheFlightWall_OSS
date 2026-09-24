/*
Purpose: Render flight info on a WS2812B NeoPixel matrix via FastLED_NeoMatrix.
Responsibilities:
- Initialize LED matrix based on HardwareConfiguration and user display settings.
- Render a bordered, three-line flight card and a minimal loading screen.
- Preserve and prominently show flight identifiers.
- Provide useful fallback labels for sparse ADS-B/AeroAPI records, including RAAF.
- Cycle through multiple flights at a configurable interval.
*/
#include "adapters/NeoMatrixDisplay.h"

#include <Adafruit_GFX.h>
#include <FastLED_NeoMatrix.h>
#include <FastLED.h>
#include "config/UserConfiguration.h"
#include "config/HardwareConfiguration.h"
#include "config/TimingConfiguration.h"
#include "core/LogoManager.h"

NeoMatrixDisplay::NeoMatrixDisplay() {}

NeoMatrixDisplay::~NeoMatrixDisplay()
{
    if (_leds)
    {
        delete[] _leds;
        _leds = nullptr;
    }
    if (_matrix)
    {
        delete _matrix;
        _matrix = nullptr;
    }
}

bool NeoMatrixDisplay::initialize()
{
    _matrixWidth = HardwareConfiguration::DISPLAY_MATRIX_WIDTH;
    _matrixHeight = HardwareConfiguration::DISPLAY_MATRIX_HEIGHT;
    _numPixels = (uint32_t)_matrixWidth * (uint32_t)_matrixHeight;

    _leds = new CRGB[_numPixels];

    _matrix = new FastLED_NeoMatrix(
        _leds,
        HardwareConfiguration::DISPLAY_TILE_PIXEL_W,
        HardwareConfiguration::DISPLAY_TILE_PIXEL_H,
        HardwareConfiguration::DISPLAY_TILES_X,
        HardwareConfiguration::DISPLAY_TILES_Y,
        NEO_MATRIX_BOTTOM + NEO_MATRIX_RIGHT +
            NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG +
            NEO_TILE_TOP + NEO_TILE_RIGHT + NEO_TILE_COLUMNS + NEO_TILE_ZIGZAG);

    FastLED.addLeds<WS2812B, HardwareConfiguration::DISPLAY_PIN, GRB>(_leds, _numPixels);
    _matrix->setTextWrap(false);
    _matrix->setTextSize(1);
    _matrix->setBrightness(UserConfiguration::DISPLAY_BRIGHTNESS);
    clear();
    _currentFlightIndex = 0;
    _lastCycleMs = millis();
    return true;
}

void NeoMatrixDisplay::clear()
{
    if (_matrix)
    {
        _matrix->fillScreen(0);
        FastLED.show();
    }
}

static String bestIdent(const FlightInfo &f)
{
    if (f.ident_iata.length())
        return f.ident_iata;
    if (f.ident.length())
        return f.ident;
    if (f.ident_icao.length())
        return f.ident_icao;
    return f.adsb_callsign;
}

struct MilitaryCallsignMatch
{
    bool matched;
    String service;
    String aircraftHint;

    MilitaryCallsignMatch(bool isMatched = false,
                          const String &serviceName = String(""),
                          const String &aircraft = String(""))
        : matched(isMatched), service(serviceName), aircraftHint(aircraft) {}
};

static MilitaryCallsignMatch matchAustralianMilitaryCallsign(const FlightInfo &f)
{
    String ident = bestIdent(f);
    ident.trim();
    ident.toUpperCase();

    String opIcao = f.operator_icao;
    opIcao.trim();
    opIcao.toUpperCase();

    // ASY is the RAAF operator/telephony code ("Aussie").
    if (opIcao == "ASY" || ident.startsWith("ASY"))
        return MilitaryCallsignMatch(true, "RAAF", "");

    // Common Australian military tactical/mission callsigns. These are
    // intentionally kept as a small, high-confidence table and can grow as
    // more locally observed callsigns are confirmed.
    struct Rule
    {
        const char *prefix;
        const char *service;
        const char *aircraftHint;
    };

    static const Rule rules[] = {
        {"BLACKCAT", "RAAF", "P-8A POSEIDON"}, // BLACKCAT full tactical callsign
        {"BLKT", "RAAF", "P-8A POSEIDON"},     // BLACKCAT abbreviated ADS-B ident
        {"DRGN", "RAAF", "KC-30A MRTT"},     // DRAGON
        {"WNSR", "RAAF", "KC-30A MRTT"},     // WINDSOR
        {"DNGO", "RAAF", "KING AIR 350"},    // DINGO
        {"EVY",  "RAAF", "VIP"},             // ENVOY
        {"DGTL", "RAAF", "E-7A WEDGETAIL"},  // DOGTAIL
        {"WGTL", "RAAF", "E-7A WEDGETAIL"},  // WEDGETAIL
        {"OBAK", "RAAF", "E-7A WEDGETAIL"},  // OUTBACK
        {"STAL", "RAAF", "C-17A"},           // STALLION
        {"WLBY", "RAAF", "C-27J"},           // WALLABY
        {"ARCH", "RAAF", "C-130J"},          // ARCHER
        {"ACHR", "RAAF", "C-130J"},          // ARCHER alt
        {"ARRW", "RAAF", "C-130J"},          // ARROW
        {"ATLS", "RAAF", ""},                // ATLAS
        {"PACR", "RAAF", "C-17A"},           // PACER
    };

    for (const Rule &rule : rules)
    {
        if (ident.startsWith(rule.prefix))
            return MilitaryCallsignMatch(true, String(rule.service), String(rule.aircraftHint));
    }

    return MilitaryCallsignMatch();
}

static bool isRAAFFlight(const FlightInfo &f)
{
    MilitaryCallsignMatch match = matchAustralianMilitaryCallsign(f);
    return match.matched && match.service == "RAAF";
}

static String bestAirportLabel(const AirportInfo &airport)
{
    String label;
    if (airport.city.length())
        label = airport.city;
    else if (airport.name.length())
        label = airport.name;
    else if (airport.code_iata.length())
        label = airport.code_iata;
    else
        label = airport.code_icao;

    label.trim();
    label.toUpperCase();
    return label;
}

static String bestAirline(const FlightInfo &f)
{
    MilitaryCallsignMatch military = matchAustralianMilitaryCallsign(f);
    if (military.matched)
        return military.service;

    if (f.airline_display_name_full.length())
        return f.airline_display_name_full;
    if (f.operator_iata.length())
        return f.operator_iata;
    if (f.operator_icao.length())
        return f.operator_icao;
    if (f.operator_code.length())
        return f.operator_code;

    return f.enriched ? String("FLIGHT") : String("ADS-B");
}

String NeoMatrixDisplay::makeFlightLine(const FlightInfo &f)
{
    String airline = bestAirline(f);
    String ident = bestIdent(f);
    String origin = bestAirportLabel(f.origin);
    String dest = bestAirportLabel(f.destination);
    String route = origin + "-" + dest;
    String type = f.aircraft_display_name_short.length() ? f.aircraft_display_name_short : f.aircraft_code;

    String line = ident;
    if (airline.length())
    {
        if (line.length())
            line += " ";
        line += airline;
    }
    if (type.length())
    {
        line += " ";
        line += type;
    }
    if ((origin.length() || dest.length()) && route.length() > 1)
    {
        line += " ";
        line += route;
    }
    return line;
}

void NeoMatrixDisplay::drawTextLine(int16_t x, int16_t y, const String &text, uint16_t color)
{
    _matrix->setCursor(x, y);
    _matrix->setTextColor(color);
    for (size_t i = 0; i < (size_t)text.length(); ++i)
    {
        _matrix->write(text[i]);
    }
}

bool NeoMatrixDisplay::drawAirlineLogo(const FlightInfo &f)
{
    String operatorIcao = f.operator_icao;
    if (operatorIcao.length() == 0)
        operatorIcao = f.operator_code;

    if (!LogoManager::loadLogo(operatorIcao, _logoBuffer))
        return false;

    // Donor logo assets are 32x32 RGB565. Pixel value 0 is treated as
    // transparent so the black display background remains untouched.
    for (uint16_t y = 0; y < LOGO_HEIGHT && y < _matrixHeight; ++y)
    {
        for (uint16_t x = 0; x < LOGO_WIDTH && x < _matrixWidth; ++x)
        {
            const uint16_t pixel = _logoBuffer[y * LOGO_WIDTH + x];
            if (pixel != 0)
                _matrix->drawPixel(x, y, pixel);
        }
    }

    return true;
}

String NeoMatrixDisplay::truncateToColumns(const String &text, int maxColumns)
{
    if ((int)text.length() <= maxColumns)
        return text;
    if (maxColumns <= 3)
        return text.substring(0, maxColumns);
    return text.substring(0, maxColumns - 3) + String("...");
}

void NeoMatrixDisplay::displaySingleFlightCard(const FlightInfo &f)
{
    const uint16_t borderColor = _matrix->Color(UserConfiguration::TEXT_COLOR_R,
                                                UserConfiguration::TEXT_COLOR_G,
                                                UserConfiguration::TEXT_COLOR_B);
    _matrix->drawRect(0, 0, _matrixWidth, _matrixHeight, borderColor);

    const int charWidth = 6;
    const int charHeight = 8;
    const int padding = 2;

    const String ident = bestIdent(f);
    const String airline = bestAirline(f);
    const MilitaryCallsignMatch military = matchAustralianMilitaryCallsign(f);
    const bool raaf = military.matched && military.service == "RAAF";

    // A 32x32 logo gets the full left-most panel. Text automatically moves
    // right and uses the remaining 128 pixels. Military/GA/missing-logo cards
    // continue to use the full width.
    const bool logoVisible = !military.matched && drawAirlineLogo(f);
    const int16_t startX = logoVisible ? (LOGO_WIDTH + 2) : (1 + padding);
    const int innerWidth = _matrixWidth - startX - 2;
    const int innerHeight = _matrixHeight - 2 - (2 * padding);
    const int maxCols = innerWidth / charWidth;

    // Keep the flight identifier at the front so long airline names cannot
    // truncate away the most useful bit of information.
    String line1;
    if (raaf)
    {
        line1 = String("RAAF");
        if (ident.length())
        {
            line1 += " ";
            line1 += ident;
        }
    }
    else
    {
        line1 = ident;
        if (airline.length())
        {
            if (line1.length())
                line1 += " ";
            line1 += airline;
        }
    }

    String origin = bestAirportLabel(f.origin);
    String dest = bestAirportLabel(f.destination);
    String line2;

    if (origin.length() && dest.length())
    {
        line2 = origin + ">" + dest;
    }
    else if (origin.length())
    {
        line2 = String("FROM ") + origin;
    }
    else if (dest.length())
    {
        line2 = String("TO ") + dest;
    }
    else if (military.matched)
    {
        line2 = "MILITARY FLIGHT";
    }
    else if (!f.enriched)
    {
        line2 = "LIVE ADS-B";
    }
    else
    {
        line2 = "ROUTE UNKNOWN";
    }

    String line3 = f.aircraft_display_name_short.length() ? f.aircraft_display_name_short : f.aircraft_code;
    if (line3.length() == 0 && military.aircraftHint.length())
    {
        line3 = military.aircraftHint;
    }
    if (line3.length() == 0)
    {
        if (raaf)
            line3 = "ROYAL AUSTRALIAN AIR FORCE";
        else if (!f.enriched)
            line3 = "UNENRICHED TARGET";
        else
            line3 = "AIRCRAFT UNKNOWN";
    }

    line1 = truncateToColumns(line1, maxCols);
    line2 = truncateToColumns(line2, maxCols);
    line3 = truncateToColumns(line3, maxCols);

    const uint16_t textColor = _matrix->Color(UserConfiguration::TEXT_COLOR_R,
                                              UserConfiguration::TEXT_COLOR_G,
                                              UserConfiguration::TEXT_COLOR_B);
    const int lineCount = 3;
    const int lineSpacing = 1;
    const int totalTextHeight = lineCount * charHeight + (lineCount - 1) * lineSpacing;
    const int topOffset = 1 + padding + (innerHeight - totalTextHeight) / 2;

    int16_t y = topOffset;
    drawTextLine(startX, y, line1, textColor);
    y += charHeight + lineSpacing;
    drawTextLine(startX, y, line2, textColor);
    y += charHeight + lineSpacing;
    drawTextLine(startX, y, line3, textColor);
}

void NeoMatrixDisplay::displayFlights(const std::vector<FlightInfo> &flights)
{
    if (_matrix == nullptr)
        return;

    _matrix->fillScreen(0);

    if (!flights.empty())
    {
        const unsigned long now = millis();
        const unsigned long intervalMs = TimingConfiguration::DISPLAY_CYCLE_SECONDS * 1000UL;

        if (flights.size() > 1)
        {
            if (now - _lastCycleMs >= intervalMs)
            {
                _lastCycleMs = now;
                _currentFlightIndex = (_currentFlightIndex + 1) % flights.size();
            }
        }
        else
        {
            _currentFlightIndex = 0;
        }

        const size_t index = _currentFlightIndex % flights.size();
        displaySingleFlightCard(flights[index]);
    }
    else
    {
        displayLoadingScreen();
    }

    FastLED.show();
}

void NeoMatrixDisplay::displayLoadingScreen()
{
    if (_matrix == nullptr)
        return;

    _matrix->fillScreen(0);

    const uint16_t borderColor = _matrix->Color(255, 255, 255);
    _matrix->drawRect(0, 0, _matrixWidth, _matrixHeight, borderColor);

    const int charWidth = 6;
    const int charHeight = 8;
    const String loadingText = "...";
    const int textWidth = loadingText.length() * charWidth;

    const int16_t x = (_matrixWidth - textWidth) / 2;
    const int16_t y = (_matrixHeight - charHeight) / 2 - 2;

    const uint16_t textColor = _matrix->Color(UserConfiguration::TEXT_COLOR_R,
                                              UserConfiguration::TEXT_COLOR_G,
                                              UserConfiguration::TEXT_COLOR_B);
    drawTextLine(x, y, loadingText, textColor);

    FastLED.show();
}

void NeoMatrixDisplay::displayMessage(const String &message)
{
    if (_matrix == nullptr)
        return;

    _matrix->fillScreen(0);

    const int charWidth = 6;
    const int charHeight = 6;

    const uint16_t textColor = _matrix->Color(UserConfiguration::TEXT_COLOR_R,
                                              UserConfiguration::TEXT_COLOR_G,
                                              UserConfiguration::TEXT_COLOR_B);

    const int innerWidth = _matrixWidth;
    const int maxCols = innerWidth / charWidth;
    String line = truncateToColumns(message, maxCols);

    const int16_t x = 0;
    const int16_t y = (_matrixHeight - charHeight) / 2;
    drawTextLine(x, y, line, textColor);
    FastLED.show();
}

void NeoMatrixDisplay::showLoading()
{
    displayLoadingScreen();
}
