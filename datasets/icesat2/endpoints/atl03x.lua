--
-- ENDPOINT:    /source/atl03x
--
local json          = require("json")
local dataframe     = require("dataframe")
local rqst          = json.decode(arg[1])
local parms         = icesat2.parms(rqst["parms"], rqst["key_space"], "icesat2", rqst["resource"])
local userlog       = msg.publish(rspq) -- create user log publisher (alerts)
local start_time    = time.gps() -- used for timeout handling

-------------------------------------------------------
-- proxy request
-------------------------------------------------------
if dataframe.proxy("atl03x", parms, rqst["parms"], rspq, userlog) ~= dataframe.PASS_THROUGH then
    return
end

-------------------------------------------------------
-- generate and send dataframes
-------------------------------------------------------
local dataframes = {}
local resource = parms["resource"]
local atl03h5 = h5.object(parms["asset"], resource)
local atl08h5 = h5.object(parms["asset"], resource:gsub("ATL03", "ATL08"))
local sender = core.framesender(rspq, parms["key_space"], parms["node_timeout"])
--for _, beam in ipairs(parms["beams"]) do
local beam = "gt1l"
    dataframes[beam] = icesat2.atl03x(beam, parms, atl03h5, atl08h5, rspq)
    if dataframes[beam] then
        userlog:alert(core.INFO, core.RTE_INFO, string.format("request <%s> on %s created dataframe for beam %s", rspq, resource, beam))
        dataframes[beam]:run(sender)
        dataframes[beam]:run(core.TERMINATE)
    else
        userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> on %s failed to create dataframe for beam %s", rspq, resource, beam))
    end
--end

-------------------------------------------------------
-- wait for dataframes to complete
-------------------------------------------------------
for beam,df in pairs(dataframes) do
    local status = df:finished(dataframe.timeout(parms["node_timeout"], start_time), rspq)
    if not status then
        userlog:alert(core.ERROR, core.RTE_TIMEOUT, string.format("request <%s> on %s timed out waiting for dataframe to complete for beam %s", rspq, resource, beam))
    end
end
