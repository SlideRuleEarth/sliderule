-- initialization
local json                  = require("json")
local aws_utils             = require("aws_utils")
local version, _, build, _  = sys.version()
local release               = "3"
local timeout               = 10800 * 1000 -- 3 hours
local result                = { status = true, build = build, start = time.latch(), messages = {} }
local consoleq              = msg.subscribe("consoleq") -- prevents error posting to consoleq

repeat

    -- check global arguments
    if not Arguments then
        table.insert(result["messages"], "no argument supplied")
        result["status"] = false
        break
    end

    -- status arguments
    local resource = Arguments:match("([^,]+)")
    if not resource then
        table.insert(result["messages"], "failed to get arguments")
        result["status"] = false
        break
    else
        table.insert(result["messages"], string.format("processing resource=%s, timeout=%d ms", resource, timeout))
    end

    -- output files
    local parquet_output_file = resource:gsub("ATL03", string.format("atl24r%s/parquet/ATL24", release)):gsub("%.h5", string.format("_00%s_01.parquet", release))
    local h5_output_file = resource:gsub("ATL03", string.format("atl24r%s/h5/ATL24", release)):gsub("%.h5", string.format("_00%s_01.h5", release))
    local xml_output_file = resource:gsub("ATL03", string.format("atl24r%s/xml/ATL24", release)):gsub("%.h5", string.format("_00%s_01.iso.xml", release))
    result["outputs"] = {h5_output_file, parquet_output_file, xml_output_file}

    -- request structure
    local rqst = {
        ["cnf"] = -2,
        ["quality_ph"] = {
            "atl03_quality_nominal", "atl03_quality_afterpulse", "atl03_quality_impulse_response", "atl03_quality_tep", "atl03_quality_noise_burst", "atl03_quality_noise_streak",
            "atl03_quality_tx_part_sat", "atl03_quality_tx_part_sat_afterpulse", "atl03_quality_tx_part_sat_ir_effect", "atl03_quality_tx_part_sat_burst", "atl03_quality_tx_part_sat_streak",
            "atl03_quality_tx_full_sat", "atl03_quality_tx_full_sat_afterpulse", "atl03_quality_tx_full_sat_ir_effect", "atl03_quality_tx_full_sat_burst", "atl03_quality_tx_full_sat_streak"
        },
        ["output"] = {
            ["asset"] = "sliderule-stage",
            ["format"] = "geoparquet",
            ["path"] = parquet_output_file,
            ["with_checksum"] = true
        }
    }

    -- wait for NSIDC credentials
    if not aws_utils.wait_credentials("nsidc-cloud") then
        table.insert(result["messages"], "failed to get NSIDC credentials")
        result["status"] = false
        break
    end

    -- create objects used in processing granule
    local parms         = bathy.parms(rqst, 0, "icesat2", resource)
    local bathymask     = bathy.mask()
    local atl03h5       = h5coro.object(parms["asset"], resource)
    local granule       = icesat2.atl03granule(parms, atl03h5, "consoleq")
    local classifier    = atl24.classifier(parms)
    local refractor     = bathy.refraction(parms)
    local uncertainty   = atl24.uncertainty(parms)
    local sender        = core.framesender(parms, "rspq", resource)
    local dataframe     = core.dataframe({}, {granule=resource, request=json.encode(rqst)})
    local dataframes    = {} -- holds beam dataframes

    -- build final dataframe from beam dataframes
    dataframe:receive("rspq", "consoleq", 6, timeout)
    for _, beam in ipairs(parms["beams"]) do
        local df = bathy.dataframe(beam, parms, bathymask, atl03h5, "consoleq")
        if df then
            df:run(classifier)
            df:run(refractor)
            df:run(uncertainty)
            df:run(sender)
            df:run(core.TERMINATE)
            dataframes[beam] = df
        else
            table.insert(result["messages"], string.format("failed to create dataframe for beam %s", beam))
        end
    end

    -- wait for data to finish being read and deduplicated
    for beam, df in pairs(dataframes) do
        sys.log(core.CRITICAL, string.format("waiting for beam %s", beam))
        local status = df:finished(timeout)
        if status then
            table.insert(result["messages"], string.format("finished dataframe for beam %s", beam))
        else
            table.insert(result["messages"], string.format("failed to finish dataframe for beam %s", beam))
        end
    end

    -- send termination signal to final dataframe
    local rspq = msg.publish("rspq")
    rspq:sendstring("")

    -- wait for final dataframe (blocks until dataframe complete or timeout)
    if not dataframe:waiton(timeout) then
        table.insert(result["messages"], "failed to receive proxied dataframe")
        result["status"] = false
        break
    end

    -- check dataFrame constraints
    sys.log(core.CRITICAL, string.format("constructed final dataframe with %d rows, and %d columns", dataframe:numrows(), dataframe:numcols()))
    if dataframe:numrows() <= 0 or dataframe:numcols() <= 0 then
        table.insert(result["messages"], "produced an empty dataframe")
        result["status"] = false
        break
    end

    -- create arrow dataFrame
    local arrow_dataframe = arrow.dataframe(parms, dataframe)
    if not arrow_dataframe then
        table.insert(result["messages"], "failed to create arrow dataframe")
        result["status"] = false
        break
    end

    -- write dataFrame to parquet file
    local arrow_filename = arrow_dataframe:export()
    if not arrow_filename then
        table.insert(result["messages"], "failed to write dataframe to parquet file")
        result["status"] = false
        break
    end

    -- send parquet file to s3
    local parquet_status = core.send2user(arrow_filename, "consoleq", parms, parquet_output_file)
    if not parquet_status then
        table.insert(result["messages"], "failed to send parquet file")
        result["status"] = false
        break
    end

    -- wait for granule to finish being read
    if not granule:waiton(timeout) then
        table.insert(result["messages"], "failed to read ATL03 granule")
        result["status"] = false
        break
    end

    -- write dataframes to h5 file
    local tmp_filename = string.format("/tmp/%s", resource:gsub("ATL03", "TMP"):gsub("%.h5", ".bin"))
    local atl24_file = atl24.writer(parms, dataframes, granule, release)
    local write_status = atl24_file:write(tmp_filename)
    if not write_status then
        table.insert(result["messages"], "failed to write h5 file")
        result["status"] = false
        break
    end

    -- send h5 file to s3
    local h5_status = core.send2user(tmp_filename, "consoleq", parms, h5_output_file)
    if not h5_status then
        table.insert(result["messages"], "failed to send h5 file")
        result["status"] = false
        break
    end

    -- read xml template file
    local xml_template = io.open(__confdir.."/template.xml", "r")
    if not xml_template then
        table.insert(result["message"], "failed to open xml template")
        result["status"] = false
        break
    end
    local xml_content = xml_template:read("*a")
    xml_template:close()

    -- build polygon string
    local info = granule:export()
    local lat_poly = info["lat_poly"]
    local lon_poly = info["lon_poly"]
    if not lat_poly or not lon_poly or #lat_poly ~= #lon_poly then
        table.insert(result["message"], "invalid latitude and longitude arrays")
        result["status"] = false
        break
    end
    local poly_list = {}
    for i=1,#lat_poly do
        table.insert(poly_list, string.format("%.15f", lat_poly[i]))
        table.insert(poly_list, string.format("%.15f", lon_poly[i]))
    end
    local poly_str = table.concat(poly_list, " ")

    -- populate content of xml file
    local now = time.gps()
    local year, month, day, hour, minute, second, _ = time.gps2date(now)
    local atl24_granule = resource:gsub("ATL03", "ATL24"):gsub("%.h5", string.format("_00%s_01.h5", release))
    xml_content = xml_content:gsub("$GENERATION_DATE", string.format("%04d-%02d-%02dT%02d:%02d:%02dZ", year, month, day, hour, minute, second))
    xml_content = xml_content:gsub("$SLIDERULE_VERSION", version)
    xml_content = xml_content:gsub("$DATA_START_UTC", string.format("%s", info["data_start_utc"]))
    xml_content = xml_content:gsub("$DATA_END_UTC", string.format("%s", info["data_end_utc"]))
    xml_content = xml_content:gsub("$POLY_STR", poly_str) -- lon lat lon lat ...
    xml_content = xml_content:gsub("$GRANULE_RELEASE", string.format("00%s", release)) -- e.g. 003
    xml_content = xml_content:gsub("$GRANULE_NAME", atl24_granule) -- ATL24...h5

    -- write populated template to xml file
    local xml_filename = string.format("/tmp/%s", resource:gsub("%.h5", ".iso.xml"))
    local xml_file = io.open(xml_filename, "w")
    if not xml_file then
        table.insert(result["messages"], "failed to open xml file")
        result["status"] = false
        break
    end
    xml_file:write(xml_content)
    xml_file:close()

    -- send xml file to s3
    local xml_status = core.send2user(xml_filename, "consoleq", parms, xml_output_file)
    if not xml_status then
        table.insert(result["messages"], "failed to send xml file")
        result["status"] = false
        break
    end

until true

-- return results
result["stop"] = time.latch()
return json.encode(result), true