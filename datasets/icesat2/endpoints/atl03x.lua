--
-- ENDPOINT:    /source/atl24s
--
local json          = require("json")
local dataframe     = require("dataframe")
local earthdata     = require("earthdata")
local rqst          = json.decode(arg[1])
local parms         = icesat2.parms(rqst["parms"], rqst["key_space"], "icesat2", rqst["resource"])
local userlog       = msg.publish(rspq) -- create user log publisher (alerts)

-------------------------------------------------------
-- query earthdata for resources to process
-------------------------------------------------------
local earthdata_status = earthdata.query(parms, rspq, userlog)
if earthdata_status ~= earthdata.SUCCESS then
    userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> earthdata queury failed : %d", rspq, earthdata_status))
    return
end

-------------------------------------------------------
-- proxy request
-------------------------------------------------------
if not parms["resource"] then
    local df = dataframe.proxy("atl03x", parms, rspq, userlog)
    dataframe.send(df, parms, rspq, userlog)
    return
end

-- get objects
local resource = parms["resource"]
local atl03h5 = h5.object(parms["asset"], resource)
local atl08h5 = h5.object(parms["asset09"], resource08)
local dataframes = {}
userlog:alert(core.INFO, core.RTE_INFO, string.format("request <%s> creating dataframes...", rspq))

-- build dataframes for each beam
for _, beam in ipairs(parms["beams"]) do
    dataframes[beam] = icesat2.atl03x(beam, parms, atl03h5, atl08h5, rspq)

    if not dataframes[beam] then
        userlog:alert(core.CRITICAL, core.RTE_ERROR, string.format("request <%s> on %s failed to create bathy dataframe for beam %s", rspq, resource, beam))
    else
        dataframes[beam]:run(core.TERMINATE)
    end
end

-- wait for dataframes to complete and write to file
for beam,beam_df in pairs(dataframes) do
    local df_finished = beam_df:finished(ctimeout(parms["node_timeout"], starttime), rspq)
    if not df_finished then
        userlog:alert(core.ERROR, core.RTE_TIMEOUT, string.format("request <%s> on %s timed out waiting for dataframe to complete on spot %d", rspq, resource, beam_df:meta("spot")))
    elseif dataframes[beam]:inerror() then
        userlog:alert(core.ERROR, core.RTE_ERROR, string.format("request <%s> on %s failed to create valid bathy dataframe for spot %d", rspq, resource, beam_df:meta("spot")))
    elseif dataframes[beam]:length() == 0 then
        userlog:alert(core.INFO, core.RTE_INFO, string.format("request <%s> on %s created an empty bathy dataframe for spot %d", rspq, resource, beam_df:meta("spot")))
    else
        local spot = beam_df:meta("spot")
        local arrow_df = arrow.dataframe(parms, beam_df)
        local output_filename = string.format("%s/bathy_spot_%d.parquet", crenv.host_sandbox_directory, spot)
        arrow_df:export(output_filename, arrow.PARQUET)
        userlog:alert(core.INFO, core.RTE_INFO, string.format("request <%s> on %s created dataframe for spot %s", rspq, resource, spot))
        outputs[beam] = string.format("%s/bathy_spot_%d.parquet", crenv.container_sandbox_mount, spot)
    end
    -- cleanup to save memory
    beam_df:destroy()
end

-------------------------------------------------------
-- clean up objects to cut down on memory usage
-------------------------------------------------------
atl03h5:destroy()
if atl09h5 then atl09h5:destroy() end
kd490:destroy()

-------------------------------------------------------
-- set atl24 output filename
-------------------------------------------------------
local atl24_filename = parms["output"]["path"]
local pos_last_delim = string.reverse(atl24_filename):find("/") or -(#atl24_filename + 2)
outputs["atl24_filename"] = string.sub(atl24_filename, #atl24_filename - pos_last_delim + 2)
userlog:alert(core.INFO, core.RTE_INFO, string.format("request <%s> generating file %s", rspq, outputs["atl24_filename"]))

-------------------------------------------------------
-- send dataframe back to user
-------------------------------------------------------
local df = arrow.dataframe(crenv.host_sandbox_directory.."/"..tempfile, arrow.PARQUET)
df:send(df, parms, rspq, userlog)


-- write arrow dataframe function that reads parquet (or csv, or geoparquet, etc) file and creates a dataframe from it
-- write core rxdataframe function that receives dataframes being sent
-- support all the nodes sending back parquet files that don't get assembled by the proxy node (need to call it something)
