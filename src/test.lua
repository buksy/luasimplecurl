simplehttp = require "simplehttp"

curl = simplehttp.newconnect("http://www.google.com")
print (getmetatable(curl))
local meta = getmetatable(curl)
for k,v in pairs(meta) do
  print("    ", k, v)
end
curl:get ()
curl:setBasicAuth ("", "")
