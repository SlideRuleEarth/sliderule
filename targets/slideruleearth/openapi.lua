-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json = require("json")
local global = require("global")
local earthdata = require("earth_data_query")
local outputdir = arg[1]

-- monitor logs and write to stdout
core.logmon(core.DEBUG):global("LogMonitor")

-- load earthdata assets
earthdata.load()

-- override global arguments to a representative object for loading API scripts
local parms = { asset="icesat2", resource="ATL03_20190314093716_11600203_007_01.h5" }
arg = {json.encode(global.merge({parms=parms}, parms))}

-------------------------------------------------------
-- output
-------------------------------------------------------
local function output(filename, content)
    local dir = filename:match("(.+)/[^/]+$")
    if dir then os.execute(string.format("mkdir -p '%s'", dir)) end
    local file = io.open(filename, "w")
    if file then
        local condensed = content:gsub("%s+", " ")
        file:write(condensed)
        file:close()
    else
        sys.log(core.CRITICAL, string.format("Failed to write output: %s", filename))
    end
end

-------------------------------------------------------
-- errors
-------------------------------------------------------
local function error_responses()
    output(outputdir.."/components/responses/ServiceUnavailable.json", [[{
        "description": "Insufficient resources are available on the server to process the request",
        "content": {
            "text/plain": {
                "schema": {
                    "type": "string"
                }
            }
        }
    }]])
    output(outputdir.."/components/responses/InternalServerError.json", [[{
        "description": "Server-side error encountered while processing the request",
        "content": {
            "text/plain": {
                "schema": {
                    "type": "string"
                }
            }
        }
    }]])
    output(outputdir.."/components/responses/NotAcceptable.json", [[{
        "description": "The format of the response requested in the request is not supported by the endpoint",
        "content": {
            "text/plain": {
                "schema": {
                    "type": "string"
                }
            }
        }
    }]])
    output(outputdir.."/components/responses/NotFound.json", [[{
        "description": "Requested endpoint not found",
        "content": {
            "text/plain": {
                "schema": {
                    "type": "string"
                }
            }
        }
    }]])
    output(outputdir.."/components/responses/Unauthorized.json", [[{
        "description": "User does not have correct privileges to execute endpoint",
        "content": {
            "text/plain": {
                "schema": {
                    "type": "string"
                }
            }
        }
    }]])
end

-------------------------------------------------------
-- parameters
-------------------------------------------------------
local function parameter_schemas()
                         output(outputdir.."/components/schemas/CoreParameters.json", core.parms():describe("General request parameters that support all requests"))
                         output(outputdir.."/components/schemas/CreParameters.json", cre.parms():describe("Request parameters for executing container runtime environment processing requests"))
    if __geo__      then output(outputdir.."/components/schemas/GeoParameters.json", geo.parms():describe("Request parameters for sampling raster datasets using GDAL")) end
    if __h5coro__   then output(outputdir.."/components/schemas/H5CoroParameters.json", h5coro.parms():describe("Request parameters for reading HDF5 files using H5Coro")) end
    if __icesat2__  then
                        output(outputdir.."/components/schemas/Icesat2Parameters.json", icesat2.parms():describe("Request parameters for executing ICESat-2 processing requests"))
                        output(outputdir.."/components/schemas/Atl03Parameters.json", icesat2.parms03():describe("Request parameters for executing ICESat-2 photon cloud processing requests"))
                        output(outputdir.."/components/schemas/Atl06Parameters.json", icesat2.parms06():describe("Request parameters for executing ICESat-2 ATL06 subsetting and dataframe requests"))
                        output(outputdir.."/components/schemas/Atl06DispatchParameters.json", icesat2.parms06d():describe("Request parameters for executing ICESat-2 ATL06-SR processing requests"))
                        output(outputdir.."/components/schemas/Atl13Parameters.json", icesat2.parms13():describe("Request parameters for executing ICESat-2 ATL13 dataframe requests"))
                        output(outputdir.."/components/schemas/Atl13sParameters.json", icesat2.parms13s():describe("Request parameters for executing ICESat-2 ATL13 subsetting requests"))
                        output(outputdir.."/components/schemas/Atl24Parameters.json", icesat2.parms24():describe("Request parameters for executing ICESat-2 ATL24 subsetting requests"))
    end
    if __gedi__     then
                        output(outputdir.."/components/schemas/GediParameters.json", gedi.parms():describe("Request parameters for executing GEDI processing requests"))
                        output(outputdir.."/components/schemas/GediL2Parameters.json", gedi.parmsl2():describe("Request parameters for executing GEDI L2 processing requests"))
                        output(outputdir.."/components/schemas/GediL4Parameters.json", gedi.parmsl4():describe("Request parameters for executing GEDI L4 processing requests"))
    end
    if __swot__     then output(outputdir.."/components/schemas/SwotParameters.json", swot.parms():describe("Request parameters for executing SWOT processing requests")) end
    if __bathy__    then output(outputdir.."/components/schemas/BathyParameters.json", bathy.parms():describe("Request parameters for executing bathymetry processing requests")) end
    if __casals__   then output(outputdir.."/components/schemas/CasalsParameters.json", casals.parms():describe("Request parameters for executing CASALS processing requests")) end
