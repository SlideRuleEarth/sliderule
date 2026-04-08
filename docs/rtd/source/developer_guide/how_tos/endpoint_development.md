# Endpoint Development Guide

This guide documents the steps required when adding, modifying, or removing API endpoints in SlideRule. Multiple files must stay in sync — follow the checklists below to avoid breaking the schema registry, the OpenAPI spec, or the build.

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
| Schema registration | `datasets/*/package/{dataset}.cpp` (in `init{dataset}()`) |
| Schema registry C++ | `packages/core/package/GeoDataFrame.cpp` and `.h` |
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

### 3. Register the schema (Parquet endpoints only)

If the endpoint returns a Parquet DataFrame, add a `GeoDataFrame::registerSchema()` call in the dataset's init function. This powers the `/source/schema` discovery endpoint.

```cpp
#include "GeoDataFrame.h"

void initexample (void)
{
    const uint32_t COL = Field::NESTED_COLUMN;
    GeoDataFrame::registerSchema("your_api", "Human-readable description", {
        {"time_ns",    COL | Field::TIME8,   "Timestamp (Unix ns)",  false},
        {"latitude",   COL | Field::DOUBLE,  "Latitude (degrees)",   false},
        {"longitude",  COL | Field::DOUBLE,  "Longitude (degrees)",  false},
        {"value",      COL | Field::FLOAT,   "Measured value",       false},
        {"spot",       COL | Field::UINT8,   "Spot number",          true},  // true = element (per-batch metadata)
        {"srcid",      COL | Field::INT32,   "Source granule ID",    false},
    });
}
```

Rules:
- Field names and types must **exactly match** the `FieldColumn<T>` declarations in the DataFrame `.h` file
- The last parameter (`is_element`) should be `true` for `FieldElement` metadata fields and `false` for `FieldColumn` row-level data
- Use `Field::NESTED_LIST | Field::FLOAT` for `FieldList<float>` columns and `Field::NESTED_ARRAY | Field::FLOAT` for `FieldArray<float, N>` columns

### 4. Update the OpenAPI spec

Edit the base template at `packages/core/data/openapi-base.json`:

1. Add a **path entry** under the appropriate section (`/arrow/your_api` or `/source/your_api`)
2. For Parquet endpoints, **column schemas are injected automatically** from `registerSchema()` — no need to add them manually
3. Add appropriate `tags`, `operationId`, `summary`, and `description`
4. Reference the correct response type (`ParquetResponse`, `StreamingResponse`, or `JSONResponse`)
The server generates the complete OpenAPI spec at runtime via `GET /source/openapi`. This endpoint reads the base template, then injects column schemas from the live `GeoDataFrame::schemaRegistry`.

### 5. Add selftests

If the endpoint has behavior testable without cloud access, add a Lua selftest in `{package_or_dataset}/selftests/`. The existing `packages/core/selftests/schema.lua` automatically validates that all registered schemas are well-formed.

## Modifying an Endpoint

When adding, removing, or renaming columns in a DataFrame:

1. Update the **DataFrame `.h` file** (`FieldColumn` declarations)
2. Update the **`registerSchema()` call** to match
3. Column schemas in the OpenAPI spec update **automatically** — `/source/openapi` reads from the live registry
4. Run the selftest to verify

## Removing an Endpoint

1. Delete the **Lua script** from `endpoints/`
2. Remove it from the **CMakeLists.txt** install list
3. Remove the **`registerSchema()` call** if present
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

# Test the live endpoint (with server running)
curl -s http://localhost:9081/source/openapi | python3 -m json.tool > /dev/null && echo "OK"
```

The schema selftest (`packages/core/selftests/schema.lua`) verifies:
- `core.schema()` returns a non-empty table of registered APIs
- Each registered schema has a description, a non-empty columns array, and valid field metadata (name, type, description, role)
- Requesting an unknown API raises an error
