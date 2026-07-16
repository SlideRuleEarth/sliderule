local M = {}

function M.load(filename)
    local f = io.open(filename, "r")
    local contents

    if f then
        contents = f:read("*all")
        f:close()
    else
        local gzfile = filename .. ".gz"
        local gzcheck = io.open(gzfile, "r")
        assert(gzcheck, "Neither file nor gzip found: " .. gzfile)
        gzcheck:close()

        local status = os.execute(string.format("gzip -d -k '%s'", gzfile))
        assert(status == true or status == 0, "Failed to decompress " .. gzfile)

        f = io.open(filename, "r")
        assert(f, "Failed to open decompressed file " .. filename)
        contents = f:read("*all")
        f:close()
    end

    return contents
end

return M
