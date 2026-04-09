--
-- ENDPOINT:    /source/openapi
--
-- INPUT:       none
--
-- OUTPUT:      Complete OpenAPI 3.1 spec as JSON with live column schemas
--              injected from the GeoDataFrame schema registry.
--
-- NOTES:       Reads the base spec template from {confdir}/openapi-base.json,
--              then injects column schemas from core.schema() into
--              components.schemas before returning the complete spec.
--

local json = require("json")

---------------------------------------------------------------
-- Type mapping: C++ types -> OpenAPI property defs
---------------------------------------------------------------
local function ctype_to_openapi(typ)
    local map = {
        ["bool"]      = {type="boolean"},
        ["int8_t"]    = {type="integer", format="int8"},
        ["int16_t"]   = {type="integer", format="int16"},
        ["int32_t"]   = {type="integer", format="int32"},
        ["int64_t"]   = {type="integer", format="int64"},
        ["uint8_t"]   = {type="integer", format="uint8"},
        ["uint16_t"]  = {type="integer", format="uint16"},
        ["uint32_t"]  = {type="integer", format="uint32"},
        ["uint64_t"]  = {type="integer", format="uint64"},
        ["float"]     = {type="number",  format="float"},
        ["double"]    = {type="number",  format="double"},
        ["time8_t"]   = {type="string",  format="timestamp-ns"},
        ["string"]    = {type="string"},
    }

    local m = map[typ]
    if m then return m end

    -- Handle array<T> and list<T>
    local inner = typ:match("^array<(.+)>$") or typ:match("^list<(.+)>$")
    if inner then
        local item = map[inner] or {type="string"}
        return {type="array", items=item}
    end

    return {type="string", description="unknown type: " .. typ}
end

---------------------------------------------------------------
-- Build response column schemas from the live registry
---------------------------------------------------------------
local function build_column_schemas()
    local result = {}
    local apis = core.schema()

    for api_name, api_desc in pairs(apis) do
        local schema = core.schema(api_name)
        local properties = {}
        for _, col in ipairs(schema.columns) do
            local prop = ctype_to_openapi(col.type)
            prop.description = col.description
            if col.role == "element" then
                prop.description = (prop.description or "") .. " (per-batch metadata)"
            end
            properties[col.name] = prop
        end

        local schema_key = api_name .. "_columns"
        result[schema_key] = {
            type = "object",
            description = api_desc .. " — auto-generated from schema registry",
            properties = properties
        }
    end

    return result
end

---------------------------------------------------------------
-- Read the base spec template
---------------------------------------------------------------
local base_path = __confdir .. "/openapi-base.json"
local f, err = io.open(base_path, "r")
if not f then
    return json.encode({error = "failed to read base spec: " .. (err or "unknown")}), false
end
local base_text = f:read("*a")
f:close()

local ok, base_spec = pcall(json.decode, base_text)
if not ok then
    return json.encode({error = "failed to parse base spec: " .. tostring(base_spec)}), false
end

---------------------------------------------------------------
-- Inject live column schemas
---------------------------------------------------------------
if not base_spec.components then
    base_spec.components = {}
end
if not base_spec.components.schemas then
    base_spec.components.schemas = {}
end

local column_schemas = build_column_schemas()
for k, v in pairs(column_schemas) do
    base_spec.components.schemas[k] = v
end

---------------------------------------------------------------
-- Add version info
---------------------------------------------------------------
local version, build = sys.version()
if base_spec.info then
    base_spec.info.version = version
end

---------------------------------------------------------------
-- Return the complete spec
---------------------------------------------------------------
return json.encode(base_spec)
