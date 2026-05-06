-------------------------------------------------------
-- initialization
-------------------------------------------------------
local json      = require("json")
local proxy     = require("proxy")
local rqst      = json.decode(arg[1])
local resources = rqst["resources"]
local parms     = rqst["parms"]

-------------------------------------------------------
-- main
-------------------------------------------------------
local function main()
    --[[ Given the client side inefficiencies of processing atl03s requests,
    the processing of the granules are serialized such that only one
    granule (and therefore one set of locks) is consumed at a time;
    otherwise it is too easy for a client to make a large atl03sp request
    and consume all the locks on the cluster while the server nodes
    wait at low CPU utilization for the client to build the dataframe
    from the streamed back records --]]
    parms["cluster_size_hint"] = parms["cluster_size_hint"] or 1
    proxy.proxy(resources, parms, "atl03s", "atl03rec")
end


-------------------------------------------------------
-- endpoint
-------------------------------------------------------
return {
    main = main,
    parms = parms,
    name = "ATL03 Parallel Subsetter",
    description = "Spatially and temporally subsets multiple ATL03 photon clouds with additional filters (p-series)",
    logging = core.CRITICAL,
    roles = {},
    signed = false,
    inputs = {"json"},
    outputs = {"binary", "arrow"},
    schema = {
        request = [[ "application/json": {
            "schema": {
                "$ref": "../components/schemas/Icesat2Parameters.json"
            }
        } ]],
        response = [[ "application/octet-stream": {
            "schema": {
                "allOf": [
                    { "$ref": "../components/schemas/atl03rec.json" },
                    { "$ref": "../components/schemas/atl03rec.photons.json" }
                ],
                "description": "Stream of binary-encoded ICESat-2 photon measurements (ATL03)"
            }
        } ]]
    }
}
