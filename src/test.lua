simplehttp = require "simplehttp"

local def = {url='', ssl_verify=false}

http = simplehttp.newconnect(def)
--[[print (getmetatable(http))
local meta = getmetatable(http)
for k,v in pairs(meta) do
  print("    ", k, v)
end
]]
local a = ""
	if (http:get (function (val)
	--	print ("In lua callback")
		a = a .. tostring(val)
		-- print (tostring(val))
		return true
		end)) then 
	print (a)
else 
	print (http:getLastError())
end
local headers = http:getResponseHeaders()
for key_name, val in pairs(headers) do
	print (key_name .. val)
end
http:disconnect()
--http:setBasicAuth ("", "")
