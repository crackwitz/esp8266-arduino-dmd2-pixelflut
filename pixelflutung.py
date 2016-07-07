import struct
import numpy as np
import os, sys
import time
import paho.mqtt.client as mqtt

delay = 0.01

if sys.argv[1:]:
	delay = float(sys.argv[1])

client = mqtt.Client()
client.connect("mqtt", 1883, 60)

#client.publish('cracki/esp-devboard/gameoflife', '254')

bitmap = np.zeros((16, 64), dtype=np.uint8)

def publish_bitmap():
	client.publish('cracki/esp-devboard/bitmap', bytearray(np.packbits(bitmap).tobytes()))

if 1:
	x = 0
	y = 0
	i = 0
	while True:
	    i += 1
	    if i % 2 == 0:
	            x += 1
	            bitmap[:] = 0
	            bitmap[:,x%64] = 1
	            publish_bitmap()
	    else:
	            y += 1
	            bitmap[:] = 0
	            bitmap[y%16,:] = 1
	            publish_bitmap()
	    time.sleep(delay)

if 0:
	for y in xrange(16):
		for x in xrange(64):
			client.publish('cracki/esp-devboard/pixelflut', 'PX {} {} 1'.format(x, y))
			time.sleep(delay)
