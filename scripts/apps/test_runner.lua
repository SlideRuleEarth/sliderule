local runner = require("test_executive")
local rd = runner.rootdir(arg[0]) -- root directory
local td = rd .. "../tests/" -- test directory
local console = require("console")

-- Initial Configuration --

console.logger:config(core.INFO)

-- Run Core Unit Tests --

if __core__ then
    runner.script(td .. "tcp_socket.lua")
    runner.script(td .. "udp_socket.lua")
    runner.script(td .. "multicast_device_writer.lua")
    runner.script(td .. "cluster_socket.lua")
    runner.script(td .. "asset_index.lua")
end

-- Run H5 Unit Tests --

if __h5__ then
    runner.script(td .. "hdf5_file.lua")
end

-- Run Pistache Unit Tests --

if __pistache__ then
    runner.script(td .. "pistache_endpoint.lua")
end

-- Run Legacy Unit Tests --

if __legacy__ then
    runner.script(td .. "message_queue.lua")
    runner.script(td .. "dictionary.lua")
    runner.script(td .. "timelib.lua")
    runner.script(td .. "ccsds_packetizer.lua")
    runner.script(td .. "cfs_interface.lua")
    runner.script(td .. "record_dispatcher.lua")
    runner.script(td .. "limit_dispatch.lua")
end

-- Run ICESat2 Unit Tests --

if __icesat2__ then
    runner.script(rd .. "../../plugins/icesat2/tests/atl06_elements.lua")
    runner.script(rd .. "../../plugins/icesat2/tests/atl06_unittest.lua")
end

-- Report Results --

runner.report()

-- Cleanup and Exit --

sys.quit()


