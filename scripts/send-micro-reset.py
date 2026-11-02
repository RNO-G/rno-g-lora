#! /usr/bin/env python3 


APPLICATION_ID=2
PREFIX="c01daf" 
FORMAT="%010x" 
PORT=9

import paho.mqtt.publish as pub 
import base64
import sys 




infile = "/dev/stdin"  

if len(sys.argv) < 2: 
    print("./send_micro-rset.py device [boot=0]")
    sys.exit(1) 


device = int(sys.argv[1])
devstring = PREFIX + (FORMAT % (device))


boot = int (sys.argv[2]) if len(sys.argv) > 2 else 0
payload = base64.b64encode(bytes([0,boot])).decode()

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
pub.single(topic,msg,hostname="localhost") 








