local cairo = require('reeme.cairo.cairo')
local bitmap = require('reeme.cairo.bitmap')

local function makePNG(w, h, func)
	local bmp = bitmap.new(w, h, 'bgra8')
	local sr = cairo.image_surface(bmp)
	local cr = sr:context()

	assert(type(func) == 'function')

	sr:check()
	func(cr, sr, cairo)
	sr:check()
	cr:free()
	sr:flush()
	local dt = sr:save_png_string()
	sr:finish()
	sr:check()
	sr:free()

	return dt
end

local function makeJPG(w, h, quality, func)
	local bmp = bitmap.new(w, h, 'bgrx8')
	local sr = cairo.image_surface(bmp)
	local cr = sr:context()
	local q = nil

	if not func and type(quality) == 'function' then
		func = quality
	elseif type(quality) == 'number' then
		q = quality
	end

	assert(type(func) == 'function')

	sr:check()
	func(cr, sr, cairo)
	sr:check()
	cr:free()
	sr:flush()
	local dt = sr:save_jpg_string(q)
	sr:finish()
	sr:check()
	sr:free()

	return dt
end

return {
	makePNG = makePNG,
	makeJPG = makeJPG
}