--
-- ENDPOINT:    /source/cre
--
-- INPUT:       arg[1]
--              {
--                  "parms":
--                  {
--                      "image":  "<container image>",
--                      "timeout": <timeout in seconds>,
--                      "userdata": "<string passed to running container>"
--                  }
--              }
--
-- OUTPUT:      <output from container>
--

local json  = require("json")
local rqst  = json.decode(arg[1])
local parms = rqst["parms"]

-- initialize result
local result = "{}"
local timeout = parms["node-timeout"] or parms["timeout"] or netsvc.NODE_TIMEOUT
local duration = 0
local interval = 10 < timeout and 10 or timeout -- seconds
local cre_runner = nil

-- get settings
local shared_directory, input_ctrl_filename, output_ctrl_filename = cre.settings()
local unique_shared_directory = string.format("%s/%s", shared_directory, rqstid)
local input_ctrl_file = string.format("%s/%s", unique_shared_directory, input_ctrl_filename)
local output_ctrl_file = string.format("%s/%s", unique_shared_directory, output_ctrl_filename)

-- create unique shared directory
if not cre.createunique(unique_shared_directory) then goto cleanup end

-- write input file
do
    local input_file = io.open(input_ctrl_file, "w")
    if not input_file then goto cleanup end
    local input_parms = { rqst_parms=parms, input_files={} }
    input_file:write(json.encode(input_parms))
    input_file:close()
end

-- execute container
do
    local cre_parms = cre.parms(parms)
    cre_runner = cre.container(cre_parms, unique_shared_directory)
end

-- read output file
do
    local output_file = io.open(output_ctrl_file, "r")
    if not output_file then goto cleanup end
    result = json.decode(output_file:read())
    output_file:close()
end

-- clean up --
::cleanup::

-- Wait Until Container Completion --
while not cre_runner:waiton(interval * 1000) do
    duration = duration + interval
    -- Check for Timeout --
    if timeout >= 0 and duration >= timeout then
        print(string.format("container <%s> request timed-out after %d seconds", parms["image"], duration))
        do return false end
    end
    print(string.format("container <%s> continuing to run (after %d seconds)", parms["image"], duration))
end

-- delete unique shared directory
cre.deleteunique(unique_shared_directory)

return result
