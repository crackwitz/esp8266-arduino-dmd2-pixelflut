local handlers = {}

function handlers.Handler()
	local funcs = {}
	return {
		add = function(func)
			table.insert(funcs, func)
		end,
		call = function(...)
			for i,func in pairs(funcs) do
				func(unpack(arg))
			end
		end,
	}
end

return handlers
