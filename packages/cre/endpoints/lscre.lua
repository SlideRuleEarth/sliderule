-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    local http_code, size, response, status = cre.list()
    print(status, http_code, size, response)
    return response
end

-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = nil,
    name = "List Containers",
    description = "List available docker images that can be executed by a user",
    logging = core.CRITICAL,
    roles = {"member", "owner"},
    signed = true,
    inputs = nil,
    outputs = {"json"},
    schema = {
        request = nil,
        response = [["application/json": {
            "schema": {
                "type": "array",
                "description": "List of available docker containers",
                "items": {
                    "type": "object",
                    "properties": {
                        "Id": {
                            "type": "string",
                            "description": "Container ID (SHA256 hash)"
                        },
                        "Names": {
                            "type": "array",
                            "items": { "type": "string" },
                            "description": "Container names"
                        },
                        "Image": {
                            "type": "string",
                            "description": "Image reference (digest or tag)"
                        },
                        "ImageID": {
                            "type": "string",
                            "description": "Image ID (SHA256 hash)"
                        },
                        "Command": {
                            "type": "string",
                            "description": "Command used to start the container"
                        },
                        "Created": {
                            "type": "integer",
                            "description": "Unix timestamp of container creation"
                        },
                        "Ports": {
                            "type": "array",
                            "items": { "type": "object" },
                            "description": "Port mappings"
                        },
                        "Labels": {
                            "type": "object",
                            "additionalProperties": { "type": "string" },
                            "description": "Container labels as key-value pairs"
                        },
                        "State": {
                            "type": "string",
                            "description": "Container state (e.g. running, exited)"
                        },
                        "Status": {
                            "type": "string",
                            "description": "Human-readable status string"
                        },
                        "HostConfig": {
                            "type": "object",
                            "properties": {
                                "NetworkMode": { "type": "string" }
                            }
                        },
                        "NetworkSettings": {
                            "type": "object",
                            "properties": {
                                "Networks": {
                                    "type": "object",
                                    "additionalProperties": {
                                        "type": "object",
                                        "properties": {
                                            "NetworkID": { "type": "string" },
                                            "EndpointID": { "type": "string" },
                                            "Gateway": { "type": "string" },
                                            "IPAddress": { "type": "string" },
                                            "IPPrefixLen": { "type": "integer" },
                                            "IPv6Gateway": { "type": "string" },
                                            "GlobalIPv6Address": { "type": "string" },
                                            "GlobalIPv6PrefixLen": { "type": "integer" },
                                            "MacAddress": { "type": "string" }
                                        }
                                    }
                                }
                            }
                        },
                        "Mounts": {
                            "type": "array",
                            "items": {
                                "type": "object",
                                "properties": {
                                    "Type": { "type": "string" },
                                    "Source": { "type": "string" },
                                    "Destination": { "type": "string" },
                                    "Mode": { "type": "string" },
                                    "RW": { "type": "boolean" },
                                    "Propagation": { "type": "string" }
                                }
                            }
                        }
                    }
                }
            }
        }]]
    }
}
