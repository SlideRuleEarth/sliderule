# SlideRule Development Guide

## Build & Test (macOS with Docker)

```bash
# Build (must use Docker — native macOS build is not supported)
cd targets/slideruleearth
docker run --user $(id -u):$(id -g) \
  -v $(pwd)/../..:$(pwd)/../.. \
  -e ROOT=$(pwd)/../.. \
  --rm 742127912612.dkr.ecr.us-west-2.amazonaws.com/sliderule-buildenv:latest \
  sh -c "cd $(pwd) && make config-release && make"

# Selftest
docker run --rm \
  -v $(pwd)/../..:$(pwd)/../.. \
  -e LOG_FORMAT=FMT_TEXT -e ENVIRONMENT_VERSION=dirty \
  -e PROJECT_BUCKET=sliderule -e PROJECT_FOLDER=cf \
  -e PROJECT_REGION=us-west-2 -e CLUSTER=localhost \
  -e DOMAIN=localhost -e ORCHESTRATOR=http://127.0.0.1:8050 \
  -e AMS=http://127.0.0.1:9082 \
  -e CONTAINER_REGISTRY=742127912612.dkr.ecr.us-west-2.amazonaws.com \
  742127912612.dkr.ecr.us-west-2.amazonaws.com/sliderule-buildenv:latest \
  $(pwd)/../../stage/sliderule/bin/sliderule $(pwd)/../../targets/slideruleearth/test_runner.lua
```

Note: The Makefile's `make build` target hardcodes `-v /data:/data` which doesn't exist on macOS. Use the docker commands above directly.

## Adding, Modifying, or Removing an Endpoint

When you change an endpoint, there are several files that must stay in sync. Follow this checklist:

### Adding a new endpoint

1. **Create the Lua script** in `{package_or_dataset}/endpoints/{name}.lua`
2. **Register it in CMakeLists.txt** — add the file to the `install(FILES ...)` block in the package's `CMakeLists.txt` so it gets installed to `${CONFDIR}/api/`
3. **If it returns Parquet (an `/arrow/*` endpoint):**
   - Add a `GeoDataFrame::registerSchema()` call in the dataset's init function (e.g., `initicesat2()` in `icesat2.cpp`, `initgedi()` in `gedi.cpp`)
   - Match field names and types exactly to the DataFrame's `FieldColumn<T>` declarations in the `.h` file
   - Include `#include "GeoDataFrame.h"` if not already present
4. **Update the OpenAPI base template** — add the endpoint path to `packages/core/data/openapi-base.json`:
   - Add a path entry under the appropriate section
   - For Parquet endpoints: column schemas are injected automatically from `registerSchema()` — no need to add them manually
   - For streaming endpoints: use `$ref: "#/components/responses/StreamingResponse"`
5. **Add selftests** if the endpoint has testable behavior without cloud access

### Modifying an endpoint (adding/removing/renaming columns)

1. **Update the DataFrame** `.h` file (FieldColumn declarations)
2. **Update the `registerSchema()` call** to match the new columns
3. Column schemas in the OpenAPI spec update **automatically** via `/source/openapi` (no manual YAML edit needed for column changes)
4. **Run the selftest** to verify the schema registry still works

### Removing an endpoint

1. **Remove the Lua script** from the `endpoints/` directory
2. **Remove it from CMakeLists.txt** install list
3. **Remove the `registerSchema()` call** if it had one
4. **Remove the path** from `packages/core/data/openapi-base.json`

### Validation

After any endpoint change, verify:
- `make selftest` passes (schema tests validate the registry)
- The OpenAPI base template is valid JSON: `python3 -c "import json; json.load(open('packages/core/data/openapi-base.json'))"`
- Column schemas match C++ code (the schema selftest at `packages/core/selftests/schema.lua` checks this)
- The live spec is correct: `curl http://localhost:9081/source/openapi | python3 -m json.tool`

## Key Architecture

- **`/arrow/*` endpoints** return Parquet files via `ArrowEndpoint` — these are the primary APIs for AI agents and programmatic use
- **`/source/*` endpoints** return JSON (discovery) or binary streaming records (data) via `LuaEndpoint`
- Both endpoint types resolve Lua scripts from `${CONFDIR}/api/{name}.lua`
- The `?query=string` portion of the URL is available in Lua as `_rqst.arg` (not `arg[1]` — that's the POST body)
- `srcid` in output columns is an int32 mapping to granule filenames in the Parquet metadata

## OpenAPI Spec

The server dynamically generates a complete OpenAPI 3.1 spec at `GET /source/openapi`.

- **Base template**: `packages/core/data/openapi-base.json` — paths, parameters, examples (no column schemas)
- **Column schemas**: injected at runtime from `GeoDataFrame::registerSchema()` via `core.schema()`
- **Endpoint code**: `packages/core/endpoints/openapi.lua`
When you add a new Parquet endpoint with `registerSchema()`, its column schema automatically appears in `/source/openapi`. You only need to edit `openapi-base.json` to add the path definition.

## File Layout

- `packages/*/endpoints/*.lua` — core and utility endpoint scripts
- `datasets/*/endpoints/*.lua` — dataset-specific endpoint scripts
- `packages/*/CMakeLists.txt` — install lists for endpoint scripts
- `datasets/*/package/{dataset}.cpp` — where `registerSchema()` calls live (in `init{dataset}()`)
- `packages/core/data/openapi-base.json` — OpenAPI base template (paths + parameters, no column schemas)
- `packages/core/endpoints/openapi.lua` — generates complete OpenAPI spec with live column schemas
- `packages/core/selftests/schema.lua` — selftest for the schema registry
