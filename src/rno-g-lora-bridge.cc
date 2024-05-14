/** 
 *  This process communicates with mosquitto, sending unsent messages from the run database. 
 *
 *  Cosmin Deaconu <cozzyd@kicp.uchicago.edu> 
 */


#include <mosquitto.h> 
#include <iostream> 
#include <string> 
#include "boost/format.hpp" 
#include "boost/json.hpp" 
#include "boost/beast/core/detail/base64.hpp"
#include <csignal> 
#include <arpa/inet.h> 
#include "rno-g-lora-common.h"
#include "CLI11.hpp" 


//postgreqsql
#include "libpq-fe.h" 



bool eui_b64 = false; 
int app_number = 2; 
int mosquitto_port = 1883; 
std::string host = "localhost"; 
std::string conn_info = DEFAULT_PG_CONN_INFO;
int keep_alive = 60; 
bool clean_sesh = true; 
bool debug_out = false; 
const char * insert_stmt_name = "insert_json_payload"; 
const char * insert_stmt = 
                   "INSERT INTO inbox (source_id,daqbox, source_name, msg_type, rcv_time, fcnt, freq, rssi, confirmed, msg_raw, msg_payload) "
                   "VALUES ($1::integer,$2::integer, $3, $4::integer, $5, $6::integer, $7::integer, $8::integer, $9::boolean, $10::bytea, $11)"; 
volatile int time_to_quit = 0; 
PGconn * db = 0; 
bool nodb=false; 
bool verbose=true; 


template<typename... Args> 
size_t  string_fmt(std::string & s, const char * fmt, Args... args)
{
  size_t len = snprintf(nullptr,0, fmt, args...); 
  s.reserve(len+1); 
  s.resize(len); 
  snprintf(&s[0], len+1, fmt, args...); 
  return len; 
}



