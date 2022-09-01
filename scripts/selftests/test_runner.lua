local runner = require("test_executive")
local td = runner.rootdir(arg[0]) -- root directory
local console = require("console")

-- Initial Configuration --

sys.setlvl(core.LOG, core.INFO)

-- Run Core Unit Tests --

if __core__ then
    runner.script(td .. "tcp_socket.lua")
    runner.script(td .. "udp_socket.lua")
    runner.script(td .. "multicast_device_writer.lua")
    runner.script(td .. "cluster_socket.lua")
    runner.script(td .. "monitor.lua")
    runner.script(td .. "http_server.lua")
    runner.script(td .. "http_client.lua")
    runner.script(td .. "http_faults.lua")
    runner.script(td .. "lua_script.lua")
end

-- Run AWS Unit Tests --

if __aws__ then
    runner.script(td .. "asset_index.lua")
    runner.script(td .. "credential_store.lua")
end

-- Run H5 Unit Tests --

if __h5__ then
    runner.script(td .. "hdf5_file.lua")
end

-- Run Pistache Unit Tests --

if __pistache__ then
    runner.script(td .. "pistache_endpoint.lua")
end

-- Run Raster Unit Tests --

if __raster__ then
    runner.script(td .. "geojson_raster.lua")
end

-- Run Legacy Unit Tests --

if __legacy__ then
    runner.script(td .. "message_queue.lua")
    runner.script(td .. "list.lua")
    runner.script(td .. "dictionary.lua")
    runner.script(td .. "table.lua")
    runner.script(td .. "timelib.lua")
    runner.script(td .. "ccsds_packetizer.lua")
    runner.script(td .. "cfs_interface.lua")
    runner.script(td .. "record_dispatcher.lua")
    runner.script(td .. "limit_dispatch.lua")
end

-- Report Results --

local errors = runner.report()

-- Cleanup and Exit --

sys.quit( errors )
