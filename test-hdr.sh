#!/bin/bash
# HDR Testing Script for Moonlight-qt
#
# Usage examples:
#   ./test-hdr.sh              # Use defaults (400 nits)
#   ./test-hdr.sh 360          # Match Horizon Zero Dawn calibration
#   ./test-hdr.sh 300          # Lower (avoid badge)
#   ./test-hdr.sh 600          # Higher (brighter displays)

MAX_NITS=${1:-400}

echo "Starting Moonlight with HDR settings:"
echo "  MAX_NITS: $MAX_NITS"
echo "  MAX_CLL:  $MAX_NITS (auto)"
echo "  MAX_FALL: $((MAX_NITS / 2)) (auto)"
echo ""
echo "To customize all values, use:"
echo "  MOONLIGHT_HDR_MAX_NITS=$MAX_NITS MOONLIGHT_HDR_MAX_CLL=500 MOONLIGHT_HDR_MAX_FALL=250 ./app/moonlight"
echo ""

MOONLIGHT_HDR_MAX_NITS=$MAX_NITS ./app/moonlight
