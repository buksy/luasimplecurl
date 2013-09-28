local simplehttp = require ("simplehttp")

local json = {}
json['int'] = 1
json['string'] = "str"
json['double'] = 100.1
local arr = {};
arr[1] = 1
arr[2] = 2
arr[3] = 3
json['array'] = {1,2,3}
local arr1 = {}
arr1[1] = {a="b",b="c"}
arr1[2] = {e="f",g="h"}
json['map_array'] = {{a="b",c="d"},{e="f",g="h"}}
json['boolean'] = true
--print (simplehttp.tableToJSON(json))

local json_str = simplehttp.tableToJSON(json)
print (json_str)
local out = simplehttp.JSONToTable(json_str)
print (out['int']);
print (out['boolean']); 
print (out['string']);
print (out['double']);
local a = out['array']
print (a[1]..a[2]..a[3])
local marray = out['map_array']
for key_name, val in pairs(marray[1]) do
        print (key_name .. val)
end
for key_name, val in pairs(marray[2]) do
        print (key_name .. val)
end

