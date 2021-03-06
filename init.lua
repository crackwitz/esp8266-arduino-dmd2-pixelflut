-- todo: pixelflut tcp on 2342

-- tmr 0: periodic time pub
-- tmr 5: arduino reset timer

m = mqtt.Client("cracki-esp-devboard", 10, nil, nil)

basetopic = "cracki/esp-devboard"
debugtopic = "cracki/esp-debug"

function startswith(self, prefix)
	return string.sub(self, 1, string.len(prefix)) == prefix
end

function send_command(message)
	uart.write(0, string.char(#message) .. message)
end

function command_reset()
	gpio.write(reset_pin, gpio.LOW)
	tmr.alarm(5, 100, tmr.ALARM_SINGLE, function() 
		gpio.write(reset_pin, gpio.HIGH)
		gpio.mode(reset_pin, gpio.INPUT, gpio.PULLUP)
	end)
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
	send_command("T" .. string.char(channel) .. message .. "\x00")
end

function mqtt_onmessage(client, topic, message)
	if topic == "runlevel" then
		if message == "shutdown" then
			command_solid(0)

		elseif message == "launch" then
			command_gameoflife(1)

		end

	elseif topic == basetopic .. "/reset" then
		if message == "arduino" then
			command_reset()
		else
			node.restart()
		end

	elseif topic == basetopic .. "/bootload" then
		port = 4711
		bootload.start(4711, command_reset)

	elseif topic == basetopic .. "/telnet" then
		telnet.start()

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

	elseif topic == basetopic .. "/gameoflife" then
		command_gameoflife(tonumber(message))

	elseif topic == basetopic .. "/gameoflife/autonoise" then
		send_command("A" .. string.char(tonumber(message)))

	elseif topic == basetopic .. "/gameoflife/noise" then
		send_command("N" .. string.char(tonumber(message)))

	end
end

function mqtt_init()
	m:lwt(basetopic .. "/status", "offline", 0, 1)
	m:on("message", mqtt_onmessage)
	m:on("connect", function(client)
		tmr.alarm(0, 60e3, tmr.ALARM_AUTO, function() 
			local secs, usecs = rtctime.get()
			m:publish(basetopic .. "/time", string.format("%d.%06d", secs, usecs), 0, 0)
		end)

		--print("Subscribing")
		-- retained messages are only received for first subscribe() call
		m:subscribe {
			["runlevel"] = 0,
			[basetopic .. "/#"] = 0,
		}
		m:publish(basetopic .. "/status", "online", 0, 1)
		m:publish(basetopic .. "/ip", ip, 0, 1)

	end)
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
			--rtctime.set(secs, usecs)
		end
	)
	mqtt_init()
end

function wlan_init()
	wifi.setmode(wifi.STATION)
	wifi.sta.config("CCCAC_PSK_2.4GHz", "23cccac42")
	wifi.sta.eventMonReg(wifi.STA_GOTIP, wlan_gotip)
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
gpio.mode(reset_pin, gpio.OUTPUT, gpio.HIGH) -- to reset arduino
command_reset()

bootload = require "bootload"
telnet = require "telnet"
--handlers = require "handlers"
wlan_init()
--dofile("wifi.lua")
