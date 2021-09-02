--------------------------------------------------------------------------------------
-- computeChecksum  -
--
--   Notes: assumes that checksum is zero to start out
--------------------------------------------------------------------------------------
local function computeChecksum(pkt)

    local cs = 0xFF

    for c in pkt:gmatch(".") do
        b = string.byte(c, 1)
        cs = cs ~ b
    end

    return cs

end

--------------------------------------------------------------------------------------
-- setChecksum  -
--
--   Notes: populates 'fld' with the checksum over a RecordObject user data type 'rud'
--------------------------------------------------------------------------------------
local function setChecksum(rud,fld)

	raw = rud:serialize()
	cs = computeChecksum(raw)
	rud:setvalue(fld, cs)

	return rud

end


--------------------------------------------------------------------------------------
-- printPacket  -
--
--   Notes:
--------------------------------------------------------------------------------------
local function printPacket(pkt,align)

    if not pkt then return end

    local lc = 0

	num_bytes_to_align = align or 16

    bytes = {string.byte(pkt, 0, -1)}
    for i=1,#bytes do
        io.write(string.format('%02X', bytes[i]))

        if lc == num_bytes_to_align then
            io.write("\n")
            lc = 0
        end
        lc = lc + 1
    end
    io.write("\n")

end

--------------------------------------------------------------------------------------
-- comparePacket  -
--
--   Notes:
--------------------------------------------------------------------------------------
local function comparePacket(pkt1, pkt2)

    if not pkt1 or not pkt2 then return false end

    bytes1 = {string.byte(pkt1, 0, -1)}
    bytes2 = {string.byte(pkt2, 0, -1)}

    if #bytes1 ~= #bytes2 then
        return false
    end

    for i=1,#bytes1 do
        if bytes1[i] ~= bytes2[i] then
            return false
        end
    end

    return true

end


--------------------------------------------------------------------------------------
-- sendCommand  -
--
--   Example: sendCommand("/TOEnable dest=127.0.0.1", UplinkQ, "cs", false)
--   Note 1: the presence of the "cs" string says to populate the checksum at field "cs"
--   Note 2: the UplinkQ must be a msgq that is already created
--------------------------------------------------------------------------------------
local function sendCommand (cmdstr, q, cs, echo)

    local ccsds_cmd = msg.create(cmdstr)
    if cs then ccsds_cmd = setChecksum(ccsds_cmd, cs) end
    if echo then printPacket(ccsds_cmd:serialize(), 32) end
    q:sendrecord(ccsds_cmd)

end

--------------------------------------------------------------------------------------
-- Return Local Package
--------------------------------------------------------------------------------------
local package = {
    computeChecksum = computeChecksum,
    setChecksum = setChecksum,
    printPacket = printPacket,
    comparePacket = comparePacket,
    sendCommand = sendCommand
}

return package
