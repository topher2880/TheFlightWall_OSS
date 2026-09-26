/*
Purpose: Orchestrate fetching and enrichment of flight data for display.
Flow:
1) Use BaseStateVectorFetcher to fetch nearby state vectors by geo filter.
2) Preserve every ADS-B callsign as a displayable FlightInfo record.
3) Attempt AeroAPI enrichment for route/operator/aircraft metadata.
4) Enrich names via FlightWallFetcher where metadata is available.

Important: failure to enrich a flight must never make an ADS-B target disappear
from the wall.
*/
#include "core/FlightDataFetcher.h"
#include "config/UserConfiguration.h"
#include "adapters/FlightWallFetcher.h"
#include <algorithm>

FlightDataFetcher::FlightDataFetcher(BaseStateVectorFetcher *stateFetcher,
                                     BaseFlightFetcher *flightFetcher)
    : _stateFetcher(stateFetcher), _flightFetcher(flightFetcher) {}

size_t FlightDataFetcher::fetchFlights(std::vector<StateVector> &outStates,
                                       std::vector<FlightInfo> &outFlights)
{
    outStates.clear();
    outFlights.clear();

    bool ok = _stateFetcher->fetchStateVectors(
        UserConfiguration::CENTER_LAT,
        UserConfiguration::CENTER_LON,
        UserConfiguration::RADIUS_KM,
        outStates);
    if (!ok)
        return 0;

    // Nearest aircraft first so the wall always leads with the target closest
    // to the configured centre point (YBHI in Topher's configuration).
    std::sort(outStates.begin(), outStates.end(), [](const StateVector &a, const StateVector &b) {
        return a.distance_km < b.distance_km;
    });

    size_t enriched = 0;
    for (const StateVector &s : outStates)
    {
        if (s.callsign.length() == 0)
        {
            continue;
        }

        FlightInfo info;
        info.adsb_callsign = s.callsign;
        info.ident = s.callsign; // Always keep a usable identifier for display.
        info.distance_km = s.distance_km;
        info.baro_altitude_m = s.baro_altitude;
        info.heading_deg = s.heading;

        if (_flightFetcher->fetchFlightInfo(s.callsign, info))
        {
            info.enriched = true;

            // Some providers can return a sparse record. Keep the ADS-B callsign
            // as the final fallback so the flight still has a useful identity.
            if (info.ident.length() == 0)
            {
                info.ident = s.callsign;
            }

            FlightWallFetcher fw;
            if (info.operator_icao.length())
            {
                String airlineFull;
                if (fw.getAirlineName(info.operator_icao, airlineFull))
                {
                    info.airline_display_name_full = airlineFull;
                }
            }
            if (info.aircraft_code.length())
            {
                String aircraftShort, aircraftFull;
                if (fw.getAircraftName(info.aircraft_code, aircraftShort, aircraftFull))
                {
                    if (aircraftShort.length())
                    {
                        info.aircraft_display_name_short = aircraftShort;
                    }
                }
            }

            enriched++;
        }
        else
        {
            Serial.print("FlightDataFetcher: keeping ADS-B-only target ");
            Serial.println(s.callsign);
        }

        // Always display the target once OpenSky has seen it, even when
        // AeroAPI enrichment fails or is intentionally sparse.
        outFlights.push_back(info);
    }

    return enriched;
}
