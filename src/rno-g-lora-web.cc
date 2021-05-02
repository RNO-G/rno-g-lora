#include "crow_all.h" 
#include <iostream> 
#include <sstream> 
#include "libpq-fe.h" 
#include <arpa/inet.h> 
#include "rno-g-lora-common.h"



const char * pg_conn_info = "dbname=rno_g_lora"; 


std::string make_table(PGresult * r) 
{
    int nrows = PQntuples(r); 
    int ncols = PQnfields(r);

    std::string ret = "<table border=1>\n\t<tr>\n"; 

    for (int j = 0; j < ncols; j++) 
    {
      ret += "\t\t<td><b>"; 
      ret += PQfname(r, j) ; 
      ret +="</b></td>\n"; 
    }
    ret += "\t</tr>\n"; 


    for (int i = 0; i < nrows; i++) 
    {
      ret += "\t<tr>\n";

      for (int j = 0; j < ncols; j++) 
      {
        ret += "\t\t<td>"; 
        ret += PQgetvalue(r, i,j) ; 
        ret += "</td>\n"; 
      }
      ret += "\t</tr>\n";
    }


    return ret; 

} 

PGconn * db = 0;

int main(int nargs, char ** args) 
{
  crow::SimpleApp app; 

  db = PQconnectdb(pg_conn_info);  

  if (PQstatus(db) != CONNECTION_OK) 
  {
    std::cerr << "Connection to database failed: " << PQerrorMessage(db) << std::endl; 
    return 1; 
  }

  PGresult * res = PQprepare(db,"lte_stmt",
      "SELECT msg_id, source_id,source_name, rcv_time, fcnt, freq, rssi, msg_payload from inbox WHERE msg_type = 2 ORDER BY msg_id DESC LIMIT 50;" ,0,0); 

  if (PQresultStatus(res) != PGRES_COMMAND_OK) 
  {
    std::cerr << "PREPARE failed " << PQresultStatus(res) << ": " << PQresultErrorMessage(res) << std::endl << PQerrorMessage(db) << std::endl; 
    return 1; 
  }

  PQclear(res); 

  res = PQprepare(db,"report_stmt",
      "SELECT msg_id, source_id,source_name, rcv_time, fcnt, freq, rssi, msg_payload from inbox WHERE msg_type = 1 ORDER BY msg_id DESC LIMIT 50;" ,0,0); 

  if (PQresultStatus(res) != PGRES_COMMAND_OK) 
  {
    std::cerr << "PREPARE failed " << PQresultStatus(res) << ": " << PQresultErrorMessage(res) << std::endl << PQerrorMessage(db) << std::endl; 
    return 1; 
  }

  PQclear(res); 

  res = PQprepare(db,"station_stmt",
      "SELECT msg_id, msg_type, rcv_time, fcnt, freq, rssi, msg_payload from inbox WHERE source_id = $1::integer ORDER BY msg_id DESC LIMIT 50;" ,0,0); 

  if (PQresultStatus(res) != PGRES_COMMAND_OK) 
  {
    std::cerr << "PREPARE failed " << PQresultStatus(res) << ": " << PQresultErrorMessage(res) << std::endl << PQerrorMessage(db) << std::endl; 
    return 1; 
  }

  PQclear(res); 



  CROW_ROUTE(app,"/")( []()
  {
    return "<html><head><title>LORA</title></head><body><h1>LORA Monitoring</h1><p> <a href='/report'>report</a> | <a href='/lte'>LTE </a><p> <a href='https://github.com/rno-g/rno-g-lora'>you can help make this less crappy</a></body></html>"; 
  }); 

  CROW_ROUTE(app,"/lte")( []() 
  {
    PGresult *r = PQexecPrepared(db, "lte_stmt",0,0,0,0,0) ; 
    if (PQresultStatus(r) != PGRES_TUPLES_OK) 
    {
      return std::string("exec failed ") + std::to_string(PQresultStatus(r)) + std::string(": ") + std::string(PQresultErrorMessage(r)) + std::string("\n") + std::string(PQerrorMessage(db)); 
    }

    std::string ret = "<html>\n<head>\n<title>LTE</title></head><body><h1>LTE STATS</h1><p><a href='/'>[back]</a>\n<hr>\n"; 
    ret += make_table(r); 
    ret += "\n</body></html>"; 
    PQclear(r); 
    return ret; 
  }); 

  CROW_ROUTE(app,"/report")( []() 
  {
    PGresult *r = PQexecPrepared(db, "report_stmt",0,0,0,0,0) ; 
    if (PQresultStatus(r) != PGRES_TUPLES_OK) 
    {
      return std::string("exec failed ") + std::to_string(PQresultStatus(r)) + std::string(": ") + std::string(PQresultErrorMessage(r)) + std::string("\n") + std::string(PQerrorMessage(db)); 
    }

    std::string ret = "<html>\n<head>\n<title>REPORT</title></head><body><h1>STATION REPORTS</h1><p><a href='/'>[back]</a>\n<hr>\n"; 
    ret += make_table(r); 
    ret += "\n</body></html>"; 
    PQclear(r); 
    return ret; 
  }); 

  CROW_ROUTE(app,"/station/<int>")( [](int station) 
  {

    int station_for_db = htonl(station); 
    const char * dbvals[1]; 
    dbvals[0] = (const char*) &station_for_db; 
    int len = 4; 
    int binary = 1; 
    PGresult *r = PQexecPrepared(db, "station_stmt",1,dbvals,&len,&binary,0) ; 
    if (PQresultStatus(r) != PGRES_TUPLES_OK) 
    {
      return std::string("exec failed ") + std::to_string(PQresultStatus(r)) + std::string(": ") + std::string(PQresultErrorMessage(r)) + std::string("\n") + std::string(PQerrorMessage(db)); 
    }

    std::string ret = "<html>\n<head>\n<title>STATION "; 
    ret += std::to_string(station); 
    ret +=" </title></head><body><h1> STATION";
    ret += std::to_string(station) + " (" + get_name(station) + ")"; 
    ret += "</h1><p><a href='/'>[back]</a>\n<hr>\n"; 
    ret += make_table(r); 
    ret += "\n</body></html>"; 
    PQclear(r); 
    return ret; 
  }); 


  app.port(1777).multithreaded().run(); 

  PQfinish(db); 

}
