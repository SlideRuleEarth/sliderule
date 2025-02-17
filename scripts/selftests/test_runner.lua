local runner = require("test_executive")
local td = runner.rootdir(arg[0]) -- root directory
local incloud = arg[1] == "cloud"

-- Run Core Self Tests --
if __core__ then
    runner.script(td .. "monitor.lua")
    runner.script(td .. "http_server.lua")
    runner.script(td .. "lua_script.lua")
    runner.script(td .. "earth_data.lua")
    runner.script(td .. "message_queue.lua")
    runner.script(td .. "list.lua")
    runner.script(td .. "ordering.lua")
    runner.script(td .. "dictionary.lua")
    runner.script(td .. "table.lua")
    runner.script(td .. "timelib.lua")
end

-- Run Streaming Self Tests --
if __streaming__ then
    runner.script(td .. "tcp_socket.lua")
    runner.script(td .. "udp_socket.lua")
    runner.script(td .. "multicast_device_writer.lua")
    runner.script(td .. "cluster_socket.lua")
    runner.script(td .. "http_client.lua")
    runner.script(td .. "http_faults.lua")
    runner.script(td .. "http_rqst.lua")
end

-- Run Legact Self Tests --
if __legacy__ then
    runner.script(td .. "ccsds_packetizer.lua")
    runner.script(td .. "record_dispatcher.lua")
    runner.script(td .. "limit_dispatch.lua")
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

-- Run Parquet Self Tests --
if __arrow__ then
    runner.script(td .. "parquet_sampler.lua")
end

-- Run Raster Self Tests --
if __geo__ then
    runner.script(td .. "geojson_raster.lua")
    runner.script(td .. "geouser_raster.lua")
end

-- Run ICESat-2 Plugin Self Tests
if __icesat2__ and incloud then
    local icesat2_td = td .. "../../datasets/icesat2/selftests/"
    runner.script(icesat2_td .. "plugin_unittest.lua")
    runner.script(icesat2_td .. "atl03_reader.lua")
    runner.script(icesat2_td .. "atl03_viewer.lua")
    runner.script(icesat2_td .. "atl03_indexer.lua")
    runner.script(icesat2_td .. "atl03_ancillary.lua")
    runner.script(icesat2_td .. "atl06_ancillary.lua")
    runner.script(icesat2_td .. "h5_file.lua")
    runner.script(icesat2_td .. "h5_element.lua")
    runner.script(icesat2_td .. "h5_2darray.lua")
    runner.script(icesat2_td .. "s3_driver.lua")
end

-- Run OPENDATA Plugin Self Tests
if __opendata__ and incloud then
    local opendata_td = td .. "../../datasets/opendata/selftests/"
    runner.script(opendata_td .. "worldcover_reader.lua")
    runner.script(opendata_td .. "globalcanopy_reader.lua")
end

-- Run PGC Plugin Self Tests
if __pgc__ and incloud then
    local pgc_td = td .. "../../datasets/pgc/selftests/"
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

-- Run LANDSAT Plugin Self Tests
if __landsat__ and incloud then
    local landsat_td = td .. "../../datasets/landsat/selftests/"
    runner.script(landsat_td .. "plugin_unittest.lua")
    runner.script(landsat_td .. "landsat_reader.lua")
end

-- Run USGS3DEP Plugin Self Tests
if __usgs3dep__ and incloud then
    local usg2dep_td = td .. "../../datasets/usgs3dep/selftests/"
    runner.script(usg2dep_td.. "plugin_unittest.lua")
    runner.script(usg2dep_td .. "usgs3dep_reader.lua")
end

-- Run GEBCO Plugin Self Tests
if __gebco__ and incloud then
    local gebco_td = td .. "../../datasets/gebco/selftests/"
    runner.script(gebco_td .. "gebco_reader.lua")
end

-- Run BLUETOPO Plugin Self Tests
if __bluetopo__ and incloud then
    local bluetopo_td = td .. "../../datasets/bluetopo/selftests/"
    runner.script(bluetopo_td.. "plugin_unittest.lua")
    runner.script(bluetopo_td.. "bluetopo_reader.lua")
end

-- Run Default Parameters Self Tests for all modules
runner.script(td .. "parms_tojson.lua")

-- Report Results --
local errors = runner.report()

-- Cleanup and Exit --
sys.quit( errors )
