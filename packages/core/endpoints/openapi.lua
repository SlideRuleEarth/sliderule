-------------------------------------------------------
-- initialization
-------------------------------------------------------
local global = require("global")

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
            "schemas": { %s, %s },
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
    table.insert(parameters, core.parms():describe("core", "General request parameters that support all requests"))
    table.insert(parameters, cre.parms():describe("cre", "Request parameters for executing container runtime environment processing requests"))
    if __icesat2__ then table.insert(parameters, icesat2.parms():describe("icesat2", "Request parameters for executing ICESat-2 processing requests")) end
    if __gedi__ then table.insert(parameters, icesat2.parms():describe("gedi", "Request parameters for executing GEDI processing requests")) end
    if __swot__ then table.insert(parameters, icesat2.parms():describe("swot", "Request parameters for executing SWOT processing requests")) end
    if __bathy__ then table.insert(parameters, icesat2.parms():describe("bathy", "Request parameters for executing bathymetry processing requests")) end
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
-- paths
-------------------------------------------------------
local function path_schemas()
    local api_list = sys.filefind(__confdir.."/api", ".lua")
    local schema_list = {}
    for _,filepath in ipairs(api_list) do
        local api = filepath:match("([^/]+)%.lua$")
        arg = {[[{"parms": "{}", "asset": "icesat2", "resource": "granule.h5"}]]} -- override global arguments to a representative object when loading API scripts
        local endpoint = require(api)
        if endpoint["schema"] then
            local verb = endpoint["inputs"] and "post" or "get"
            local summary = endpoint["name"]
            local description = endpoint["description"]
            local request_body = request_body_schema(endpoint)
            local response = response_schema(endpoint)
            local schema = string.format([[
                "/%s": {
                    "%s": {
                        "summary": "%s",
                        "description": "%s",
                        "requestBody": { %s },
                        "responses": { %s }
                    }
                }
            ]], api, verb, summary, description, request_body, response)
            table.insert(schema_list, schema)
        end
    end
    return table.concat(schema_list, ",")
end

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local spec = specification_template()
    local version = sys.version()
    local parameters = parameter_schemas()
    local records = record_schemas()
    local paths = path_schemas()
    return string.format(spec, version, parameters, records, paths):gsub("%s+", " ")
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = nil,
    name = "Open API Specification",
    description = "Return the Open API specification for the compliant endpoints, parameters, and dataframes",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    inputs = nil,
    outputs = {"json"},
    schema = {
        request = nil,
        response = [[ "application/json": {
            "schema": {
                "type": "object",
                "description": "OpenAPI specification of SlideRule endpoints"
            }
        } ]]
    }
}