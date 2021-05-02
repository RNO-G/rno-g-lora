#! /usr/bin/env python3 


APPLICATION_ID=2
PREFIX="c01daf" 
FORMAT="%010x" 
CONFIRMED= "true"
PORT=1

import paho.mqtt.publish as pub 
import base64
import sys 



payload="" 

infile = "/dev/stdin"  

if len(sys.argv) < 3: 
    print("./send_mode.py device modestring=['normal','lte-off','sbc-off']") 
    sys.exit(1) 


device = int(sys.argv[1])
devstring = PREFIX + (FORMAT % (device)) 
table = { 'normal':b'\x01', 'lte-off':b'\x02', 'sbc-off':b'\x03' } 

val = table[sys.argv[2]]
payload = base64.b64encode(val).decode()

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







    







