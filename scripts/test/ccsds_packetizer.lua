local runner = require("test_executive")
local packet = require("packet")

-- CCSDS Packetizer Unit Test --

cmdoutq = msg.subscribe("cmdoutq")
payinq = msg.publish("payinq")

packetizer = ccsds.packetizer("payinq", "cmdoutq", 0xBF, ccsds.CMD, 3)

payinq:sendstring("HELLOWORLD\0")
val1 = cmdoutq:recvstring(3000)
packet.printPacket(val1, 32)

cmd.exec("CCSDS::DEFINE_COMMAND match.cmd NULL 0xBF 3 19 2")
cmd.exec("ADD_FIELD match.cmd CS UINT8 7 1 NA")
cmd.exec("ADD_FIELD match.cmd msg STRING 8 11 NA")
packet.sendCommand("/match.cmd msg=HELLOWORLD", msg.publish("cmdoutq"), "CS", true)
val2 = cmdoutq:recvstring(3000)

runner.check(packet.comparePacket(val1, val2))

-- Report Results --

runner.report()

