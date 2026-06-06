# Distance-Based UE Scenario - Quick Guide

## Problem: UEs Have Same Spectral Efficiency

If you're seeing both near and far UEs with the same SE (e.g., SE: 5.55), you need to adjust channel model parameters.

## Solution: Use These Parameters

### Recommended Configuration for Clear SE Differentiation

```bash
./ns3 run "vr-cttc-nr-oran-distance -- \
  --burstGeneratorType=oran-util-udp-no-queue \
  --ueNumPergNb=2 \
  --farUeDistance=50 \
  --channelScenario=UMa \
  --enableShadowing=true \
  --totalTxPower=23 \
  --simulationTime=2s \
  --RngRun=5"
```

### Quick Parameter Guide

| Parameter | Default | Purpose | Recommended Values |
|-----------|---------|---------|-------------------|
| `--channelScenario` | `UMa` | Channel propagation model | **UMa** (best differentiation), RMa (extreme), UMi (minimal) |
| `--enableShadowing` | `true` | Enable realistic fading | Keep **true** for SE difference |
| `--totalTxPower` | `23` | gNB TX power (dBm) | 20-25 dBm (lower = more differentiation) |
| `--farUeDistance` | `50` | Far UE distance (meters) | 40-80m with UMa, 60-150m with RMa |

## Channel Scenario Details

### UMa (Urban Macro) - RECOMMENDED ⭐
- **Best for**: 40-200m range
- **Path Loss**: High enough for clear differentiation
- **Expected**: Near UEs SE ~8-10, Far UEs SE ~3-5

```bash
--channelScenario=UMa --farUeDistance=50 --totalTxPower=23
```

### RMa (Rural Macro) - For Extreme Differences
- **Best for**: 60-300m range
- **Path Loss**: Very high
- **Expected**: Near UEs SE ~8-10, Far UEs SE ~1-3
- **Warning**: Far UEs may disconnect if distance > 150m

```bash
--channelScenario=RMa --farUeDistance=80 --totalTxPower=25
```

### UMi (Urban Micro) - Not Recommended for This Scenario
- **Path Loss**: Too low for significant differentiation
- **Expected**: Both near and far UEs will have similar SE
- **Use case**: Only for very short-range scenarios

## Troubleshooting

### Problem: All UEs Still Have Same SE
**Solutions (try in order):**
1. Confirm shadowing is enabled: `--enableShadowing=true`
2. Reduce TX power: `--totalTxPower=20`
3. Increase distance: `--farUeDistance=70`
4. Switch to RMa: `--channelScenario=RMa`

### Problem: Far UEs Lose Connection
**Solutions:**
1. Increase TX power: `--totalTxPower=26`
2. Reduce distance: `--farUeDistance=40`
3. Switch to UMa if using RMa: `--channelScenario=UMa`

### Problem: Want Even More Differentiation
**Solutions:**
1. Disable beamforming (requires code modification)
2. Lower TX power: `--totalTxPower=18`
3. Use RMa with longer distance: `--channelScenario=RMa --farUeDistance=100`

## Testing Different Scenarios

### Test 1: Moderate Differentiation
```bash
./ns3 run "vr-cttc-nr-oran-distance -- \
  --channelScenario=UMa \
  --farUeDistance=45 \
  --totalTxPower=24 \
  --ueNumPergNb=4"
```

### Test 2: High Differentiation
```bash
./ns3 run "vr-cttc-nr-oran-distance -- \
  --channelScenario=UMa \
  --farUeDistance=60 \
  --totalTxPower=21 \
  --ueNumPergNb=4"
```

### Test 3: Extreme Differentiation
```bash
./ns3 run "vr-cttc-nr-oran-distance -- \
  --channelScenario=RMa \
  --farUeDistance=80 \
  --totalTxPower=25 \
  --ueNumPergNb=4"
```

## Checking Results

### In the Log Output
Look for lines like:
```
OranLogicVrBitrate:GetFairShareBitrate(): [DEBUG] RNTI: 1 LCID: 5 Bitrate: 200000000 bps SE: 8.2
OranLogicVrBitrate:GetFairShareBitrate(): [DEBUG] RNTI: 2 LCID: 5 Bitrate: 80000000 bps SE: 3.1
```

**Good differentiation**: SE values differ by >50% (e.g., 8.2 vs 3.1)
**Poor differentiation**: SE values differ by <20% (e.g., 5.5 vs 5.3)

### Enable Position Logging
```bash
export NS_LOG="CttcNrDemo=level_info"
./ns3 run vr-cttc-nr-oran-distance
```

You'll see:
```
gNB position: (x, y, z)
Near UE 0 positioned at (x, y, z)  // ~2m from gNB
Far UE 1 positioned at (x, y, z)   // 50m from gNB
```

## Understanding Path Loss

Approximate path loss for different scenarios at 28 GHz:

| Scenario | 2m (near) | 50m (far) | Difference |
|----------|-----------|-----------|------------|
| UMi | ~70 dB | ~95 dB | 25 dB |
| UMa | ~75 dB | ~110 dB | **35 dB** ⭐ |
| RMa | ~80 dB | ~120 dB | **40 dB** |

**Key**: Larger difference = Better SE differentiation

## Summary: Quick Start

For most use cases, start with:

```bash
./ns3 run "vr-cttc-nr-oran-distance -- \
  --burstGeneratorType=oran-util-udp-no-queue \
  --ueNumPergNb=2 \
  --channelScenario=UMa \
  --farUeDistance=50 \
  --totalTxPower=23"
```

This should give you:
- Near UEs: SE ~7-9, excellent quality
- Far UEs: SE ~3-5, degraded but connected

Adjust `farUeDistance` and `totalTxPower` to fine-tune the differentiation level.
