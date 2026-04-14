#!/usr/bin/env bash
#
# Regenerate tmp_schema_test/ by starting a local sliderule server in Docker
# and fetching all schema endpoints.
#
# Usage:  ./scripts/test_schema.sh [output_dir]
#
# Requires: build artifacts in stage/sliderule/ (run the Docker build first).
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="${1:-$REPO_ROOT/tmp_schema_test}"
CONTAINER_NAME="sliderule-schema-test"
PORT=9081
BASE_URL="http://localhost:$PORT"
IMAGE="742127912612.dkr.ecr.us-west-2.amazonaws.com/sliderule-buildenv:latest"

# Clean up container on exit
cleanup() { docker stop "$CONTAINER_NAME" >/dev/null 2>&1 || true; }
trap cleanup EXIT

# Start the server
echo "Starting sliderule server..."
docker run -d --rm --name "$CONTAINER_NAME" \
  -v "$REPO_ROOT":"$REPO_ROOT" \
  -e LOG_FORMAT=FMT_TEXT \
  -e ENVIRONMENT_VERSION=dirty \
  -e IPV4=127.0.0.1 \
  -p "$PORT:$PORT" \
  "$IMAGE" \
  "$REPO_ROOT/stage/sliderule/bin/sliderule" \
  "$REPO_ROOT/targets/slideruleearth/server-local.lua" >/dev/null

# Wait for server to be ready
echo -n "Waiting for server"
for i in $(seq 1 30); do
    if curl -sf "$BASE_URL/source/health" -o /dev/null 2>/dev/null; then
        echo " ready"
        break
    fi
    echo -n "."
    sleep 1
done

mkdir -p "$OUT_DIR"

# Helper: fetch JSON and pretty-print to file
fetch() {
    curl -sf "$1" | python3 -m json.tool > "$2"
}

# Health check
fetch "$BASE_URL/source/health" "$OUT_DIR/health.json"
echo "health OK"

# All schemas
fetch "$BASE_URL/source/schema" "$OUT_DIR/schema_all.json"
echo "schema_all OK"

# Per-API schemas
APIS=$(python3 -c "import json,sys; print('\n'.join(json.load(sys.stdin).keys()))" < "$OUT_DIR/schema_all.json")
for api in $APIS; do
    fetch "$BASE_URL/source/schema?api=$api" "$OUT_DIR/schema_${api}.json"
    echo "schema_${api} OK"
done

# Error case
fetch "$BASE_URL/source/schema?api=NoSuchApi" "$OUT_DIR/schema_error.json"
echo "schema_error OK"

# OpenAPI spec
fetch "$BASE_URL/source/openapi" "$OUT_DIR/openapi.json"
echo "openapi OK"

echo "Done. Output in $OUT_DIR/"
