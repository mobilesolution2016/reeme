return function(uri)
    local match = { }
	
    if uri == '/' then
        return 'index', 'index'
    end
	
    for v in ngx.re.gmatch(uri , '/([A-Za-z0-9_]+)', "o") do
        match[#match + 1] = v[1]
    end
	
    if #match == 1 then
        return match[1], 'index'
    else
        return string.lower(table.concat(match, '.', 1, #match - 1)), string.lower(match[#match])
    end
end