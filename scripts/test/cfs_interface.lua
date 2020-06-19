local runner = require("test_executive")
local packet = require("packet")

--local console = require("console")
--console.logger:config(core.INFO)

-- Cfs Interface Unit Test Start --

runner.command("NEW CFS_INTERFACE cfsif cfstlmq cfscmdq 127.0.0.1 5001 127.0.0.1 5002")
runner.command("cfsif::DROP_INVALID FALSE")

ground_tlmq = msg.subscribe("cfstlmq")
ground_cmdq = msg.publish("cfscmdq")

flight_tlmsock = core.udp("127.0.0.1", 5001, core.CLIENT)
flight_cmdsock = core.udp("127.0.0.1", 5002, core.SERVER)

--[[
*****************************
Test1: Invalid Telemetry
*****************************
--]]

testtlm = "BOGUS_TELEMETRY_PACKET"
flight_tlmsock:send(testtlm)
val = ground_tlmq:recvstring(3000) -- 3 second timeout
runner.check('val == testtlm')
print(string.format('TLM: |%s|', val))

--[[
*****************************
Test2: Invalid Command
*****************************
--]]

testcmd = "BOGUS_COMMAND_PACKET"
ground_cmdq:sendstring(testcmd)
val = flight_cmdsock:receive()
runner.check('string.match(val, testcmd) == testcmd')
print(string.format('CMD: |%s|', val))

--[[
*****************************
Test3: Valid Ccsds Telemetry
*****************************
--]]

cmd.exec("CCSDS::DEFINE_TELEMETRY test.tlm NULL 0x421 12 2")
cmd.exec("ADD_FIELD test.tlm days UINT16 6 1 LE")
cmd.exec("ADD_FIELD test.tlm ms UINT32 8 1 LE")
testtlm = msg.create("/test.tlm days=4 ms=300")
raw = testtlm:serialize()
io.write("RAW_TLM: ")
packet.printPacket(raw)
flight_tlmsock:send(raw)
val = ground_tlmq:recvstring(3000) -- 3 second timeout
runner.check(packet.comparePacket(raw, val), "Failed to compare packets")
io.write("VAL_TLM: ")
packet.printPacket(val)

--[[
*****************************
Test4: Valid Ccsds Command
*****************************
--]]

cmd.exec("CCSDS::DEFINE_COMMAND test.cmd NULL 0x420 0 12 2")
cmd.exec("ADD_FIELD test.cmd CS UINT8 7 1 NA")
cmd.exec("ADD_FIELD test.cmd counter UINT32 8 1 LE")
testcmd = msg.create("/test.cmd counter=0x12345678")
raw = testcmd:serialize()
cs = packet.computeChecksum(raw)
print(string.format('CS: %02X', cs))
testcmd:setvalue("CS", cs)
raw = testcmd:serialize()
io.write("RAW_CMD: ")
packet.printPacket(raw)
ground_cmdq:sendrecord(testcmd)
val = flight_cmdsock:receive()
runner.check(packet.comparePacket(raw, val), "Failed to compare packets")
io.write("VAL_CMD: ")
packet.printPacket(val)

-- Clean Up --

-- runner.command("CLOSE cfsif")

-- Report Results --

runner.report()

