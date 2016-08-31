return function(uri)
    local match = { }
	
    if uri == '/' then
        return 'index', 'index'
    end
	
	local segs = string.split(uri, './')
	
    if #segs == 1 then
        return segs[1], 'index'
    else
        return string.lower(table.concat(segs, '.', 1, #segs - 1)), string.lower(segs[#segs])
    end
end