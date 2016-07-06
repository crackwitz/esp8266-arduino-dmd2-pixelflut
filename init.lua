m = mqtt.Client("cracki-esp-devboard", 120, nil, nil)

basetopic = "cracki/esp-devboard"

function startswith(self, prefix)
	return string.sub(self, 1, string.len(prefix)) == prefix
end


function mqtt_onmessage(client, topic, message)
	if topic == basetopic .. "/dutycycle" then
		uart.write(0, "D" .. string.char(tonumber(message)))

	elseif topic == basetopic .. "/pixelflut" then
		local _,_, sx, sy, sval = string.find(message, "^PX (%d+) (%d+) (%d+)$")
		-- print("match", sx, sy, sval)
		if sval ~= nil then
			local x = tonumber(sx)
			local y = tonumber(sy)
			local val = tonumber(sval)
			uart.write(0, "!" .. string.char(x, y, val))
		end

	elseif topic == basetopic .. "/bitmap" then
		if #message == 64 * 16 / 8 then
			uart.write(0, "B" .. message)
		end

	elseif topic == basetopic .. "/gameoflife" then
		-- enable/disable or 255=clear 254=fill
		uart.write(0, "G" .. string.char(tonumber(message)))

	elseif topic == basetopic .. "/gameoflife/autonoise" then
		uart.write(0, "A" .. string.char(tonumber(message)))

	elseif topic == basetopic .. "/gameoflife/noise" then
		uart.write(0, "N" .. string.char(tonumber(message)))

	end
end

function mqtt_init()
	m:on("connect", function(client)
		tmr.start(0)
		m:subscribe(basetopic .. "/#", 0)
		m:publish(basetopic .. "/ip", ip, 0, 1)
		print("subscribed")
	end)
	m:on("message", mqtt_onmessage)
	m:connect("mqtt.space.aachen.ccc.de")
end

function wlan_gotip()
	ip, mask, gateway = wifi.sta.getip()
	print(string.format("IP:      %s", ip))
	print(string.format("Mask:    %s", mask))
	print(string.format("Gateway: %s", gateway))

	sntp.sync(
		"ptbtime1.ptb.de",
		function(secs, usecs, server)
			print("Time Sync", secs, usecs, server)
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

wlan_init()
dofile("telnet.lua")
