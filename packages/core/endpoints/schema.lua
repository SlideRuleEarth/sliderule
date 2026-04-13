--
-- ENDPOINT:    /source/schema
--
-- INPUT:       query string: api=<api_name>
--              e.g. /source/schema?api=Atl03DataFrame
--              If no api parameter, returns all schemas as a map keyed by API name.
--
-- OUTPUT:      JSON object with column schemas for the current product APIs
--              (the "x" endpoints: atl03x, atl06x, atl08x, atl13x, atl24x,
--              gedi01bx, gedi02ax, gedi04ax, casals1bx) and their associated
--              processing runners (SurfaceFitter, PhoReal, SurfaceBlanket).
--              Deprecated/legacy interfaces are intentionally excluded.
--
local json = require("json")

-- Instantiate schema-only DataFrames to populate the schema registry.
-- Called with no arguments, these create lightweight objects that register
-- column names and descriptions without starting threads or opening files.
if __icesat2__ then
    icesat2.atl03x()
    icesat2.atl06x()
    icesat2.atl08x()
    icesat2.atl13x()
    icesat2.atl24x()
    -- Runner schemas (replace atl03x columns when processing stages are active)
    icesat2.fit()
    icesat2.phoreal()
    icesat2.blanket()
end

if __gedi__ then
    gedi.gedi01bx()
    gedi.gedi02ax()
    gedi.gedi04ax()
end

if __casals__ then
    casals.casals1bx()
end

-- Pre-compute all schemas at load time
local all_schemas = {}
local apis = core.schema()
for api_name, _ in pairs(apis) do
    all_schemas[api_name] = core.schema(api_name)
end
local cached_all = json.encode(all_schemas)

-- Pre-compute per-api responses
local cached_per_api = {}
for api_name, schema in pairs(all_schemas) do
    cached_per_api[api_name] = json.encode(schema)
end

-- Handle request
local function parse_query(qs)
    local params = {}
    if qs then
        for k, v in qs:gmatch("([^&=]+)=([^&=]+)") do
            params[k] = v
        end
    end
    return params
end

local params = parse_query(_rqst and _rqst.arg or nil)
local api = params["api"]

if not api then
    return cached_all
end

if cached_per_api[api] then
    return cached_per_api[api]
end

return json.encode({error = "unknown api: " .. api})
