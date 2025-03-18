--TODO: Change test runner so that it searches all the directories for selftests/*.lua files and then runs all of them
--it can be smart about checking if the __<package_name>__ exists

local runner = require("test_executive")
local td = runner.rootdir(arg[0]).."../../" -- root directory
local incloud = arg[1] == "cloud"

-- Run Core Self Tests --
if __core__ then
    runner.script(td .. "packages/core/selftests/monitor.lua")
    runner.script(td .. "packages/core/selftests/lua_script.lua")
    runner.script(td .. "packages/core/selftests/earth_data.lua")
    runner.script(td .. "packages/core/selftests/dfio.lua")
    runner.script(td .. "packages/core/selftests/message_queue.lua")
    runner.script(td .. "packages/core/selftests/list.lua")
    runner.script(td .. "packages/core/selftests/ordering.lua")
    runner.script(td .. "packages/core/selftests/dictionary.lua")
    runner.script(td .. "packages/core/selftests/table.lua")
    runner.script(td .. "packages/core/selftests/timelib.lua")
    runner.script(td .. "packages/core/selftests/asset_index.lua")
    runner.script(td .. "packages/core/selftests/asset_loaddir.lua")
end

-- Run Streaming Self Tests --
if __streaming__ then
    runner.script(td .. "packages/streaming/selftests/tcp_socket.lua")
    runner.script(td .. "packages/streaming/selftests/udp_socket.lua")
    runner.script(td .. "packages/streaming/selftests/multicast_device_writer.lua")
    runner.script(td .. "packages/streaming/selftests/cluster_socket.lua")
    runner.script(td .. "packages/streaming/selftests/http_server.lua")
    runner.script(td .. "packages/streaming/selftests/http_client.lua")
    runner.script(td .. "packages/streaming/selftests/http_faults.lua")
    runner.script(td .. "packages/streaming/selftests/http_rqst.lua")
    runner.script(td .. "packages/streaming/selftests/record_dispatcher.lua")
    runner.script(td .. "packages/streaming/selftests/limit_dispatch.lua")
end

-- Run Legact Self Tests --
if __legacy__ then
    runner.script(td .. "packages/legacy/selftests/ccsds_packetizer.lua")
end

-- Run AWS Self Tests --
if __aws__ then
    runner.script(td .. "packages/aws/selftests/credential_store.lua")
end

-- Run H5 Self Tests --
if __h5__ then
    runner.script(td .. "packages/h5/selftests/hdf5_file.lua")
end

-- Run Parquet Self Tests --
if __arrow__ then
end

-- Run Raster Self Tests --
if __geo__ then
    runner.script(td .. "packages/geo/selftests/parquet_sampler.lua")
    runner.script(td .. "packages/geo/selftests/geojson_raster.lua")
    runner.script(td .. "packages/geo/selftests/geouser_raster.lua")
end

-- Run ICESat-2 Self Tests
if __icesat2__ and incloud then
    local icesat2_td = td .. "datasets/icesat2/selftests/"
    runner.script(icesat2_td .. "plugin_unittest.lua")
    runner.script(icesat2_td .. "atl03_dataframe.lua")
    runner.script(icesat2_td .. "atl03_reader.lua")
    runner.script(icesat2_td .. "atl03_viewer.lua")
    runner.script(icesat2_td .. "atl03_indexer.lua")
    runner.script(icesat2_td .. "atl03_ancillary.lua")
    runner.script(icesat2_td .. "atl06_ancillary.lua")
    runner.script(icesat2_td .. "h5_file.lua")
    runner.script(icesat2_td .. "h5_element.lua")
    runner.script(icesat2_td .. "h5_2darray.lua")
    runner.script(icesat2_td .. "parameters.lua")
    runner.script(icesat2_td .. "s3_driver.lua")
end

-- Run OPENDATA Self Tests
if __opendata__ and incloud then
    local opendata_td = td .. "datasets/opendata/selftests/"
    runner.script(opendata_td .. "worldcover_reader.lua")
    runner.script(opendata_td .. "globalcanopy_reader.lua")
end

-- Run PGC Self Tests
if __pgc__ and incloud then
    local pgc_td = td .. "datasets/pgc/selftests/"
    runner.script(pgc_td .. "plugin_unittest.lua")
    runner.script(pgc_td .. "arcticdem_reader.lua")
    runner.script(pgc_td .. "arcticdem_reader_batch.lua")
    runner.script(pgc_td .. "temporal_filter_test.lua")
    runner.script(pgc_td .. "url_filter_test.lua")
    runner.script(pgc_td .. "zonal_stats_test.lua")
    runner.script(pgc_td .. "resampling_test.lua")
    runner.script(pgc_td .. "aoi_bbox_test.lua")
    runner.script(pgc_td .. "proj_pipeline_test.lua")
    runner.script(pgc_td .. "remadem_reader.lua")
    runner.script(pgc_td .. "subset_test.lua")
end

-- Run LANDSAT Self Tests
if __landsat__ and incloud then
    local landsat_td = td .. "datasets/landsat/selftests/"
    runner.script(landsat_td .. "plugin_unittest.lua")
    runner.script(landsat_td .. "landsat_reader.lua")
end

-- Run USGS3DEP Self Tests
if __usgs3dep__ and incloud then
    local usg2dep_td = td .. "datasets/usgs3dep/selftests/"
    runner.script(usg2dep_td.. "plugin_unittest.lua")
    runner.script(usg2dep_td .. "usgs3dep_reader.lua")
end

-- Run GEBCO Self Tests
if __gebco__ and incloud then
    local gebco_td = td .. "datasets/gebco/selftests/"
    runner.script(gebco_td .. "gebco_reader.lua")
end

-- Run BLUETOPO Self Tests
if __bluetopo__ and incloud then
    local bluetopo_td = td .. "datasets/bluetopo/selftests/"
    runner.script(bluetopo_td.. "plugin_unittest.lua")
    runner.script(bluetopo_td.. "bluetopo_reader.lua")
end

-- Report Results --
local errors = runner.report()

-- Cleanup and Exit --
sys.quit( errors )
