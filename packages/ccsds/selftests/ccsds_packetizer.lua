local runner = require("test_executive")
local packet = require("packet")

-- Requirements --

if not __ccsds__ then
    print("Skipping ccsds packetizer self test")
    return
end

-- CCSDS Packetizer Unit Test --

local cmdoutq = msg.subscribe("cmdoutq")
local payinq = msg.publish("payinq")

local packetizer = ccsds.packetizer("payinq", "cmdoutq", 0xBF, ccsds.CMD, 3)

payinq:sendstring("HELLOWORLD\0")
local val1 = cmdoutq:recvstring(3000)
packet.printPacket(val1, 32)

cmd.exec("CCSDS::DEFINE_COMMAND match.cmd NULL 0xBF 3 19 2")
cmd.exec("ADD_FIELD match.cmd CS UINT8 7 1 NA")
cmd.exec("ADD_FIELD match.cmd msg STRING 8 11 NA")
packet.sendCommand("/match.cmd msg=HELLOWORLD", msg.publish("cmdoutq"), "CS", true)
local val2 = cmdoutq:recvstring(3000)

runner.assert(packet.comparePacket(val1, val2))

-- Report Results --

runner.report()

