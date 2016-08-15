local appRootDir = nil
if not appRootDir then
	local appRootDir = ngx.var.APP_ROOT
	if not appRootDir then
		ngx.say("APP_ROOT is not definde in app nginx conf")
		ngx.eof()
	end
	
	if package.path and #package.path > 0 then
		package.path = package.path .. ';' .. appRootDir .. '/?.lua'
	else
		package.path = appRootDir .. '/?.lua'
	end

	require("public.index")()
end