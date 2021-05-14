#! /bin/sh 

read -r -p "Are you sure? This will nuke the database (type \"DO IT LIVE!\" to confirm): " response
if [[ "$response" == "DO IT LIVE!" ]] 
then 

  echo "DOING IT LIVE! (goodbye data...)"

psql rno_g_lora << E_O_SQL

DROP TABLE if exists inbox; 
DROP TABLE if exists outbox; 


CREATE TABLE inbox ( 
msg_id SERIAL PRIMARY KEY, 
source_id  integer not null, 
daqbox  integer not null, 
source_name  char(15) not null, 
msg_type  integer not null, 
rcv_time  timestamp not null, 
fcnt  integer not null, 
freq  integer not null, 
rssi  integer not null, 
confirmed bool not null, 
msg_raw bytea, 
msg_payload jsonb 
); 

CREATE TABLE outbox (
msg_id SERIAL PRIMARY KEY, 
to_id integer not null, 
to_name char(15) not null, 
creation_time timestamp not null,
sent_time timestamp, 
sent bool, 
type integer not null , 
cmd_type integer not null, 
cmd_payload bytea, 
cmd_raw jsonb
); 

E_O_SQL

else
  echo "bwak bwak" 
fi 

