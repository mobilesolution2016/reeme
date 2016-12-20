local IndexController = {
	__index = {
		indexAction = function(self)
			return 'hello reeme'
		end,
	}
}

return function(act)
	return IndexController
end