boost::json::stream_parser json_parse; 
void message_received(mosquitto * , void * , const mosquitto_message *msg)
{

  json_parse.reset(); 
  json_parse.write((const char*) msg->payload, msg->payloadlen); 


  if (!json_parse.done())
  {
    std::cerr << "Trouble reading msg " << msg->mid << " topic: " << msg->topic << std::endl; 
    return; 
  }


  boost::json::value json_payload = json_parse.release(); 

  try
  {

    //figure out what type of packet we're dealing with 

    int port = json_payload.at("fPort").as_int64(); 
    int count = json_payload.at("fCnt").as_int64(); 
    auto device = json_payload.at("deviceName").as_string(); 
    auto msg_b64 = json_payload.at("data") == nullptr ? "" : json_payload.at("data").as_string(); 
    int freq= json_payload.at("txInfo").at("frequency").as_int64(); 
    int rssi = json_payload.at("rxInfo").at(0).at("rssi").as_int64(); 
    auto timestr = json_payload.at("rxInfo").at(0).at("time").as_string(); 
    uint8_t confirmed = json_payload.get_object().count("confirmedUplink") ? json_payload.at("confirmedUplink").as_bool() : false; 


    int station; 
    boost::json::string devEUI; 

    if (eui_b64) 
    {
      auto devEUI_b64 = json_payload.at("devEUI").as_string();
      uint8_t devEUI_arr[8] = {0}; 
      boost::beast::detail::base64::decode(devEUI_arr, devEUI_b64.c_str(), devEUI_b64.size()); 
      station = devEUI_arr[7] + (devEUI_arr[6] << 8); 
      char devEUIbuf[17] = {0}; 
      sprintf(devEUIbuf,"%02x%02x%02x%02x%02x%02x%02x%02x",
          devEUI_arr[0], 
          devEUI_arr[1], 
          devEUI_arr[2], 
          devEUI_arr[3], 
          devEUI_arr[4], 
          devEUI_arr[5], 
          devEUI_arr[6], 
          devEUI_arr[7]); 

      devEUI = devEUIbuf; 
    }
    else
    {
      devEUI = json_payload.at("devEUI").as_string(); 
      station = strtoull(devEUI.c_str(),0,16) & 0xffff; 
    }

    if (verbose) 
    {
      std::cout << "From:  "  << device<< " ( " <<get_name(station) << " / " ;
      std::cout << devEUI << " / " << station <<  "), port:" << port << ", count: " << count << ", freq: " << freq  << ", rssi: " << rssi << " dB"  << std::endl; 
    }

    std::vector<uint8_t> bytes; 
    std::string json_as_str; 

    if (port == RNO_G_MSG_REPORT )  
    {
      rno_g_report report; 
      boost::beast::detail::base64::decode(&report, msg_b64.c_str(), msg_b64.size()); 
      bytes.resize(sizeof(report)); 
      memcpy(&bytes[0], &report, sizeof(report)); 
      
      boost::json::object payload_json; 
      payload_json ["when"] = report.when; 
      payload_json ["mode"] = get_mode(report); 
      payload_json ["lte_state"] = get_lte_state(report); 
      payload_json ["sbc_state"] = get_sbc_state(report); 
      payload_json ["sbc_boot_mode"] = get_sbc_boot_mode(report); 

      payload_json ["analog"] = { 
        {"when", report.analog_monitor.when}, 
        {"temp_C", report.analog_monitor.temp_cC/100.}, 
        {"I_surf3V_mA", { 
                       report.analog_monitor.i_surf3v[0], 
                       report.analog_monitor.i_surf3v[1], 
                       report.analog_monitor.i_surf3v[2], 
                       report.analog_monitor.i_surf3v[3], 
                       report.analog_monitor.i_surf3v[4], 
                       report.analog_monitor.i_surf3v[5] 
                     }
        }, 
        {"I_down3V_mA", { 
                       report.analog_monitor.i_down3v[0], 
                       report.analog_monitor.i_down3v[1], 
                       report.analog_monitor.i_down3v[2] 
                     }, 
   
        }, 
        {"I_sbc5V_mA" , report.analog_monitor.i_sbc5v }, 
        {"I_radiant_mA" , report.analog_monitor.i_5v[0] }, 
        {"I_lowthresh_mA" , report.analog_monitor.i_5v[1] } 
      }; 

      payload_json["power_monitor"] = { 
        {"when_power", report.power_monitor.when_power}, 
        {"PV_V", report.power_monitor.PVv_cV/100.}, 
        {"PV_I_mA", report.power_monitor.PVi_mA}, 
        {"BAT_V", report.power_monitor.BATv_cV/100.}, 
        {"BAT_I_mA", report.power_monitor.BATi_mA}, 
        {"when_temp", report.power_monitor.when_temp}, 
        {"local_T_C", report.power_monitor.local_T_C + report.power_monitor.local_T_sixteenth_C/16.}, 
        {"remote1_T_C", report.power_monitor.remote1_T_C + report.power_monitor.remote1_T_sixteenth_C/16.}, 
        {"remote2_T_C", report.power_monitor.remote2_T_C + report.power_monitor.remote2_T_sixteenth_C/16.} 
      }; 

      payload_json["power_state"] = { 
        {"low_power_mode", report.power_state.low_power_mode}, 
        {"sbc_power", report.power_state.sbc_power}, 
        {"lte_power", report.power_state.lte_power}, 
        {"radiant_power", report.power_state.radiant_power}, 
        {"lowthresh_power", report.power_state.lowthresh_power}, 
        {"dh_amp_power", { 
                           !!(report.power_state.dh_amp_power & (1 << 0)), 
                           !!(report.power_state.dh_amp_power & (1 << 1)), 
                           !!(report.power_state.dh_amp_power & (1 << 2)), 
                         } 
        }, 
        {"surf_amp_power", { 
                           !!(report.power_state.surf_amp_power & ( 1 << 0)), 
                           !!(report.power_state.surf_amp_power & ( 1 << 1)), 
                           !!(report.power_state.surf_amp_power & ( 1 << 2)), 
                           !!(report.power_state.surf_amp_power & ( 1 << 3)), 
                           !!(report.power_state.surf_amp_power & ( 1 << 4)), 
                           !!(report.power_state.surf_amp_power & ( 1 << 5)), 
                           }
        }
      }; 

      json_as_str = boost::json::serialize(payload_json); 


    }

    else if (port == RNO_G_MSG_LTE_STATS) 
    {
      rno_g_lte_stats_t stats; 
      boost::beast::detail::base64::decode(&stats, msg_b64.c_str(), msg_b64.size()); 
      bytes.resize(sizeof(stats)); 
      memcpy(&bytes[0], &stats, sizeof(stats)); 

      boost::json::object payload_json; 
      payload_json["when"] = stats.when;
      payload_json["mcc"] = stats.mcc;
      payload_json["mnc"] = stats.mnc;
      payload_json["earfcn"] = stats.earfcn;
      payload_json["rsrp"] = stats.rsrp;
      payload_json["rssi"] = stats.rssi;
      payload_json["tx_power"] = stats.tx_power;
      payload_json["rsrq"] = -stats.neg_rsrq_x10/10.;
      payload_json["band"] = stats.band;
      payload_json["service_domain"] = stats.service_domain;
      payload_json["n_parsed_ok"] = stats.parsed_ok;
      json_as_str = boost::json::serialize(payload_json); 
    }


    else if (port == RNO_G_MSG_LORA_STATS) 
    {
      rno_g_lora_stats_t stats; 
      boost::beast::detail::base64::decode(&stats, msg_b64.c_str(), msg_b64.size()); 
      bytes.resize(sizeof(stats)); 
      memcpy(&bytes[0], &stats, sizeof(stats)); 
      boost::json::object payload_json; 

      payload_json["when"] = stats.when;
      payload_json["uptime"] = stats.uptime;
      payload_json["rx"] = stats.rx;
      payload_json["tx"] = stats.tx;
      payload_json["rx_dropped"] = stats.rx_dropped;
      payload_json["tx_dropped"] = stats.tx_dropped;
      payload_json["last_recv"] = stats.last_recv; 
      payload_json["last_sent"] = stats.last_sent; 
      payload_json["join_time"] = stats.join_time; 
      payload_json["rssi"] = stats.rssi; 
      payload_json["snr"] = stats.snr; 
      json_as_str = boost::json::serialize(payload_json); 
    }

    else if (port == RNO_G_MSG_REPORT_V2)
    {
      rno_g_report_v2_t report; 
      boost::beast::detail::base64::decode(&report, msg_b64.c_str(), msg_b64.size()); 
      bytes.resize(sizeof(report)); 
      memcpy(&bytes[0], &report, sizeof(report)); 
      string_fmt(json_as_str, RNO_G_REPORT_V2_JSON_FMT, RNO_G_REPORT_V2_JSON_VALS((&report))); 
    }
    else if (port == RNO_G_MSG_REPORT_V3)
    {
      rno_g_report_v3_t report; 
      boost::beast::detail::base64::decode(&report, msg_b64.c_str(), msg_b64.size()); 
      bytes.resize(sizeof(report)); 
      memcpy(&bytes[0], &report, sizeof(report)); 
      string_fmt(json_as_str, RNO_G_REPORT_V3_JSON_FMT, RNO_G_REPORT_V3_JSON_VALS((&report))); 
    }



   if (verbose) 
   {
     std::cout << json_as_str << std::endl; ; 
   }

   if (!nodb) 
   {
     const char * dbvals[11]; 
     int lens[11] = {0}; 
     int binary[11] = {1,1,0,1,0,1,1,1,1,1,0}; 
     int daqbox = get_daqbox_from_station(station); 

     //network byte order
     int station_for_db = htonl(station); 
     dbvals[0] = (const char*)  &station_for_db; 
     lens[0] = sizeof(int);; 
     int daqbox_for_db = htonl(daqbox); 
     dbvals[1] = (const char*)  &daqbox_for_db; 
     lens[1] = sizeof(int);; 
     dbvals[2] = get_name(station); 
     int port_for_db = htonl(port);
     dbvals[3] = (const char*) &port_for_db; 
     lens[3] = 4; 
     dbvals[4] = timestr.c_str(); 
     int fcnt_for_db = htonl(count); 
     dbvals[5] = (const char*) &fcnt_for_db; 
     lens[5] = sizeof(int);
     int freq_for_db = htonl(freq);
     dbvals[6] =(const char*) &freq_for_db; 
     lens[6] = sizeof(int);
     int rssi_for_db = htonl(rssi);
     dbvals[7] =(const char*) &rssi_for_db; 
     lens[7] = sizeof(int);
     dbvals[8] = (const char*) &confirmed; 
     lens[8] = 1; 
     dbvals[9] = (const char*) &bytes[0]; 
     lens[9] = bytes.size(); 
     dbvals[10] = json_as_str.c_str(); 

     //Insert into our database
     PGresult * res = PQexecPrepared(db, insert_stmt_name, 11,  dbvals, lens,binary,0); 
     if (PQresultStatus(res) != PGRES_COMMAND_OK) 
     {
        std::cerr << "exec failed " << PQresultStatus(res) << ": " << PQresultErrorMessage(res) << std::endl << PQerrorMessage(db) << std::endl; 
     }

     PQclear(res); 
   }
  }
  catch (const std::exception & ex) 
  {
    std::cerr << "EXCEPTION:" << ex.what() << std::endl; 
    std::cerr <<  "=========== START JSON PAYLOAD===================" << std::endl;
    std::cerr <<  json_payload << std::endl;
    std::cerr <<  "===========END JSON PAYLOAD===================" << std::endl;

  }

}

