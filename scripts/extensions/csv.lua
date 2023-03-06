local pp = require("prettyprint")

local function get_next_row (contents, position)
    if position >= #contents then return end
    local i, new_position = contents:find("\n", position)
    if not i then i = #contents + 1 end
    new_position = i + 1
    local line = contents:sub(position, i - 1)
    local tokens = {}
    local offset = 0
    local j,_ = line:find(",", offset)
    while j and j > 0 do
        local token = line:sub(offset, j - 1)
        token = token:match("^%s*(.-)%s*$") -- trim white space
        table.insert(tokens, token)
        offset = j + 1
        j,_ = line:find(",", offset)
    end
    local token = line:sub(offset)
    token = token:match("^%s*(.-)%s*$") -- trim white spa
    table.insert(tokens, token)
    return tokens, new_position
end

local function open (filename)
  -- open file
  local f,err = io.open(filename)
	if not f then
		sys.log(core.CRITICAL, string.format("Unable to open csv file %s: %s", filename, tostring(err)))
		return
	end
  -- read file
	local contents = f:read("*a")
  -- close file
  f:close()
  -- get header line
  local header, position = get_next_row(contents, 0)
  -- populate table
  local data = {}
  while true do
      local fields, new_position = get_next_row(contents, position)
      position = new_position
      if not fields then break end
      local entry = {}
      for i,field in ipairs(fields) do
          if header and header[i] then
              entry[header[i]] = field
          end
      end
      table.insert(data, entry)
  end

  return data
end

local package = {
    open = open
}

return package

