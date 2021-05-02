#! /bin/sh

psql << E_O_SQL 

CREATE DATABASE rno_g_lora; 
CREATE USER lora; 
GRANT ALL PRIVILEGES ON DATABASE rno_g_lora TO lora; 

E_O_SQL 

