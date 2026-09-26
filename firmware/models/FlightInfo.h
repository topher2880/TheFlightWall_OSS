#pragma once

#include <Arduino.h>
#include <vector>
#include "AirportInfo.h"

struct FlightInfo
{
    // Flight identifiers
    String ident;
    String ident_icao;
    String ident_iata;

    // Raw ADS-B callsign from the state vector. This is retained even when
    // the metadata provider cannot enrich the flight.
    String adsb_callsign;

    // True when detailed metadata was returned by the flight information
    // provider (for example FlightAware AeroAPI).
    bool enriched = false;

    // Operator
    String operator_code;
    String operator_icao;
    String operator_iata;

    // Route
    AirportInfo origin;
    AirportInfo destination;

    // Aircraft
    String aircraft_code;

    // Live ADS-B telemetry copied from the matching OpenSky state vector.
    // Altitude is metres from OpenSky; display code converts it to feet.
    double distance_km = NAN;
    double baro_altitude_m = NAN;
    double heading_deg = NAN;

    // Human-friendly display strings
    String airline_display_name_full;
    String aircraft_display_name_short;
};
