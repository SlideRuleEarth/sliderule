--
-- ENDPOINT:    /source/atl03sp
--

local json = require("json")
local proxy = require("proxy")

local rqst = json.decode(arg[1])
local resources = rqst["resources"]
local parms = rqst["parms"]

--[[ Given the client side inefficiencies of processing atl03s requests,
the processing of the granules are serialized such that only one
granule (and therefore one set of locks) is consumed at a time;
otherwise it is too easy for a client to make a large atl03sp request
and consume all the locks on the cluster while the server nodes
wait at low CPU utilization for the client to build the dataframe
from the streamed back records --]]
parms["cluster_size_hint"] = parms["cluster_size_hint"] or 1

proxy.proxy(resources, parms, "atl03s", "atl03rec")
