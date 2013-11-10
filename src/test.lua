simplehttp = require "simplehttp"
simplehttp1 = require "simplehttp"
simplehttp2 = require "simplehttp"
simplehttp3 = require "simplehttp"

local def = {url='http://cp.dev2.dev.flexiant.net', ssl_verify=false}
local def1 = {url='http://www.facebook.com', ssl_verify=false}

http = simplehttp.newconnect(def)
http1 = simplehttp1.newconnect(def1)

--[[print (getmetatable(http))
local meta = getmetatable(http)
for k,v in pairs(meta) do
  print("    ", k, v)
end
]]
local a = ""
local post = {}
post['hello'] = "world"
post['good'] = "boy"
post['test'] = "account"
if (http:post (post, function (val)
	--	print ("In lua callback")
		a = a .. tostring(val)
		-- print (tostring(val))
		return true
		end)) then 
	print (a)
else 
	print (http:getLastError())
end
--[[local headers = http:getResponseHeaders()
for key_name, val in pairs(headers) do
	print (key_name .. val)
end
http:disconnect()

local a1 = ""
        if (http1:get (function (val)
        --      print ("In lua callback")
                a1 = a1 .. tostring(val)
                -- print (tostring(val))
                return true
                end)) then
        print (a1)
else
        print (http1:getLastError())
end
local headers1 = http1:getResponseHeaders()
for key_name, val in pairs(headers1) do
        print (key_name .. val)
end
http1:disconnect()
]]
--http:setBasicAuth ("", "")
