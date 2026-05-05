-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json = require("json")
local global = require("global")
local earthdata = require("earth_data_query")
local outputfile = arg[1] -- overridden below

-- monitor logs and write to stdout
core.logmon(core.DEBUG):global("LogMonitor")

-- load earthdata assets
earthdata.load()

-- override global arguments to a representative object when loading API scripts
local parms = { asset="icesat2", resource="ATL03_20190314093716_11600203_007_01.h5" }
arg = {json.encode(global.merge({parms=parms}, parms))}

-------------------------------------------------------
-- specification
-------------------------------------------------------
local function specification_template()
    return [[{
        "openapi": "3.0.3",
        "info": {
            "title": "SlideRule OpenAPI Specification",
            "version": "%s",
            "license": {
                "name": "BSD 3-Clause",
                "url": "https://opensource.org/licenses/BSD-3-Clause"
            }
        },
        "components": {
            "schemas": { %s, %s, %s },
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
            },
            "responses": {
                "ServiceUnavailable": {
                    "description": "Insufficient resources are available on the server to process the request",
                    "content": {
                        "text/plain": {
                            "schema": {
                                "type": "string"
                            }
                        }
                    }
                },
                "InternalServerError": {
                    "description": "Server-side error encountered while processing the request",
                    "content": {
                        "text/plain": {
                            "schema": {
                                "type": "string"
                            }
                        }
                    }
                },
                "NotAcceptable": {
                    "description": "The format of the response requested in the request is not supported by the endpoint",
                    "content": {
                        "text/plain": {
                            "schema": {
                                "type": "string"
                            }
                        }
                    }
                },
                "NotFound": {
                    "description": "Requested endpoint not found",
                    "content": {
                        "text/plain": {
                            "schema": {
                                "type": "string"
                            }
                        }
                    }
                },
                "Unauthorized": {
                    "description": "User does not have correct privileges to execute endpoint",
                    "content": {
                        "text/plain": {
                            "schema": {
                                "type": "string"
                            }
                        }
                    }
                }
            }
        },
        "paths": { %s }
    }]]
end

-------------------------------------------------------
-- parameters
-------------------------------------------------------
local function parameter_schemas()
    local parameters = {}
    table.insert(parameters, core.parms():describe("CoreParameters", "General request parameters that support all requests"))
    table.insert(parameters, cre.parms():describe("CreParameters", "Request parameters for executing container runtime environment processing requests"))
    if __h5coro__ then table.insert(parameters, h5coro.parms():describe("H5CoroParameters", "Request parameters for reading HDF5 files using H5Coro")) end
    if __icesat2__ then table.insert(parameters, icesat2.parms():describe("Icesat2Parameters", "Request parameters for executing ICESat-2 processing requests")) end
    if __gedi__ then table.insert(parameters, gedi.parms():describe("GediParameters", "Request parameters for executing GEDI processing requests")) end
    if __swot__ then table.insert(parameters, swot.parms():describe("SwotParameters", "Request parameters for executing SWOT processing requests")) end
    if __bathy__ then table.insert(parameters, bathy.parms():describe("BathyParameters", "Request parameters for executing bathymetry processing requests")) end
    if __casals__ then table.insert(parameters, casals.parms():describe("CasalsParameters", "Request parameters for executing CASALS processing requests")) end
    return table.concat(parameters, ",")
end

-------------------------------------------------------
-- records
-------------------------------------------------------
local function record_schemas()
    local records = {}
    local rectypes = sys.lsrec(false)
    for _,rectype in ipairs(rectypes) do
        table.insert(records, msg.describe(rectype))
    end
    return table.concat(records, ",")
end

