local runner = require("test_executive")
local rd = runner.rootdir(arg[0])
local console = require("console")

-- Initial Configuration --

console.logger:config(core.INFO)

-- Run Unit Tests --

runner.script(rd .. "tcp_socket.lua")
runner.script(rd .. "udp_socket.lua")
runner.script(rd .. "multicast_device_writer.lua")

-- Run Legacy Unit Tests --

runner.script(rd .. "message_queue.lua")
runner.script(rd .. "dictionary.lua")
runner.script(rd .. "timelib.lua")
runner.script(rd .. "ccsds_packetizer.lua")
runner.script(rd .. "cfs_interface.lua")
runner.script(rd .. "record_dispatcher.lua")
runner.script(rd .. "limit_dispatch.lua")

--[[
runner.script(rd .. "cluster_socket.lua")
--]]

-- Report Results --

runner.report()

-- Cleanup and Exit --

sys.quit()


