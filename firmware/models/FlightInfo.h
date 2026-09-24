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

    // Human-friendly display strings
    String airline_display_name_full;
    String aircraft_display_name_short;
};
