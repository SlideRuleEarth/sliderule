## Summary

Adds JSON schema files as the single source of truth for DataFrame column definitions, enabling `/source/openapi` and `/source/schema` endpoints to serve complete API specifications on a cold server without requiring any DataFrame to be instantiated first.

## Motivation

AI agents and API consumers need `/source/openapi` to return a complete OpenAPI spec immediately after server startup. Without a schema mechanism that reads from disk, column definitions are unavailable until a user makes at least one request per API — creating a chicken-and-egg problem for automated discovery.

## Architecture

```text
schemas/*.json          (single source of truth)
       |
       |---> scripts/generate_columns.py ---> {api}.columns.h  (build-time, #include'd by DataFrame .h files)
       |
       \---> Lua endpoints read JSON directly from disk at request time
                |-- /source/schema   -> returns column definitions
                \-- /source/openapi  -> injects column schemas into OpenAPI 3.1 spec
```

- **JSON schema files** (`schemas/*.json`) -- one per API (12 total), defining columns, metadata, staged fields, and dynamic field documentation
- **Python generator** (`scripts/generate_columns.py`) -- runs at build time via CMake `add_custom_command`, produces `.columns.h` headers with `FieldColumn`/`FieldElement` declarations
- **DataFrame `.h` files** -- `#include "{api}.columns.h"` instead of inline declarations
- **Lua endpoints** -- read JSON from `${CONFDIR}/schemas/` at request time, no C++ dependency
- **Generated headers installed** to `${CONFDIR}/generated/` as inspectable build artifacts

## Changes

- 12 JSON schema files in `schemas/` with support for `"inherits_metadata"` (GEDI base class owns metadata) and `"codegen": false` (FrameRunner schemas built dynamically at runtime)
- `scripts/generate_columns.py` generates `.columns.h` headers and `index.json` from JSON schemas
- CMake integration: `add_custom_command` generates headers, installs JSON schemas and generated headers to `${CONFDIR}`
- 9 DataFrame `.h` files updated to `#include` generated headers
- `Field.h`, `FieldColumn.h`, `FieldElement.h` -- added `description` parameter to constructors (used by generated declarations)
- `openapi.lua` -- new endpoint generating a complete OpenAPI 3.1 spec with column schemas injected from JSON
- `schema.lua` -- new endpoint returning column definitions from JSON files on disk
- `openapi-base.json` -- OpenAPI base template defining paths, parameters, and request/response schemas
- `schema.lua` selftest validates JSON schema files on disk
- `CLAUDE.md` and `docs/.../endpoint_development.md` document the new workflow

## Validation

- **No changes to existing DataFrames** -- all generated column names, types, and declaration order are identical to the inline declarations on `main` (verified by diff across all 9 DataFrame `.h` files)
- **Slight increase in binary/memory footprint** -- each `FieldColumn` and `FieldElement` now carries a `const char* description` (8 bytes per column instance plus string literals in the binary). This enables the generated headers to embed human-readable descriptions directly in the column declarations.
- Build compiles cleanly, 78 selftests pass with 0 errors
- Live server tested: `/source/openapi` returns complete OpenAPI 3.1 spec with all 12 column schemas on a cold server immediately after startup

## Test plan

- [ ] Docker build succeeds (`make config-release && make`)
- [ ] Selftests pass (`make selftest` -- 78 tests, 0 errors)
- [ ] `/source/health` returns `{"healthy":true}`
- [ ] `/source/schema` lists all 12 APIs on a cold server
- [ ] `/source/schema?api=atl06x` returns full column definitions
- [ ] `/source/openapi` returns valid OpenAPI 3.1 spec with `*_columns` schemas injected
- [ ] `/source/schema?api=nonexistent` returns a clean error, not a crash
