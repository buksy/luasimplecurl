local simplehttp = require ("simplehttp")

local json = {}
json['int'] = 1
json['string'] = "str"
json['double'] = 100.1
local arr = {};
arr[1] = 1
arr[2] = 2
arr[3] = 3
json['arry'] = {1,2,3}
local arr1 = {}
arr1[1] = {a="b",b="c"}
arr1[2] = {e="f",g="h"}
json['map_array'] = {{a="b",c="d"},{e="f",g="h"}}
json['boolean'] = true
print (simplehttp.tableToJSON(json))