end

-------------------------------------------------------
-- records
-------------------------------------------------------
local function record_schemas()
    local rectypes = sys.lsrec(false)
    for _,rectype in ipairs(rectypes) do
        output(string.format(outputdir.."/components/schemas/%s.json", rectype), msg.describe(rectype))
    end
end

-------------------------------------------------------
-- dataframes
-------------------------------------------------------
local function dataframe_schemas()
    -- bathy
    sys.log(core.INFO, "Building schemas for Bathy dataframes")
    output(outputdir.."/components/schemas/BathyDataFrame.json", bathy.dataframe("gt1l", bathy.parms()):describe("ICESat-2 photon cloud used for bathymetry processing"))
    -- casals
    sys.log(core.INFO, "Building schemas for CASALS dataframes")
    output(outputdir.."/components/schemas/Casals1bDataFrame.json", casals.casals1bx(casals.parms()):describe("CASALS 1B elevations"))
    -- gedi
    sys.log(core.INFO, "Building schemas for GEDI dataframes")
    output(outputdir.."/components/schemas/Gedi01bDataFrame.json", gedi.gedi01bx("beam0", gedi.parms()):describe("GEDI 1B waveforms"))
    output(outputdir.."/components/schemas/Gedi02aDataFrame.json", gedi.gedi02ax("beam0", gedi.parmsl2()):describe("GEDI 2A elevations"))
    output(outputdir.."/components/schemas/Gedi04aDataFrame.json", gedi.gedi04ax("beam0", gedi.parmsl4()):describe("GEDI 4A above ground biomass density"))
    -- icesat2
    sys.log(core.INFO, "Building schemas for ICESat-2 dataframes")
    output(outputdir.."/components/schemas/Atl03DataFrame.json", icesat2.atl03x("gt1l", icesat2.parms03({phoreal={}, fit={}, atl24={compact=false}, atl08_class={}, yapc={}})):describe("ICESat-2 photon cloud (ATL03)"))
    output(outputdir.."/components/schemas/Atl06DataFrame.json", icesat2.atl06x("gt1l", icesat2.parms06()):describe("ICESat-2 ice-sheet elevations (ATL06)"))
    output(outputdir.."/components/schemas/Atl08DataFrame.json", icesat2.atl08x("gt1l", icesat2.parms03()):describe("ICESat-2 vegetation metrics (ATL08)"))
    output(outputdir.."/components/schemas/Atl13DataFrame.json", icesat2.atl13x("gt1l", icesat2.parms13()):describe("ICESat-2 in-land lake metrics (ATL13)"))
    output(outputdir.."/components/schemas/Atl24DataFrame.json", icesat2.atl24x("gt1l", icesat2.parms24({atl24={compact=false}})):describe("ICESat-2 near-shore bathymetry (ATL24)"))
