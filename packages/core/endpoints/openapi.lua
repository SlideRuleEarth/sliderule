--
-- ENDPOINT:    /source/openapi
--
-- INPUT:       none
--
-- OUTPUT:      Complete OpenAPI 3.1 spec as JSON with live column schemas
--              injected from the GeoDataFrame schema registry.
--
-- NOTES:       Instantiates schema-only DataFrames at load time to populate
--              the schema registry, reads the base spec template from
--              {confdir}/openapi-base.json, injects column schemas into
--              components.schemas, and returns the complete spec.
--

local json = require("json")

---------------------------------------------------------------
-- Instantiate schema-only DataFrames (no threads, no H5 reads)
---------------------------------------------------------------
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

---------------------------------------------------------------
-- Build response column schemas from the live registry
---------------------------------------------------------------
local function build_column_schemas()
    local result = {}
    local apis = core.schema()

    for api_name, api_desc in pairs(apis) do
        local schema = core.schema(api_name)
        if schema and schema.columns then
            local properties = {}
            local required = {}
            local metadata = {}
            for _, col in ipairs(schema.columns) do
                local prop = {}
                prop.type = col.type
                if col.format and col.format ~= "" then
                    prop.format = col.format
                end
                if col.items_type then
                    local items = {type = col.items_type}
                    if col.items_format and col.items_format ~= "" then
                        items.format = col.items_format
                    end
                    prop.items = items
                end
                if col.description and col.description ~= "" then
                    prop.description = col.description
                end
                if col.condition then
                    prop.description = (prop.description or "") .. " (condition: " .. col.condition .. ")"
                end

                -- Separate per-row columns from per-file metadata
                if col.role == "element" then
                    prop.description = (prop.description or "") .. " (per-file metadata, not a row column)"
                    metadata[col.name] = prop
                else
                    properties[col.name] = prop
                    if not col.condition then
                        table.insert(required, col.name)
                    end
                end
            end

            local schema_entry = {
                type = "object",
                description = api_desc,
                properties = properties
            }
            if #required > 0 then
                schema_entry.required = required
            end
            if next(metadata) then
                schema_entry["x-metadata"] = metadata
            end
            result[api_name] = schema_entry
        end
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
