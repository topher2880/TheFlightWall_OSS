#pragma once

#include <Arduino.h>

static constexpr uint16_t LOGO_WIDTH = 32;
static constexpr uint16_t LOGO_HEIGHT = 32;
static constexpr size_t LOGO_PIXEL_COUNT = LOGO_WIDTH * LOGO_HEIGHT;
static constexpr size_t LOGO_BUFFER_BYTES = LOGO_PIXEL_COUNT * sizeof(uint16_t);

class LogoManager
{
public:
    // Mount LittleFS and prepare the logo directory.
    // Returns false if the filesystem could not be mounted.
    static bool init();

    // Load /logos/<ICAO>.bin into rgb565Buffer.
    // Logo files are 32x32 RGB565 (2048 bytes) keyed by operator ICAO code.
    static bool loadLogo(const String &operatorIcao, uint16_t *rgb565Buffer);

private:
    static const char *LOGO_DIR;
};
