#include "crow_all.h" 
#include <iostream> 
#include <sstream> 
#include "libpq-fe.h" 
#include <endian.h> 
#include "rno-g-lora-common.h"
#include <ctime> 




int stations[] = ENABLED_STATIONS; 
int n_stations = sizeof(stations)/sizeof(*stations); 
int daqboxes[] = ENABLED_DAQBOXES; 


std::string current_time(time_t *when = 0) 
{
  time_t now; 
  time(&now) ; 
  std::string ret = "current time: " ; 
  char buf[32]; 
  ret += ctime_r(&now,buf); 
  if (when) *when=now; 
  return ret; 
}

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

  res = PQprepare(db,"lastheard_stmt",
      "SELECT rcv_time FROM inbox WHERE source_id = $1::integer ORDER BY msg_id DESC LIMIT 1;" ,0,0); 


  if (PQresultStatus(res) != PGRES_COMMAND_OK) 
  {
    std::cerr << "PREPARE failed " << PQresultStatus(res) << ": " << PQresultErrorMessage(res) << std::endl << PQerrorMessage(db) << std::endl; 
    return 1; 
  }

  PQclear(res); 

  res = PQprepare(db,"lora_stmt",
      "SELECT msg_id, source_id,source_name, rcv_time, fcnt, freq, rssi, msg_payload from inbox WHERE msg_type = 3 ORDER BY msg_id DESC LIMIT 50;" ,0,0); 

  if (PQresultStatus(res) != PGRES_COMMAND_OK) 
  {
    std::cerr << "PREPARE failed " << PQresultStatus(res) << ": " << PQresultErrorMessage(res) << std::endl << PQerrorMessage(db) << std::endl; 
    return 1; 
  }
  PQclear(res); 

 

  CROW_ROUTE(app,"/")( []()
  {
    char hostname[512]; 
    gethostname(hostname,511); 
    std::string ret =  "<html><head><title>LORA</title></head><body><h1>LORA Monitoring</h1><p>See also <a href='https://192.168.99.50:3000'>grafana</a>.<p> <a href='/report'>All Station Reports</a> | <a href='/lte'>All LTE Stats </a> | <a href='/lora'>All LORA Stats</a> <hr>\n"; 
    ret+="<table border=1><tr><td>By Station:</td> \n";
    std::vector<int> last_heard(n_stations); 
    int istation = 0;
    for (int station : stations) 
    {
      ret += "<td> <a href='/station/" + std::to_string(station)+"'>" + std::to_string(station) + "</a> </td>\n"; 
      int station_for_db = htonl(station); 
      const char * dbvals[1]; 
      dbvals[0] = (const char*) &station_for_db; 
      int len = 4; 
      int binary = 1; 
      PGresult *r = PQexecPrepared(db, "lastheard_stmt",1,dbvals,&len,&binary,1) ; 
      struct timespec tnow; 
      clock_gettime(CLOCK_REALTIME,&tnow); 
      double now = tnow.tv_sec + tnow.tv_nsec*1e-9; 
      double when = 0; 
      if (PQresultStatus(r) != PGRES_TUPLES_OK) 
      {
        return std::string("exec failed ") + std::to_string(PQresultStatus(r)) + std::string(": ") + std::string(PQresultErrorMessage(r)) + std::string("\n") + std::string(PQerrorMessage(db)); 
      }
      else
      {
        uint64_t * binwhen = (uint64_t*) PQgetvalue(r,0,0); 


        //convert to unix time 
        when = be64toh(*binwhen)/1.e6 + 946684800; 
      }

      last_heard[istation++] = round(now-when); 
      PQclear(r); 

    }
    ret +="</tr><tr><td> Age of latest packet </td>\n"; 
    for (int last : last_heard) 
    {
      ret += "<td> " + std::to_string(last) + " s</td>\n"; 
    }

    ret += "</tr></table><p>\n"; 
    ret +="<table border=1><tr><td> By DAQBox: </td> \n"; 
    for (int daqbox : daqboxes) 
    {
     ret += "<td> <a href='/station/" + std::to_string(get_station_from_daqbox(daqbox))+"'>" + std::to_string(daqbox) + "</a> </td>\n"; 
    }

    ret += "</tr></table><p><a href='https://github.com/rno-g/rno-g-lora'>you can help make this less crappy</a></body></html>"; 
    return ret; 
  }); 

  CROW_ROUTE(app,"/lte")( []() 
  {
    PGresult *r = PQexecPrepared(db, "lte_stmt",0,0,0,0,0) ; 
    if (PQresultStatus(r) != PGRES_TUPLES_OK) 
    {
      return std::string("exec failed ") + std::to_string(PQresultStatus(r)) + std::string(": ") + std::string(PQresultErrorMessage(r)) + std::string("\n") + std::string(PQerrorMessage(db)); 
    }

    std::string ret = "<html>\n<head>\n<title>LTE</title></head><body><h1>LTE STATS</h1><p><a href='/'>[back]</a>\n<hr>\n"; 
    ret += "<p>" + current_time(); 
    ret += make_table(r); 
    ret += "\n</body></html>"; 
    PQclear(r); 
    return ret; 
  }); 

  CROW_ROUTE(app,"/lora")( []() 
  {
    PGresult *r = PQexecPrepared(db, "lora_stmt",0,0,0,0,0) ; 
    if (PQresultStatus(r) != PGRES_TUPLES_OK) 
    {
      return std::string("exec failed ") + std::to_string(PQresultStatus(r)) + std::string(": ") + std::string(PQresultErrorMessage(r)) + std::string("\n") + std::string(PQerrorMessage(db)); 
    }

    std::string ret = "<html>\n<head>\n<title>LORA</title></head><body><h1>LORA STATS</h1><p><a href='/'>[back]</a>\n<hr>\n"; 
    ret += "<p>" + current_time(); 
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
    ret += "<p>" + current_time(); 
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
    ret +=" </title></head><body><h1> STATION ";
    ret += std::to_string(station) + " (" + get_name(station) + ", DAQBox " + std::to_string(get_daqbox_from_station(station)) + ")"; 
    ret += "</h1><p><a href='/'>[back]</a>\n<hr>\n"; 
    ret += "<p>" + current_time(); 
    ret += make_table(r); 
    ret += "\n</body></html>"; 
    PQclear(r); 
    return ret; 
  }); 


  app.port(1777).run(); 

  PQfinish(db); 

}
