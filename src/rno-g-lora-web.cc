#include "crow_all.h" 
#include <iostream> 
#include <sstream> 
#include "libpq-fe.h" 
#include <endian.h> 
#include "rno-g-lora-common.h"
#include <ctime> 
#include <boost/tokenizer.hpp> 
#include <boost/format.hpp>



std::vector<int> stations; 

int port = 1777; 
std::string prefix = ""; 
std::string grafana_link = ""; 
std::string conn_info = DEFAULT_PG_CONN_INFO; 


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


int parse(int nargs, char ** args) 
{
  for (int i = 1; i < nargs; i++) 
  {
    if ( (!strcmp(args[i],"-p") || !strcmp(args[i],"--port")) && i < nargs-1)
    {
      int maybe_port = atoi(args[++i]); 
      if (maybe_port) 
      {
        port = maybe_port; 
      }
      else
      {
        return -1;
      }
    }

    else if ((!strcmp(args[i],"-P") || !strcmp(args[i],"--prefix")) && i < nargs-1)
    {
      prefix = args[++i]; 
    }

    else if ( (!strcmp(args[i],"-s") || !strcmp(args[i],"--station")) && i < nargs-1)
    {
      int maybe_a_station = atoi(args[++i]);
      if (maybe_a_station)
        stations.push_back(maybe_a_station); 
      else 
        return -1; 
    }

    else if ( (!strcmp(args[i],"-S") || !strcmp(args[i],"--stations")) && i < nargs -1)
    {
      std::string s = args[++i]; 
      boost::tokenizer<boost::char_separator<char>> tok{s,boost::char_separator<char>(",")}; 

      for (const auto & t : tok)
      {
        int maybe_a_station = atoi(t.c_str()); 

        if (maybe_a_station) 
        {
          stations.push_back(maybe_a_station); 
        }
        else 
          return -1; 
      }
    }
    else if ( (!strcmp(args[i],"-G") || !strcmp(args[i],"--grafana")) && i < nargs -1) 
    {
      grafana_link = args[++i]; 
    }
    else if ( (!strcmp(args[i],"-c") || !strcmp(args[i],"--credentials")) && i < nargs -1) 
    {
      conn_info = args[++i]; 
    }

  }
  return 0; 
}

void usage() 
{
  std::cout << " rno-g-lora-web [opts] " << std::endl; 
  std::cout << "    -h/--help    show this " << std::endl; 
  std::cout << "    -p/--port N  listening port (default 1777)" << std::endl; 
  std::cout << "    -P/--prefix serving prefix, if using an alias" << std::endl; 
  std::cout << "    -s/--station station_number  add station to station list, may be called multiple times" << std::endl; 
  std::cout << "    -S/--stations station_number1,station_number2,...  add stations to station list, may be called multiple times" << std::endl; 
  std::cout << "    -G/--grafana link to grafana (default is gethostbyname():3000)" << std::endl; 
  std::cout << "    -c/--credentials pg_conn_info" << std::endl; 
}

int main(int nargs, char ** args) 
{

  if (parse(nargs,args))
  {
    usage(); 
    return 1; 
  }

  if (!stations.size())
  {
    //add defaults...
    stations.push_back(21); 
    stations.push_back(11); 
    stations.push_back(22); 
    stations.push_back(12); 
  }


  std::cout << "Using stations"; 
  for (auto s : stations) 
    std::cout << s;
  std::cout << std::endl; 

  crow::SimpleApp app; 

  db = PQconnectdb(conn_info.c_str());  

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

    std::string grafana = grafana_link; 
    if (grafana=="") 
    {
      char hostname[80]; 
      gethostname(hostname,79); 
      grafana = "https://"; 
      grafana+=hostname; 
      grafana+=":3000"; 
    }


    std::string ret =  (boost::format("<html><head><title>LORA</title></head><body><h1>LORA Monitoring</h1><p>See also <a href='%1%'>grafana</a>.<p> <a href='%2%/report'>All Station Reports</a> | <a href='%2/lte'>All LTE Stats </a> | <a href='%2%/lora'>All LORA Stats</a> <hr>\n") % grafana_link % prefix ).str(); 
    ret+="<table border=1><tr><td>By Station:</td> \n";
    std::vector<int> last_heard(stations.size()); 
    int istation = 0;
    for (int station : stations) 
    {
      ret += (boost::format("<td> <a href='%1/station/") % prefix).str()  + std::to_string(station)+"'>" + std::to_string(station) + "</a> </td>\n"; 
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
    ret += "<p><a href='https://github.com/rno-g/rno-g-lora'>you can help make this less crappy</a></body></html>"; 
    return ret; 
  }); 

  CROW_ROUTE(app,"/lte")( []() 
  {
    PGresult *r = PQexecPrepared(db, "lte_stmt",0,0,0,0,0) ; 
    if (PQresultStatus(r) != PGRES_TUPLES_OK) 
    {
      return std::string("exec failed ") + std::to_string(PQresultStatus(r)) + std::string(": ") + std::string(PQresultErrorMessage(r)) + std::string("\n") + std::string(PQerrorMessage(db)); 
    }

    std::string ret = (boost::format("<html>\n<head>\n<title>LTE</title></head><body><h1>LTE STATS</h1><p><a href='%1/'>[back]</a>\n<hr>\n") % prefix).str();  
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

    std::string ret = (boost::format("<html>\n<head>\n<title>LORA</title></head><body><h1>LORA STATS</h1><p><a href='%1/'>[back]</a>\n<hr>\n") % prefix).str(); 
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

    std::string ret = (boost::format("<html>\n<head>\n<title>REPORT</title></head><body><h1>STATION REPORTS</h1><p><a href='%1/'>[back]</a>\n<hr>\n") % prefix).str(); 
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
    ret += (boost::format("</h1><p><a href='%1/'>[back]</a>\n<hr>\n") % prefix).str(); 
    ret += "<p>" + current_time(); 
    ret += make_table(r); 
    ret += "\n</body></html>"; 
    PQclear(r); 
    return ret; 
  }); 


  app.port(port).run(); 

  PQfinish(db); 

}
