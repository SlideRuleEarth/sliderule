# AGENTS.md — SlideRule

SlideRule is a cloud-native science data processing framework. C++ server core, Lua scripting layer, Python/Node.js clients, and Python Lambda microservices.

## Repository Layout

```
apps/node/          # C++ server (the main binary)
  packages/         # Core compile-time packages (core, arrow, aws, geo, h5coro, …)
  datasets/         # Mission-specific packages (icesat2, gedi, swot, …); cannot depend on other datasets
  scripts/          # Lua entry points: server.lua, test_runner.lua, job_runner.lua, openapi.lua
apps/ams/           # Asset Metadata Service (Python/Flask Lambda)
apps/mcp/           # Model Context Protocol Lambda (Python)
apps/provisioner/   # Cluster provisioner Lambda (Python)
apps/runner/        # Job runner Lambda (Python)
apps/authenticator/ # GitHub OAuth Lambda (Python)
clients/python/     # pip-installable Python client
clients/nodejs/     # npm package @sliderule/sliderule
targets/slideruleearth/  # THE primary Makefile — all developer commands live here
build/sliderule/    # CMake out-of-tree build output
stage/sliderule/    # CMake install destination (staged for Docker)
version.txt         # Single source of version truth (e.g. v5.4.4)
```

## C++ Server Build

All commands run from `targets/slideruleearth/`:

```bash
make config-debug    # configure with clang + ASan + clang-tidy + cppcheck
make config-release  # configure release build (no static analysis)
make                 # build + install (make -j8 && make install)
make build           # build inside Docker buildenv container
```

**Critical quirks:**
- Debug builds **require clang** — `ClangOverrides.txt` overrides the compiler to `clang`/`clang++`.
- `clang-tidy` runs with `-warnings-as-errors=*` — any clang-tidy warning **breaks the build**.
- `SKIP_STATIC_ANALYSIS=ON` speeds up iteration but **must be OFF before committing**.
- Inject custom CMake flags without editing the Makefile: `make config-debug USERCFG="-DFOO=ON"`.
- Install prefix is `stage/sliderule/`, not `/usr/local`.

## Running the Server Locally

```bash
make run       # starts server.lua on port 9081 with env vars pre-set
make selftest  # runs test_runner.lua — the only truly offline C++ tests
make job       # runs job_runner.lua
```

The Makefile injects required env vars (`LOG_FORMAT`, `IPV4`, `CLUSTER`, `DOMAIN`, `AMS`, etc.) automatically. Pass `RUNNER=valgrind` to wrap the binary.

## Testing

### C++ Selftests (offline, local)
```bash
make selftest
```
`test_runner.lua` auto-discovers `selftests/*.lua` under all packages and datasets. Tag filter: pass the package name wrapped in `__` (e.g., `__core__`).

### Python Client Tests (require live endpoint)
```bash
# From clients/python/:
make test
# = coverage run -m pytest && coverage report -m

# Single file:
conda run -n sliderule --no-capture-output \
  sh -c 'cd clients/python && coverage run -m pytest tests/test_icesat2.py'
```

### Microservice Tests (require live endpoint or mocks)
```bash
make ams-test [ARGS="-k test_name"]
make authenticator-test [ARGS="-k test_name"]
make provisioner-test [ARGS="-k test_name"]
make mcp-test [ARGS="-k test_name"]
make runner-test
```
`ARGS` is passed directly to pytest. All run inside their respective conda environments.

### Node.js Client Tests (require live endpoint)
```bash
# From clients/nodejs/:
make test DOMAIN=slideruleearth.io ORGANIZATION=sliderule
make test TEST=tests/test_icesat2.js   # single test
```
Node 20 required. Install deps first: `npm ci && npx playwright install --with-deps`.

## Linting / Spell Check

```bash
pre-commit run --all-files   # runs codespell on Python/RST/Markdown only
```
`parm`/`parms` are intentional abbreviations — codespell ignores them. CI enforces this on every push/PR to `main`.

## Conda Environments

Each service has its own named conda env. Use `conda run -n <env>` or activate before running tests.

| Service | Conda env |
|---------|-----------|
| Python client | `sliderule` |
| AMS | `ams` |
| Authenticator | `authenticator` |
| Provisioner | `provisioner` |
| MCP | `mcp` |
| Runner | `runner` |

Install Python client into its env: `make python` (from `targets/slideruleearth/`).

## OpenAPI

```bash
make openapi           # bundle + lint all service specs
make sliderule-openapi # just the server
```
The server binary runs `openapi.lua` to generate the spec, then `@redocly/cli` processes it.

## CI

| Workflow | Trigger |
|----------|---------|
| `linter.yml` | push/PR to `main` — runs codespell only |
| `sliderule-testrunner.yml` | Daily cron — kicks remote integration test run |
| `sliderule-testreport.yml` | Daily cron — checks test report freshness |
| `sliderule-nodejstest.yml` | Weekly cron — Jest tests against live endpoint |
| `python-publish.yml` / `nodejs-publish.yml` | Manual dispatch or release |

**No CI workflow compiles the C++ code.** Server builds and integration tests run against the live deployed cluster, not local builds.

## Release

```bash
make release RELEASE=vX.Y.Z   # from targets/slideruleearth/
```
Updates `version.txt` and `clients/python/version.txt`, commits, tags, pushes, creates GitHub release, builds/pushes Docker images.

## Architecture Notes

- **Lua is the scripting layer:** The server binary accepts a Lua script at startup. All configuration, component instantiation, test orchestration, and OpenAPI generation are in Lua.
- **Package init pattern:** Every C++ package exposes `void init{package}(void)` (e.g., `initcore()`, `initgeo()`). Each has a `{package}.cpp` and `{package}.h`.
- **Python client uses `setup.py`**, not `pyproject.toml`. Published via `python setup.py sdist bdist_wheel && twine upload`.
- **ECR registry:** `742127912612.dkr.ecr.us-west-2.amazonaws.com` (us-west-2).
- **MCP service** (`apps/mcp/`) implements the Anthropic MCP protocol for AI assistant integration with SlideRule data.
