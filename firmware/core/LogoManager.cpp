/*
Purpose: Load airline logos from LittleFS.
Adapted from the LogoManager approach used by the biohead and LuckierTrout
FlightWall forks. Logo assets are 32x32 RGB565 bitmaps named by operator ICAO.
*/
#include "core/LogoManager.h"

#include <LittleFS.h>

const char *LogoManager::LOGO_DIR = "/logos";

bool LogoManager::init()
{
    if (!LittleFS.begin(true))
    {
        Serial.println("LogoManager: LittleFS mount failed; airline logos disabled");
        return false;
    }

    if (!LittleFS.exists(LOGO_DIR))
    {
        LittleFS.mkdir(LOGO_DIR);
    }

    Serial.print("LogoManager: LittleFS mounted, used ");
    Serial.print((unsigned)LittleFS.usedBytes());
    Serial.print(" / ");
    Serial.println((unsigned)LittleFS.totalBytes());
    return true;
}

bool LogoManager::loadLogo(const String &operatorIcao, uint16_t *rgb565Buffer)
{
    if (rgb565Buffer == nullptr)
        return false;

    String icao = operatorIcao;
    icao.trim();
    icao.toUpperCase();

    if (icao.length() == 0)
        return false;

    const String path = String(LOGO_DIR) + "/" + icao + ".bin";
    File f = LittleFS.open(path, "r");
    if (!f)
        return false;

    if (f.size() != LOGO_BUFFER_BYTES)
    {
        Serial.print("LogoManager: invalid logo size for ");
        Serial.println(icao);
        f.close();
        return false;
    }

    const size_t bytesRead =
        f.read(reinterpret_cast<uint8_t *>(rgb565Buffer), LOGO_BUFFER_BYTES);
    f.close();

    return bytesRead == LOGO_BUFFER_BYTES;
}