void do_log(mosquitto* , void * , int level, const char * str) 
{
  if (level == MOSQ_LOG_WARNING || level == MOSQ_LOG_ERR) 
  {
    std::cerr << str << std::endl; 
  }
  else if (level == MOSQ_LOG_INFO || level == MOSQ_LOG_NOTICE || debug_out)  
  {
    std::cout  << str << std::endl; 
  }
}

void do_connected(mosquitto * mosq, void * , int result)  
{

  if (!result) 
  {
    //successful connection, let's subscribe to uplinks! 
    boost::format fmt= boost::format{"application/%d/device/+/event/up"} % app_number; 
    mosquitto_subscribe(mosq, NULL, fmt.str().c_str(), 2); 
  }
  else
  {
    std::cerr << "Failed to connect to mosquitto: result= " << result << std::endl; 
  }
}

void do_subscribe(mosquitto * , void *, int , int , const int * granted_qos)
{
  if (debug_out) 
  {
    std::cout << "Subscribed! qos=" << granted_qos[0] <<  std::endl; 
  }

}


void sigHandler(int signum) 
{
  std::cerr << "Got signal " << signum << std::endl; 
  time_to_quit = 1; 
}


int main(int argc, char ** argv) 
{

  CLI::App app{"RNO-G Lora Bridge"}; 
  app.add_option("-a,--application_id", app_number, "ChirpStack Application Number"); 
  app.add_option("-H,--host", host, "MQTT host"); 
  app.add_option("-p,--port", mosquitto_port, "MQTT port"); 
  app.add_option("-k,--keep-alive", keep_alive, "MQTT Keepalive"); 
  app.add_option("-d,--postgres-connection-info", conn_info, "PGSQL connection info (use dummy for no db)"); 
  app.add_option("-B,--base64-eui ", eui_b64, "EUI is base64"); 
  app.add_flag("-D,--debug",debug_out, "Enable debug output") ; 
  app.add_flag("-v,--verbose",verbose, "Enable verbose output") ; 
 
  CLI11_PARSE(app, argc, argv); 


  if (conn_info=="dummy")
  {
    nodb = true; 
  }
  else
  {
    std::cout << " Using conn_info: " << conn_info << std::endl; 

  }



  //set up mosquitto 
  if (mosquitto_lib_init()) 
  {
    std::cerr<< "Could not initialize libmosquitto" << std::endl; 
    return 1; 
  }


  mosquitto * mosq = mosquitto_new(NULL, clean_sesh, NULL); 

  if (!mosq) 
  {

    std::cerr << "Could not initialize mosquitto instance" << std::endl; 
  }


  mosquitto_log_callback_set(mosq, do_log); 
  mosquitto_connect_callback_set(mosq, do_connected); 
  mosquitto_message_callback_set(mosq, message_received); 
  mosquitto_subscribe_callback_set(mosq, do_subscribe); 

  if (mosquitto_connect(mosq, host.c_str(), mosquitto_port, keep_alive))
  {
    std::cerr << "Unable to connect (host = " << host  << " , port = " << mosquitto_port << ")" <<  std::endl; 
  }

  signal(SIGINT, sigHandler); 


  //set up postgresql 
  
  db = nodb ? NULL : PQconnectdb(conn_info.c_str()); 

  if (!nodb && PQstatus(db) != CONNECTION_OK) 
  {
    time_to_quit = 1; 
    std::cerr << "Connection to database failed: " << PQerrorMessage(db) << std::endl; 
  }

  if (!nodb) 
  {
    PGresult *res; 
    //hopefully we don't have to worry about malicious users, but... 

  //  PGresult * res = PQexec(db, "SELECT pg_catalog.set_config('search_path', '', false)"); 
  //
  //  if (PQresultStatus(res) != PGRES_TUPLES_OK) 
  //  {
  //    std::cerr << "SET failed: " << PQerrorMessage(db) << std::endl; 
  //    time_to_quit =1; 
  //  }
  //
  //  PQclear(res); 


    //prepare the insert statements
    res = PQprepare(db,  insert_stmt_name, insert_stmt,0,0); 
    if (PQresultStatus(res) != PGRES_COMMAND_OK) 
    {
      std::cerr << "PREPARE failed " << PQresultStatus(res) << ": " << PQresultErrorMessage(res) << std::endl << PQerrorMessage(db) << std::endl; 
      time_to_quit =1; 
    }



    PQclear(res); 
  }

  while (!time_to_quit) 
  {
    mosquitto_loop(mosq,1000, 1); 
  }


  if (!nodb) 
    PQfinish(db); 
  mosquitto_destroy(mosq); 
  mosquitto_lib_cleanup(); 

  return 0; 

}
