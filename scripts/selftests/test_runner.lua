local runner = require("test_executive")
local console = require("console")
local global = require("global")
local td = runner.rootdir(arg[0]) -- root directory
local loglvl = global.eval(arg[1]) or core.INFO

-- Initial Configuration --
-- console.monitor:config(core.LOG, loglvl)
-- sys.setlvl(core.LOG, loglvl)

local maxRuns = 1
for runNum = 1, maxRuns do

    -- Run Core Self Tests --
if __core__ then
    runner.script(td .. "tcp_socket.lua")
    runner.script(td .. "udp_socket.lua")
    runner.script(td .. "multicast_device_writer.lua")
    runner.script(td .. "cluster_socket.lua")
    runner.script(td .. "monitor.lua")
    runner.script(td .. "http_server.lua")
    runner.script(td .. "http_client.lua")
    runner.script(td .. "http_faults.lua")
    runner.script(td .. "http_rqst.lua")
    runner.script(td .. "lua_script.lua")
end

-- Run AWS Self Tests --
if __aws__ then
    runner.script(td .. "asset_index.lua")
    runner.script(td .. "credential_store.lua")
    runner.script(td .. "asset_loaddir.lua")
end

-- Run H5 Self Tests --
if __h5__ then
    runner.script(td .. "hdf5_file.lua")
end

-- Run Pistache Self Tests --
if __pistache__ then
    runner.script(td .. "pistache_endpoint.lua")
end

-- Run Raster Self Tests --
if __geo__ then
    runner.script(td .. "geojson_raster.lua")
--    runner.script(td .. "geouser_raster.lua")
end

-- Run Legacy Self Tests --
if __legacy__ then
    runner.script(td .. "message_queue.lua")
    runner.script(td .. "list.lua")
    runner.script(td .. "ordering.lua")
    runner.script(td .. "dictionary.lua")
    runner.script(td .. "table.lua")
    runner.script(td .. "timelib.lua")
    runner.script(td .. "ccsds_packetizer.lua")
    runner.script(td .. "cfs_interface.lua")
    runner.script(td .. "record_dispatcher.lua")
    runner.script(td .. "limit_dispatch.lua")
end

-- Run ICESat-2 Plugin Self Tests
if __icesat2__ then
    local icesat2_td = td .. "../../plugins/icesat2/selftests/"
    runner.script(icesat2_td .. "plugin_unittest.lua")
    runner.script(icesat2_td .. "atl03_reader.lua")
    runner.script(icesat2_td .. "atl03_indexer.lua")
    runner.script(icesat2_td .. "atl03_ancillary.lua")
    runner.script(icesat2_td .. "atl06_ancillary.lua")
    runner.script(icesat2_td .. "h5_file.lua")
    runner.script(icesat2_td .. "s3_driver.lua")
end

-- Run opendata Plugin Self Tests
if __opendata__ then
    local opendata_td = td .. "../../plugins/opendata/selftests/"
    runner.script(opendata_td .. "worldcover_reader.lua")
end

-- Run PGC Plugin Self Tests
if __pgc__ then
    local pgc_td = td .. "../../plugins/pgc/selftests/"
    runner.script(pgc_td .. "arcticdem_reader.lua")
    runner.script(pgc_td .. "temporal_filter_test.lua")
    runner.script(pgc_td .. "url_filter_test.lua")
    runner.script(pgc_td .. "zonal_stats_test.lua")
    runner.script(pgc_td .. "resampling_test.lua")
    runner.script(pgc_td .. "aoi_bbox_test.lua")
    runner.script(pgc_td .. "proj_pipeline_test.lua")
    runner.script(pgc_td .. "remadem_reader.lua")
    runner.script(pgc_td .. "subset_test.lua")
end

-- Run Landsat Plugin Self Tests
if __landsat__ then
    local landsat_td = td .. "../../plugins/landsat/selftests/"
    runner.script(landsat_td .. "landsat_reader.lua")
end


-- Run usgs3dep Plugin Self Tests
if __usgs3dep__ then
    local usg2dep_td = td .. "../../plugins/usgs3dep/selftests/"
    runner.script(usg2dep_td .. "usgs3dep_reader.lua")
end


if maxRuns > 1 then
    print(string.format("\n--------------------------------\nTest Repeat Run: %d of %d finished\n--------------------------------", runNum, maxRuns))
    sys.wait(3)
end

end  -- test repeat loop end

-- Report Results --
local errors = runner.report()

-- Cleanup and Exit --
sys.quit( errors )
