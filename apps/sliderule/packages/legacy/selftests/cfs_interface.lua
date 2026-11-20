local runner = require("test_executive")
local packet = require("packet")

-- Setup --

runner.command("NEW CFS_INTERFACE cfsif cfstlmq cfscmdq 127.0.0.1 5001 127.0.0.1 5002")
runner.command("cfsif::DROP_INVALID FALSE")

local ground_tlmq = msg.subscribe("cfstlmq")
local ground_cmdq = msg.publish("cfscmdq")

local flight_tlmsock = streaming.udp("127.0.0.1", 5001, streaming.CLIENT)
local flight_cmdsock = streaming.udp("127.0.0.1", 5002, streaming.SERVER)

-- Self Test --

--[[
*****************************
Test1: Invalid Telemetry
*****************************
--]]

runner.unittest("Invalid Telemetry", function()
    TestTlm = "BOGUS_TELEMETRY_PACKET"
    flight_tlmsock:send(TestTlm)
    Value = ground_tlmq:recvstring(3000) -- 3 second timeout
    runner.assert('Value == TestTlm')
    print(string.format('TLM: |%s|', Value))
end)

--[[
*****************************
Test2: Invalid Command
*****************************
--]]

runner.unittest("Invalid Telemetry", function()
    TestCmd = "BOGUS_COMMAND_PACKET"
    ground_cmdq:sendstring(TestCmd)
    Value = flight_cmdsock:receive()
    runner.assert('string.match(Value, TestCmd) == TestCmd')
    print(string.format('CMD: |%s|', Value))
end)

--[[
*****************************
Test3: Valid Ccsds Telemetry
*****************************
--]]

runner.unittest("Invalid Telemetry", function()
    cmd.exec("CCSDS::DEFINE_TELEMETRY test.tlm NULL 0x421 12 2")
    cmd.exec("ADD_FIELD test.tlm days UINT16 6 1 LE")
    cmd.exec("ADD_FIELD test.tlm ms UINT32 8 1 LE")
    TestTlm = msg.create("/test.tlm days=4 ms=300")
    local raw = TestTlm:serialize()
    io.write("RAW_TLM: ")
    packet.printPacket(raw)
    flight_tlmsock:send(raw)
    Value = ground_tlmq:recvstring(3000) -- 3 second timeout
    runner.assert(packet.comparePacket(raw, Value), "Failed to compare packets")
    io.write("VAL_TLM: ")
    packet.printPacket(Value)
end)

--[[
*****************************
Test4: Valid Ccsds Command
*****************************
--]]

runner.unittest("Invalid Telemetry", function()
    cmd.exec("CCSDS::DEFINE_COMMAND test.cmd NULL 0x420 0 12 2")
    cmd.exec("ADD_FIELD test.cmd CS UINT8 7 1 NA")
    cmd.exec("ADD_FIELD test.cmd counter UINT32 8 1 LE")
    TestCmd = msg.create("/test.cmd counter=0x12345678")
    local raw = TestCmd:serialize()
    local cs = packet.computeChecksum(raw)
    print(string.format('CS: %02X', cs))
    TestCmd:setvalue("CS", cs)
    raw = TestCmd:serialize()
    io.write("RAW_CMD: ")
    packet.printPacket(raw)
    ground_cmdq:sendrecord(TestCmd)
    Value = flight_cmdsock:receive()
    runner.assert(packet.comparePacket(raw, Value), "Failed to compare packets")
    io.write("VAL_CMD: ")
    packet.printPacket(Value)
end)

-- Clean Up --

runner.command("CLOSE cfsif")

-- Report Results --

runner.report()
