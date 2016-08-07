local bootload = {}

local server = nil

function bootload.close()
	if server ~= nil then
		server:close()
		server = nil
	end
end

function bootload.start(port, resetfunc)
	bootload.close()

	if port == 0 then return end

	server = net.createServer(net.TCP, 180)
	server:listen(port, function(socket)
		local fifo = {}
		local fifo_drained = true

		local function on_sent(c)
			if #fifo > 0 then
				c:send(table.remove(fifo, 1))
			else
				fifo_drained = true
			end
		end

		local function fifo_append(str)
			table.insert(fifo, str)
			if socket ~= nil and fifo_drained then
				fifo_drained = false
				on_sent(socket)
			end
		end

		-- new connection
		--bootload.close()

		--uart.setup(0, 57600, 8, uart.PARITY_NONE, uart.STOPBITS_1, 0)
		--resetfunc()

		socket:on("disconnection", function(socket) 
			--uart.setup(0, 115200, 8, uart.PARITY_NONE, uart.STOPBITS_1, 0)
		end)

		local outbuf = ""

		uart.on("data", 1, function(data)
			fifo_append(data)
		end, 0)

	    socket:on("receive", function(socket, data)
	        uart.write(0, data)
	    end)
	end)
end

return bootload
