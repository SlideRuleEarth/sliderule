local runner = require("test_executive")
local console = require("console")
local global = require("global")
local td = runner.rootdir(arg[0]) -- root directory
local loglvl = global.eval(arg[1]) or core.INFO

-- Initial Configuration --
console.monitor:config(core.LOG, loglvl)
sys.setlvl(core.LOG, loglvl)

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
    runner.script(icesat2_td .. "atl06_elements.lua")
    runner.script(icesat2_td .. "atl03_indexer.lua")
    runner.script(icesat2_td .. "h5_file.lua")
    runner.script(icesat2_td .. "s3_driver.lua")
end

-- Run PGC Plugin Self Tests
if __pgc__ then
    local pgc_td = td .. "../../plugins/pgc/selftests/"
    runner.script(pgc_td .. "arcticdem_reader.lua")
    runner.script(pgc_td .. "remadem_reader.lua")
end

-- Report Results --
local errors = runner.report()

-- Cleanup and Exit --
sys.quit( errors )
