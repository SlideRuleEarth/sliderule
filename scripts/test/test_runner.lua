local runner = require("test_executive")
local rd = runner.rootdir(arg[0])
local console = require("console")

-- Initial Configuration --

console.logger:config(core.INFO)

-- Run Core Unit Tests --

if __core__ then
    runner.script(rd .. "tcp_socket.lua")
    runner.script(rd .. "udp_socket.lua")
    runner.script(rd .. "multicast_device_writer.lua")
end

-- Run Pistache Unit Tests --

if __pistache__ then
    runner.script(rd .. "pistache_endpoint.lua")
end

-- Run ICESat2 Unit Tests --

if __icesat2__ then
    runner.script(rd .. "hdf5_file.lua")
end

-- Run Legacy Unit Tests --

if __legacy__ then
    runner.script(rd .. "message_queue.lua")
    runner.script(rd .. "dictionary.lua")
    runner.script(rd .. "timelib.lua")
    runner.script(rd .. "ccsds_packetizer.lua")
    runner.script(rd .. "cfs_interface.lua")
    runner.script(rd .. "record_dispatcher.lua")
    runner.script(rd .. "limit_dispatch.lua")
end

--[[
runner.script(rd .. "cluster_socket.lua")
--]]

-- Report Results --

runner.report()

-- Cleanup and Exit --

sys.quit()


