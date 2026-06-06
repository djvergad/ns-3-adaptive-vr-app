# vr-cttc-nr-oran-distance.cc - Distance-Based UE Placement Scenario

## Overview

This is a modified version of `vr-cttc-nr-oran.cc` that implements distance-based UE placement. Instead of having all UEs randomly distributed in a small area, this scenario places:

- **Half of the UEs** very close to the gNB (in a small circle with ~2m radius)
- **Half of the UEs** at a configurable distance from the gNB (arranged in a circle)

This setup allows testing scenarios with heterogeneous channel conditions, where some UEs have excellent connectivity while others experience degraded signal quality.

## Key Modifications

### 1. New Command Line Parameter

```bash
--farUeDistance=<value>
```

- **Description**: Distance in meters of far UEs from the gNB
- **Default**: 50.0 meters
- **Purpose**: Controls how far the distant UEs are placed from the gNB

### 2. UE Placement Logic

The original code used `GridScenarioHelper` to automatically distribute UEs in a 3x3 meter area. The new implementation:

1. Creates UE nodes using the GridScenarioHelper
2. **Manually repositions** each UE based on whether it's a "near" or "far" UE:
   - **Near UEs** (first half): Positioned in a circle with 2m radius around the gNB
   - **Far UEs** (second half): Positioned in a circle at `farUeDistance` from the gNB

### 3. Scenario Size Adjustment

The scenario dimensions are automatically adjusted based on `farUeDistance`:

```cpp
double scenarioSize = std::max(farUeDistance * 2.5, 100.0);
```

This ensures the simulation area is large enough to accommodate far UEs.

### 4. Code Changes Summary

#### Added Include:
```cpp
#include <cmath>  // For M_PI, cos, sin
```

#### Added Parameter:
```cpp
double farUeDistance = 50.0; // Distance in meters for far UEs from gNB
```

#### Modified UE Positioning Section:
- Replaced automatic UE distribution with manual circular placement
- Near UEs: radius = 2.0m
- Far UEs: radius = farUeDistance (configurable)
- Both groups are evenly distributed around the circle using angle calculations

## Usage Examples

### Default Scenario
```bash
./ns3 run "vr-cttc-nr-oran-distance"
```
- Far UEs at 50m from gNB

### Custom Distance
```bash
./ns3 run "vr-cttc-nr-oran-distance --farUeDistance=100"
```
- Far UEs at 100m from gNB

### With 4 UEs per gNB
```bash
./ns3 run "vr-cttc-nr-oran-distance --ueNumPergNb=4 --farUeDistance=75"
```
- 2 UEs near gNB (~2m)
- 2 UEs at 75m from gNB

### With 10 UEs per gNB
```bash
./ns3 run "vr-cttc-nr-oran-distance --ueNumPergNb=10 --farUeDistance=60"
```
- 5 UEs near gNB (~2m)
- 5 UEs at 60m from gNB

## Expected Channel Quality Differences

With the default farUeDistance of **50 meters**:

- **Near UEs (~2m from gNB)**:
  - Excellent RSRP (Reference Signal Received Power)
  - High SINR (Signal-to-Interference-plus-Noise Ratio)
  - Maximum achievable data rates
  - Minimal packet loss

- **Far UEs (50m from gNB)**:
  - Significantly lower RSRP due to path loss
  - Lower SINR
  - Reduced data rates
  - Potential for increased packet loss and retransmissions
  - Still connected but with degraded performance

## Recommended Distance Values

- **30-40m**: Moderate difference in channel quality
- **50m (default)**: Significant but not extreme difference
- **75-100m**: Large difference, far UEs may experience considerable degradation
- **>100m**: Very poor quality for far UEs, may lose connection depending on TX power and environment

## Testing Different Scenarios

To find the optimal distance for your specific test case, you can run experiments with varying distances:

```bash
for dist in 30 40 50 60 70 80 90 100; do
    ./ns3 run "vr-cttc-nr-oran-distance --farUeDistance=$dist --simTag=dist_$dist --outputDir=./results"
done
```

This will create output files for each distance, allowing you to compare throughput, delay, and other metrics.

## Logging and Debugging

To see the UE placement positions, enable the component logging:

```bash
export NS_LOG="CttcNrDemo=level_info"
./ns3 run "vr-cttc-nr-oran-distance"
```

You'll see output like:
```
gNB position: (x, y, z)
Placing N UEs near gNB and M UEs at distance Dm
Near UE 0 positioned at (x, y, z)
Near UE 1 positioned at (x, y, z)
...
Far UE N positioned at (x, y, z)
Far UE N+1 positioned at (x, y, z)
...
```

## Visualization

The UE positions follow this pattern:

```
                    Far UE 2
                        |
                        |
    Far UE 1 ---------- gNB ---------- Far UE 3
                      / | \
                     /  |  \
                  Near UEs (clustered)
                        |
                    Far UE 4
```

Near UEs form a tight cluster around the gNB, while far UEs are distributed evenly on a larger circle.

## Notes

- The code maintains all other functionality from the original `vr-cttc-nr-oran.cc`
- All other command line parameters remain unchanged
- The gNB position is determined by the GridScenarioHelper
- UE height is maintained at 1.5m for all UEs
- gNB height is 10m (as in the original)
