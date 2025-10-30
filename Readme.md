# ADS-B Aircraft Display for LSZH (Zurich Airport)

Real-time aircraft tracking display showing all aircraft within 20 nautical miles of Zurich Airport using the OpenSky Network API.

## Features

- Real-time aircraft tracking using OpenSky Network's free API
- 20 nautical mile radius around LSZH (Zurich Airport)
- Displays:
  - Aircraft callsign
  - Altitude in feet
  - Ground speed in knots
  - Distance from LSZH in nautical miles
- Geographic positioning on screen
- Auto-refresh every 10 seconds


## Requirements

- GCC compiler
- libcurl (for API requests)
- libjansson (for JSON parsing)
- Linux/Unix system

## Installation

### Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install build-essential libcurl4-openssl-dev libjansson-dev
```

### Fedora/RHEL:
```bash
sudo dnf install gcc make libcurl-devel jansson-devel
```

### macOS:
```bash
brew install curl jansson
```

## Compilation

```bash
make
```

Or manually:
```bash
gcc -Wall -Wextra -std=c11 -O2 -o aircraft_display aircraft_display.c -lcurl -ljansson -lm
```

## Usage

```bash
./aircraft_display
```

The display will continuously update, showing:
- Aircraft positions relative to LSZH (marked with '+')
- Each aircraft marked with 'X'
- Aircraft information (callsign, altitude, speed, distance)
- Total aircraft count in the area

Press Ctrl+C to exit.

## Display Layout

```
LSZH - Aircraft within 20nm - Count: 5

                    SWR123
                    Alt:35000ft
                    Spd:450kt
                    Dst:12.3nm
                   /
                  X

              +  (LSZH center)
```

## OpenSky Network API

This program uses the free OpenSky Network API which:
- Provides real ADS-B data from aircraft worldwide
- Has a rate limit of approximately 1 request per 10 seconds for anonymous users
- No API key required for basic usage
- More info: https://opensky-network.org/apidoc/

## Coordinates

- LSZH (Zurich Airport): 47.458056°N, 8.548056°E
- Range: 20 nautical miles (37 km)

## Notes

- The display updates every 10 seconds to respect API rate limits
- Aircraft without valid position data are filtered out

## Troubleshooting

If you see "Failed to fetch aircraft data":
- Check your internet connection
- Verify that OpenSky Network API is accessible
- Wait a few seconds and the program will retry automatically

If compilation fails:
- Ensure all dependencies are installed
- Check that pkg-config can find libcurl and libjansson
