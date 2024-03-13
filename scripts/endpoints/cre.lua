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
local result = ""

-- get settings
local shared_directory, input_ctrl_filename, output_ctrl_filename = cre.settings()
local unique_shared_directory = string.format("%s/%s", shared_directory, rqstid)
local input_ctrl_file = string.format("%s/%s", unique_shared_directory, input_ctrl_filename)
local output_ctrl_file = string.format("%s/%s", unique_shared_directory, output_ctrl_filename)
print(input_ctrl_file, output_ctrl_file)

-- create unique shared directory
if not cre.createunique(rqstid) then goto cleanup end

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
    local cre_runner = cre.container(cre_parms, unique_shared_directory)
end

-- read output file
do
    local output_file = io.open(output_ctrl_file, "r")
    if not output_file then goto cleanup end
    local output_parms = json.decode(output_file:read())
    output_file:close()
end

-- clean up --
::cleanup::
cre.deleteunique(rqstid)

return result
