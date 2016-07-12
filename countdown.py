import os
import sys
import time
import math
import datetime
import dateutil.parser
import paho.mqtt.client as mqtt

endzeit = dateutil.parser.parse(sys.argv[1])

client = mqtt.Client()
client.connect("mqtt", 1883, 60)
client.publish('cracki/esp-devboard/solid', "0")

print repr(endzeit)

sched = math.ceil(time.time())

while True:
	dt = sched - time.time()
	if dt > 0:
		time.sleep(dt)
	sched += 1

	now = datetime.datetime.now()
	delta = endzeit - now
	delta = delta.total_seconds()

	if delta < 0: break

	hours, delta = divmod(delta, 3600)
	minutes, seconds = divmod(delta, 60)

	formatted = "{:02.0f}:{:02.0f}:{:02.0f} ".format(hours, minutes, seconds)
	client.publish('cracki/esp-devboard/text', formatted)
