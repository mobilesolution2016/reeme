local TestController = {
	__index = {
		indexAction = function(self)
			return 'test'
		end,
		
		doAction = function(self)
			return self.response:view('error', {
				message = 'test.do'
			})
		end,
	}
}

return function(act)
	return TestController
end