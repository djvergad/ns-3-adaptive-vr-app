#!/bin/bash
set -euxo pipefail
source ns3venv/bin/activate
#stdbuf -i0 -o0 -e0 python adaptive-sem-simulations.py --campaignName 842e301 --paramSet nStas --cores 8
python adaptive-sem-simulations.py --campaignName d388aff --paramSet nStas --cores 48