end

-------------------------------------------------------
-- framerunners
-------------------------------------------------------
local function framerunner_schemas()
    sys.log(core.INFO, "Building schemas for ICESat-2 framerunners")
    local parms = icesat2.parms03({phoreal={}, fit={}, atl24={compact=false}, atl08_class={}, yapc={}, als={}})
    -- phoreal
    local phoreal_df = icesat2.atl03x("gt1l", parms)
    local phoreal = icesat2.phoreal(parms)
    phoreal_df:run(phoreal)
    phoreal_df:run(core.TERMINATE)
    phoreal_df:finished()
    output(outputdir.."/components/schemas/PhoRealDataFrame.json", phoreal_df:describe("ICESat-2 custom vegetation metrics (PhoREAL)"))
    -- surface fit
    local fit_df = icesat2.atl03x("gt1l", parms)
    local fit = icesat2.fit(parms)
    fit_df:run(fit)
    fit_df:run(core.TERMINATE)
    fit_df:finished()
    output(outputdir.."/components/schemas/SurfaceFitterDataFrame.json", fit_df:describe("ICESat-2 custom surface fit (ATL06-SR)"))
    -- surface blanket
    local blanket_df = icesat2.atl03x("gt1l", parms)
    local blanket = icesat2.blanket(parms)
    blanket_df:run(blanket)
    blanket_df:run(core.TERMINATE)
    blanket_df:finished()
    output(outputdir.."/components/schemas/SurfaceBlanketDataFrame.json", blanket_df:describe("ICESat-2 custom ground and canopy heights (ALS)"))
end

-------------------------------------------------------
-- request body
-------------------------------------------------------
local function request_body_schema(endpoint)
    local content = ""
    local inputs = global.set(endpoint["inputs"])
    if not inputs then return content end
    local schema = [["requestBody": { "content": { %s } },]]
    if inputs then
        content = endpoint["schema"]["request"]
    end
    return string.format(schema, content)
end

-------------------------------------------------------
-- response
-------------------------------------------------------
local function response_schema(endpoint)
    local content = ""
    local outputs = global.set(endpoint["outputs"])
    local schema = [[
        "200": { "description": "OK", "content": { %s } },
        "503": { "$ref": "../components/responses/ServiceUnavailable.json" },
        "500": { "$ref": "../components/responses/InternalServerError.json" },
        "406": { "$ref": "../components/responses/NotAcceptable.json" },
        "404": { "$ref": "../components/responses/NotFound.json" },
        "401": { "$ref": "../components/responses/Unauthorized.json" }
    ]]
    if outputs then
        content = endpoint["schema"]["response"]
    end
    return string.format(schema, content)
end

-------------------------------------------------------
-- security
-------------------------------------------------------
local function security_schema(endpoint)
    local oauth_block = nil
    local signature_block = nil
    local roles = global.set(endpoint["roles"])
    if roles then
        if roles["member"] then oauth_block = "\"OAuth2\": [\"sliderule:access\"]" end
        if roles["owner"] then oauth_block = "\"OAuth2\": [\"sliderule:admin\"]" end
    end
    if endpoint["signed"] then signature_block = "\"SignatureAuth\":[]" end
    if oauth_block and signature_block then
        return string.format("{%s,%s}", oauth_block, signature_block)
    elseif oauth_block then
        return string.format("{%s}", oauth_block)
    elseif signature_block then
        return string.format("{%s}", signature_block)
    else
        return ""
    end
end

