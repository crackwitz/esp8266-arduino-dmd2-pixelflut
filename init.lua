m = mqtt.Client("cracki-esp-devboard", 120, nil, nil)

basetopic = "cracki/esp-devboard"


function string.startswith(self, prefix)
	return string.sub(self, 1, string.len(prefix)) == prefix
end


function mqtt_onmessage(client, topic, message)
	-- if not topic:startswith(basetopic) then return end

	--print(topic .. " " .. message)
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
			synctime = secs + usecs*1e-6
			m:publish(basetopic .. "/started", synctime, 0, 1)
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

function timerfunc()
	s, us = rtctime.get()
	msg = string.format("%d.%06d", s, us)
	m:publish(basetopic.."/unixtime", msg, 0, 0)
end

wlan_init()
dofile("telnet.lua")
tmr.register(0, 5000, tmr.ALARM_AUTO, timerfunc)

-- all outputs
dmd_a = 1 -- D1 -- init low
dmd_b = 2 -- D2 -- init low
dmd_oe = 3 -- D3, needs to support pwm -- init low
dmd_sck = 5-- D5, init low, spi clock
dmd_sclk = 0 -- init low, "store clock" (chip select), active high, must not be controlled by SPI, but custom
dmd_r_data = 7-- init high (dark), mosi


--gpio.mode(3, gpio.OUTPUT)
--gpio.write(3, gpio.HIGH)

--pwm.setup(dmd_oe, 100, 10)
--pwm.start(dmd_oe)

gpio.mode(dmd_a, gpio.OUTPUT)
gpio.write(dmd_a, gpio.LOW)
gpio.mode(dmd_b, gpio.OUTPUT)
gpio.write(dmd_b, gpio.LOW)
gpio.mode(dmd_oe, gpio.OUTPUT)
gpio.write(dmd_oe, gpio.LOW)
gpio.mode(dmd_sclk, gpio.OUTPUT)
gpio.write(dmd_sclk, gpio.LOW)

-- spi id: 1 = HSPI
-- bits: lit = low, dark = high
-- msb first
-- cpol 0, cpha 0
-- 4 MHz recommended (clockdiv 20, 80 mhz base), 8 is pushing it
spi.setup(1, spi.MASTER, spi.CPOL_LOW, spi.CPHA_LOW, 8, 10, spi.HALFDUPLEX)


width_pixels = 64
height_pixels = 16

width_bytes = math.floor((width_pixels+7) / 8)
height_rows = height_pixels

-- bytes that can be shifted out as is
-- indexed by AB, then a vector
canvas = {[0]={}, [1]={}, [2]={}, [3]={}}
for AB=0,3 do
	for k = 0, width_bytes * height_rows / 4 - 1 do
		canvas[AB][k] = 0xFF
	end
end

function get_xy_index(x, y)
	local index   = 0

	local row = math.floor(y / 4)
	index = index + row

	local xbyte = math.floor(x / 8)
	index = index + (width_bytes - 1 - xbyte) * 4

	local scanrow = y % 4

	local offset  = x % 8

	return scanrow, index, offset
end

print(get_xy_index(63, 0))
print(get_xy_index(63, 4))
print(get_xy_index(63, 8))
print(get_xy_index(63,12))
print(get_xy_index(55, 0))
print(get_xy_index(63, 1))

for x = 0,7 do
	for y = 0,15 do
		local r,i,b = get_xy_index(x*8,y)
		canvas[r][i] = bit.bxor(0xFF, bit.bor(x, 16*y))
	end
end

--	-- bits here
--	canvas = {}
--	for i=0, height_rows-1 do
--		canvas[i] = {}
--		for j=0, width_bytes-1 do
--			canvas[i][j] = bit.bxor(0xFF, bit.bor(i, j*16))
--		end
--	end

function writeSPIData(canvas, scanrow)
	--	for j = width_bytes-1, 0, -1 do
	--		spi.set_mosi(1, offset, 8,
	--			canvas[scanrow+ 0][j],
	--			canvas[scanrow+ 4][j],
	--			canvas[scanrow+ 8][j],
	--			canvas[scanrow+12][j])
	--		offset = offset + 32
	--	end

	--spi.send(1, canvas[scanrow])
end

pwm.setup(dmd_oe, 1000, 100) -- 100 Hz, 10/1024 duty cycle

scanrow = 0

function scandisplay()
	--writeSPIData(canvas, scanrow) -- fixme: this seems to toggle something for output

	--spi.send(1, canvas[scanrow])

	spi.set_mosi(1, 0, 8, unpack(canvas[scanrow]))
	spi.transaction(1, 0, 0, 0, 0, #canvas[scanrow] * 8, 0, 0)

	-- disable output
	gpio.write(dmd_oe, gpio.LOW)
	pwm.stop(dmd_oe)

	-- store register
	gpio.write(dmd_sclk, gpio.HIGH)
	gpio.write(dmd_sclk, gpio.LOW)

	-- select output
	gpio.write(dmd_a, bit.isset(scanrow, 0) and 0 or 1)
	gpio.write(dmd_b, bit.isset(scanrow, 1) and 0 or 1)

	--print("scan row " .. scanrow)

	-- and enable output (PWM)
	-- PWM will be stopped at next timer cycle

	pwm.start(dmd_oe)
	--gpio.write(dmd_oe, gpio.HIGH) -- or pwm

	scanrow = (scanrow + 1) % 4
end

tmr.alarm(1, 5, tmr.ALARM_AUTO, scandisplay)

tmr.alarm(2, 2000, tmr.ALARM_SINGLE, function() pwm.stop(dmd_oe) tmr.stop(1) end)