-------------------------------------------------------
-- dataframes
-------------------------------------------------------
local function dataframe_schemas()
    local dataframes = {}
    -- bathy
    sys.log(core.INFO, "Building schemas for Bathy dataframes")
    local bathy_parms = bathy.parms()
    table.insert(dataframes, bathy.dataframe("gt1l", bathy_parms):describe("BathyDataFrame", "ICESat-2 photon cloud used for bathymetry processing"))
    -- casals
    sys.log(core.INFO, "Building schemas for CASALS dataframes")
    local casals_parms = casals.parms()
    table.insert(dataframes, casals.casals1bx(casals_parms):describe("Casals1bDataFrame", "CASALS 1B elevations"))
    -- gedi
    sys.log(core.INFO, "Building schemas for GEDI dataframes")
    local gedi_parms = gedi.parms()
    table.insert(dataframes, gedi.gedi01bx("beam0", gedi_parms):describe("Gedi01bDataFrame", "GEDI 1B waveforms"))
    table.insert(dataframes, gedi.gedi02ax("beam0", gedi_parms):describe("Gedi02aDataFrame", "GEDI 2A elevations"))
    table.insert(dataframes, gedi.gedi04ax("beam0", gedi_parms):describe("Gedi04aDataFrame", "GEDI 4A above ground biomass density"))
    -- icesat2
    sys.log(core.INFO, "Building schemas for ICESat-2 dataframes")
    local icesat2_parms = icesat2.parms({phoreal={}, fit={}, atl24={compact=false}, atl08_class={}, yapc={}})
    table.insert(dataframes, icesat2.atl03x("gt1l", icesat2_parms):describe("Atl03DataFrame", "ICESat-2 photon cloud (ATL03)"))
    table.insert(dataframes, icesat2.atl06x("gt1l", icesat2_parms):describe("Atl06DataFrame", "ICESat-2 ice-sheet elevations (ATL06)"))
    table.insert(dataframes, icesat2.atl08x("gt1l", icesat2_parms):describe("Atl08DataFrame", "ICESat-2 vegetation metrics (ATL08)"))
    table.insert(dataframes, icesat2.atl13x("gt1l", icesat2_parms):describe("Atl13DataFrame", "ICESat-2 in-land lake metrics (ATL13)"))
    table.insert(dataframes, icesat2.atl24x("gt1l", icesat2_parms):describe("Atl24DataFrame", "ICESat-2 near-shore bathymetry (ATL24)"))
    -- return list of dataframes
    return table.concat(dataframes, ",")
end

-------------------------------------------------------
-- request body
-------------------------------------------------------
local function request_body_schema(endpoint)
    local content = ""
    local inputs = global.set(endpoint["inputs"])
    if not inputs then return content end
    local schema = [["content": { %s }]]
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
        "503": { "$ref": "#/components/responses/ServiceUnavailable" },
        "500": { "$ref": "#/components/responses/InternalServerError" },
        "406": { "$ref": "#/components/responses/NotAcceptable" },
        "404": { "$ref": "#/components/responses/NotFound" },
        "401": { "$ref": "#/components/responses/Unauthorized" }
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
            local security = security_schema(endpoint)
            local summary = endpoint["name"]
            local description = endpoint["description"]
            local request_body = request_body_schema(endpoint)
            local response = response_schema(endpoint)
            local schema = string.format([[
                "/%s": {
                    "%s": {
                        "security": [ %s ],
                        "summary": "%s",
                        "description": "%s",
                        "requestBody": { %s },
                        "responses": { %s }
                    }
                }
            ]], api, verb, security, summary, description, request_body, response)
            table.insert(schema_list, schema)
        else
            sys.log(core.ERROR, string.format("Failed to generate OpenAPI schema for: %s", api))
        end
    end
    table.sort(schema_list)
    return table.concat(schema_list, ",")
end

-------------------------------------------------------
-- main
-------------------------------------------------------
sys.log(core.INFO, "Building OpenAPI specification of SlideRule API")
local spec = specification_template()
local version = sys.version()
local parameters = parameter_schemas()
local records = record_schemas()
local dataframes = dataframe_schemas()
local paths = path_schemas()
local schema = string.format(spec, version, parameters, records, dataframes, paths):gsub("%s+", " ")
local file = io.open(outputfile, "w")
if file then
    file:write(schema)
    file:close()
else
    sys.log(core.CRITICAL, string.format("Failed to write output: %s", outputfile))
end
sys.quit()