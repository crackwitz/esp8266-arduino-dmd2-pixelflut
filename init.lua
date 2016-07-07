-- todo: pixelflut tcp on 2342

m = mqtt.Client("cracki-esp-devboard", 120, nil, nil)

basetopic = "cracki/esp-devboard"

function startswith(self, prefix)
	return string.sub(self, 1, string.len(prefix)) == prefix
end

function send_command(message)
	uart.write(0, string.char(#message) .. message)
end

function command_solid(value)
	-- 0: dark, 1: lit
	send_command("S" .. string.char(value))
end

function command_gameoflife(runval)
	-- enable/disable or 255=clear 254=fill
	send_command("G" .. string.char(runval))
end

function command_dutycycle(dc)
	send_command("D" .. string.char(dc))
end

function command_pixelflut(x, y, val)
	send_command("P" .. string.char(x, y, val))
end

function command_bitmap(message)
	--print("bitmap " .. #message)
	if #message == 64 * 16 / 8 then
		--command_pixelflut(0, 0, 1)
		send_command("B" .. message)
	end
end

function command_text(channel, message)
	send_command("T" .. string.char(channel) .. message)
end

function mqtt_onmessage(client, topic, message)
	if topic == "runlevel" then
		if message == "shutdown" then
			command_gameoflife(255)

		elseif message == "launch" then
			command_gameoflife(1)

		end

	elseif topic == basetopic .. "/dutycycle" then
		command_dutycycle(tonumber(message))

	elseif topic == basetopic .. "/pixelflut" then
		local _,_, sx, sy, sval = string.find(message, "^PX (%d+) (%d+) (%d+)$")
		-- print("match", sx, sy, sval)
		if sval ~= nil then
			local x = tonumber(sx)
			local y = tonumber(sy)
			local val = tonumber(sval)
			command_pixelflut(x, y, val)
		end

	elseif topic == basetopic .. "/bitmap" then
		command_bitmap(message)

	elseif topic == basetopic .. "/text" then
		command_text(0, message)

	elseif topic == basetopic .. "/text1" then
		command_text(1, message)

	elseif topic == basetopic .. "/text2" then
		command_text(2, message)

	elseif topic == basetopic .. "/solid" then
		command_solid(tonumber(message))
		command_gameoflife(255 - tonumber(message)) -- hotfix for stale arduino firmware

	elseif topic == basetopic .. "/gameoflife" then
		command_gameoflife(tonumber(message))

	elseif topic == basetopic .. "/gameoflife/autonoise" then
		send_command("A" .. string.char(tonumber(message)))

	elseif topic == basetopic .. "/gameoflife/noise" then
		send_command("N" .. string.char(tonumber(message)))

	end
end

function mqtt_init()
	m:on("connect", function(client)
		tmr.alarm(0, 10000, tmr.ALARM_AUTO, function() 
			local secs, usecs = rtctime.get()
			m:publish(basetopic .. "/time", string.format("%d.%06d", secs, usecs), 0, 1)
		end)

		--print("Subscribing")
		m:subscribe("runlevel", 0)
		m:subscribe(basetopic .. "/#", 0)
		m:publish(basetopic .. "/ip", ip, 0, 1)

	end)
	m:on("message", mqtt_onmessage)
	m:connect("mqtt.space.aachen.ccc.de")
end

function wlan_gotip()
	ip, mask, gateway = wifi.sta.getip()
	--print(string.format("IP:      %s", ip))
	--print(string.format("Mask:    %s", mask))
	--print(string.format("Gateway: %s", gateway))

	sntp.sync(
		"ptbtime1.ptb.de",
		function(secs, usecs, server)
			--print("Time Sync", secs, usecs, server)
			m:publish(basetopic .. "/started", string.format("%d.%06d", secs, usecs), 0, 1)
			rtctime.set(secs, usecs)
		end
	)

	mqtt_init()
end

function wlan_init()
	wifi.sta.eventMonReg(wifi.STA_GOTIP, wlan_gotip)
	wifi.setmode(wifi.STATION)
	wifi.sta.config("CCCAC_PSK_2.4GHz", "23cccac42")
	wifi.sta.eventMonStart()
end

--	function setup_pixelflut_tcp()
--		pixelflut_tcp_srv = net.createServer(net.TCP, 180)
--		pixelflut_tcp_srv:listen(2342, function(socket)
--		    socket:on("receive", function(c, l)
--		        
--		    end)
--		end)
--	end

reset_pin = 2
gpio.mode(reset_pin, gpio.OUTPUT, gpio.HIGH) -- reset arduino
gpio.write(reset_pin, gpio.LOW)
tmr.alarm(0, 100, tmr.ALARM_SINGLE, function() 
	gpio.write(reset_pin, gpio.HIGH)
	gpio.mode(reset_pin, gpio.INPUT, gpio.PULLUP)
end)

wlan_init()
dofile("telnet.lua")