-------------------------------------------------------
-- paths
-------------------------------------------------------
local function path_schemas()
    local api_list = sys.filefind(__confdir.."/api", ".lua")
    local schema_list = {}
    for _,filepath in ipairs(api_list) do
        local api = filepath:match("([^/]+)%.lua$")
        sys.log(core.INFO, string.format("Loading endpoint: %s", api))
        local endpoint = require(api)
        if endpoint["schema"] then
            local verb = endpoint["inputs"] and "post" or "get"
            local tags = endpoint["schema"]["tags"] or ""
            local first_tag = tags:match("^%s*(.-)%s*,") or tags:match("^%s*(.-)%s*$") or ""
            local security = security_schema(endpoint)
            local summary = endpoint["name"]
            local description = endpoint["description"]
            local request_body = request_body_schema(endpoint)
            local response = response_schema(endpoint)
            local contents = string.format([[{
                "%s": {
                    "operationId": "%s",
                    "tags": [ %s ],
                    "security": [ %s ],
                    "summary": "%s",
                    "description": "%s",
                    %s
                    "responses": { %s }
                }
            }]], verb, api, first_tag, security, summary, description, request_body, response)
            output(string.format("%s/paths/%s.json", outputdir, api), contents)
            local schema = string.format([[
                "/%s": {
                    "$ref": "./paths/%s.json"
                }
            ]], api, api)
            table.insert(schema_list, schema)
        else
            sys.log(core.ERROR, string.format("Failed to generate OpenAPI schema for: %s", api))
        end
    end
    table.sort(schema_list)
    return table.concat(schema_list, ",")
end

-------------------------------------------------------
-- specification
-------------------------------------------------------
local function specification_root()
    local version = sys.version()
    local paths = path_schemas()
    local template = string.format([[{
        "openapi": "3.0.3",
        "info": {
            "title": "SlideRule OpenAPI Specification",
            "version": "%s",
            "license": {
                "name": "BSD 3-Clause",
                "url": "https://opensource.org/licenses/BSD-3-Clause"
            },
            "description": "APIs to process Earth science datasets in the cloud.  Most endpoints are public though some require authorization and payload signing.
                            Endpoints are grouped two different ways: by series and by package.  The _series_ indentifies the endpoint's underlying implementation:
                            x-series endpoints are based on dataframe construction and manipulation; p-series endpoints are based on custom stream processing;
                            s-series endpoints stream standard data products; v-series endpoints stream summary statistics of standard data products;
                            and a-series endpoints return an immediate ASCII text response typically formatted as json.  The _package_ indentifies the application
                            grouping the endpoint belongs to; for example, the icesat2 package provides endpoints to process ICESat-2 data."
        },
        "servers": [
            { "url": "https://sliderule.slideruleearth.io/source", "description": "User facing web services" }
        ],
        "components": {
            "securitySchemes": {
                "SignatureAuth": {
                    "type": "apiKey",
                    "in": "header",
                    "name": "x-sliderule-signature",
                    "description": "Ed25519 signature of the canonical message, base64-encoded;
                                    the canonical message is constructed as <path>:<timestamp>:<request body>,
                                    where <path> is 'subdomain.domain/path/endpoint',
                                    and <timestamp> is the current Unix time in seconds supplied the x-sliderule-timestamp header"
                },
                "OAuth2": {
                    "type": "oauth2",
                    "flows": {
                        "authorizationCode": {
                            "authorizationUrl": "https://login.slideruleearth.io/auth/github/login",
                            "tokenUrl": "https://login.slideruleearth.io/auth/github/token",
                            "scopes": {
                                "sliderule:access": "Member role access",
                                "sliderule:admin": "Owner role access"
                            }
                        }
                    }
                }
            }
        },
        "paths": { %s }
    }]], version, paths)
    output(outputdir.."/openapi.json", template)
end

-------------------------------------------------------
-- main
-------------------------------------------------------
sys.log(core.INFO, "Building OpenAPI specification of SlideRule API")
error_responses()
parameter_schemas()
record_schemas()
dataframe_schemas()
framerunner_schemas()
specification_root()
sys.quit()