import struct
import os, sys
import time
import paho.mqtt.client as mqtt

delay = 0.01

if sys.argv[1:]:
	delay = float(sys.argv[1])

client = mqtt.Client()
client.connect("mqtt", 1883, 60)

#client.publish('cracki/esp-devboard/gameoflife', '254')

bitmap = bytearray([0] * 128)

for x in xrange(256):
	for k in xrange(128):
		bitmap[k] = (k + x) % 256
	client.publish('cracki/esp-devboard/bitmap', bitmap)
	time.sleep(delay)

if 0:
	for y in xrange(16):
		for x in xrange(64):
			client.publish('cracki/esp-devboard/pixelflut', 'PX {} {} 0'.format(x, y))
			time.sleep(delay)
