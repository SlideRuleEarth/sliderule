--
-- ENDPOINT:    /source/cre
--
-- INPUT:       None
--
-- OUTPUT:      <list of running containers as json>
--

local http_code, size, response, status = cre.list()
print(status, http_code, size, response)
return response
