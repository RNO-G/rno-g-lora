#! /usr/bin/env python3 


APPLICATION_ID=2
PREFIX="c01daf" 
FORMAT="%010x" 
CONFIRMED= "true"
PORT=7

import paho.mqtt.publish as pub 
import base64
import sys 
import struct 



payload="" 

if len(sys.argv) < 4: 
    print("./set_batt_voltages.py device low high") 
    sys.exit(1) 


device = int(sys.argv[1])
low = float(sys.argv[2])
high = float(sys.argv[3])

devstring = PREFIX + (FORMAT % (device)) 

b = struct.pack("<ff",low,high); 
 
payload = base64.b64encode(b).decode()

msg = (
    "{" 
    f" \"confirmed\": {CONFIRMED}, "
    f" \"fPort\": {PORT}, "
    f" \"data\": \"{payload}\" "
    "}" 
    )

topic = f"application/{APPLICATION_ID}/device/{devstring}/command/down"
print(topic) 
print(msg) 
#pub.single(topic,msg,hostname="localhost") 







    







