#! /usr/bin/env python3 


APPLICATION_ID=1
PREFIX="c01daf" 
FORMAT="%010x" 
CONFIRMED= "true"
PORT=10

import paho.mqtt.publish as pub 
import base64
import sys 


device = 960 
devstring = PREFIX + (FORMAT % (device)) 

payload="" 

infile = "/dev/stdin"  

if len(sys.argv) > 1: 
    infile = sys.argv[1]

with open(infile,"rb") as f: 
  payload = base64.b64encode(f.read()).decode()


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







    







