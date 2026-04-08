# Endpoint Development Guide

This guide documents the steps required when adding, modifying, or removing API endpoints in SlideRule. Multiple files must stay in sync — follow the checklists below.

## Background

SlideRule exposes two families of HTTP endpoints:

- **`/arrow/*`** — Return Parquet/CSV/GeoParquet/Feather files via `ArrowEndpoint`. These are the primary APIs for programmatic use and AI agents.
- **`/source/*`** — Return JSON (discovery endpoints) or binary streaming records (data endpoints) via `LuaEndpoint`. Streaming endpoints require the SlideRule Python client to decode.

Both endpoint types resolve Lua scripts from `${CONFDIR}/api/{name}.lua` at runtime. The script name maps directly to the URL path (e.g., `/arrow/atl06x` loads `atl06x.lua`).

## Key Files

| Purpose | Location |
|---------|----------|
| Endpoint Lua scripts | `packages/*/endpoints/*.lua` and `datasets/*/endpoints/*.lua` |
| CMake install lists | `packages/*/CMakeLists.txt` and `datasets/*/CMakeLists.txt` |
| JSON schema definitions | `schemas/*.json` |
| Column header generator | `scripts/generate_columns.py` |
| Schema endpoint | `packages/core/endpoints/schema.lua` |
| Schema selftest | `packages/core/selftests/schema.lua` |
| OpenAPI base template | `packages/core/data/openapi-base.json` |
| OpenAPI endpoint | `packages/core/endpoints/openapi.lua` |

## Adding a New Endpoint

### 1. Create the Lua script

Create `{package_or_dataset}/endpoints/{name}.lua`. Follow existing endpoints as examples:

- For a **normal response** (JSON): return a string from the script (see `health.lua`, `version.lua`)
- For a **streaming/Parquet response**: use `_rqst.rspq` and the dataframe proxy pattern (see `atl06x.lua`, `gedi04ax.lua`)

Important: query string parameters arrive in `_rqst.arg`, not `arg[1]`. The POST body is in `arg[1]`.

### 2. Register in CMakeLists.txt

Add the Lua file to the `install(FILES ...)` block in the package or dataset's `CMakeLists.txt`:

```cmake
install (
    FILES
        ${CMAKE_CURRENT_LIST_DIR}/endpoints/your_endpoint.lua
        # ... existing files ...
    DESTINATION
        ${CONFDIR}/api
)
```

Without this, the script won't be installed and the endpoint will return "Failed execution".

### 3. Create a JSON schema (Parquet endpoints only)

If the endpoint returns a Parquet DataFrame, create a JSON schema file at `schemas/{api_name}.json`:

```json
{
  "api": "your_api",
  "description": "Human-readable description",
  "columns": [
    {"name": "time_ns",   "type": "time8_t",  "flags": "TIME_COLUMN", "desc": "Timestamp (Unix ns)"},
    {"name": "latitude",  "type": "double",   "flags": "Y_COLUMN",    "desc": "Latitude (degrees)"},
    {"name": "longitude", "type": "double",   "flags": "X_COLUMN",    "desc": "Longitude (degrees)"},
    {"name": "value",     "type": "float",    "flags": "Z_COLUMN",    "desc": "Measured value"}
  ],
  "metadata": [
    {"name": "spot", "type": "uint8_t", "desc": "Spot number"}
  ]
}
```

The JSON schema is the **single source of truth** for column definitions. At build time, `scripts/generate_columns.py` generates a `.columns.h` C++ header from each JSON file, and the DataFrame `.h` file uses `#include "{api_name}.columns.h"` instead of inline declarations.

Available column types: `bool`, `int8_t`, `int16_t`, `int32_t`, `int64_t`, `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`, `float`, `double`, `time8_t`, `string`, `FieldList<T>`, `FieldArray<T,N>`.

Available flags: `TIME_COLUMN`, `X_COLUMN`, `Y_COLUMN`, `Z_COLUMN`.

Optional schema fields:
- `"staged"` — columns conditionally added via `addColumn()` at runtime
- `"dynamic"` — documentation for columns added dynamically (ancillary, samples)
- `"inherits_metadata": true` — metadata is declared in a base class (used by GEDI DataFrames)
- `"codegen": false` — skip C++ header generation (used by FrameRunner schemas like `atl03x-phoreal`)

### 4. Update the OpenAPI spec

Edit the base template at `packages/core/data/openapi-base.json`:

1. Add a **path entry** under the appropriate section (`/arrow/your_api` or `/source/your_api`)
2. Column schemas are **injected automatically** from the JSON schema files — no manual column definitions needed
3. Add appropriate `tags`, `operationId`, `summary`, and `description`
4. Reference the correct response type (`ParquetResponse`, `StreamingResponse`, or `JSONResponse`)

The server generates the complete OpenAPI spec at runtime via `GET /source/openapi`. This endpoint reads the base template, then injects column schemas by reading JSON files from `${CONFDIR}/schemas/`.

### 5. Add selftests

If the endpoint has behavior testable without cloud access, add a Lua selftest in `{package_or_dataset}/selftests/`. The schema selftest (`packages/core/selftests/schema.lua`) automatically validates that all JSON schema files are well-formed and indexed.

## Modifying an Endpoint

When adding, removing, or renaming columns in a DataFrame:

1. Update the **JSON schema** in `schemas/{api_name}.json` — this is the single source of truth
2. The generated `.columns.h` header updates **automatically** at build time
3. Column schemas in the OpenAPI spec update **automatically** — `/source/openapi` reads from JSON files
4. Run the selftest to verify

## Removing an Endpoint

1. Delete the **Lua script** from `endpoints/`
2. Remove it from the **CMakeLists.txt** install list
3. Delete the **JSON schema** file from `schemas/`
4. Remove the **path** from `packages/core/data/openapi-base.json`

## Validation

After any endpoint change:

```bash
# Build
cd targets/slideruleearth
# (use Docker build — see README or CLAUDE.md)

# Run selftests (includes schema validation)
make selftest

# Validate OpenAPI base template
python3 -c "import json; json.load(open('packages/core/data/openapi-base.json'))"

# Validate all JSON schemas
python3 -c "import json, glob; [json.load(open(f)) for f in glob.glob('schemas/*.json')]"

# Test the live endpoint (with server running)
curl -s http://localhost:9081/source/openapi | python3 -m json.tool > /dev/null && echo "OK"
```

The schema selftest (`packages/core/selftests/schema.lua`) verifies:
- `schemas/index.json` exists and lists at least 9 APIs
- Each listed API has a valid JSON schema file on disk
- Schema files contain required fields (`api`, `description`, `columns`)
