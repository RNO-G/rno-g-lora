/** 
 *  This process communicates with mosquitto, sending unsent messages from the run database. 
 *
 *  Cosmin Deaconu <cozzyd@kicp.uchicago.edu> 
 */


#include <mosquitto.h> 
#include "rno-g-control.h" 
#include <iostream> 
#include <string> 
#include "boost/format.hpp" 
#include "boost/json.hpp" 
#include "boost/beast/core/detail/base64.hpp"
#include <csignal> 
#include <arpa/inet.h> 


//postgreqsql
#include "libpq-fe.h" 



int app_number = 2; 
int mosquitto_port = 1883; 
const char * host = "localhost"; 
int keep_alive = 60; 
bool clean_sesh = true; 
bool debug_out = false; 
const char * pg_conn_info = "dbname=rno_g_lora"; 
const char * insert_stmt_name = "insert_json_payload"; 
const char * insert_stmt = 
                   "INSERT INTO inbox (source_id,source_name, msg_type, rcv_time, fcnt, freq, rssi, confirmed, msg_raw, msg_payload) "
                   "VALUES ($1::integer, $2, $3::integer, $4, $5::integer, $6::integer, $7::integer, $8::boolean, $9::bytea, $10)"; 
volatile int time_to_quit = 0; 
PGconn * db = 0; 
bool nodb=false; 
bool verbose=true; 


const char * get_name (int station)
{
  switch (station)
  {
    case 11: 
      return "Nanoq"; 
    case 12: 
      return "Terianniaq"; 
    case 13: 
      return "Ukaleq"; 
    case 14: 
      return "Tuttu"; 
    case 15: 
      return "Ummimak"; 
    case 16: 
      return "Aarluk"; 
    case 17: 
      return "Ussuk"; 
    case 21: 
      return "Amaroq"; 
    case 22: 
      return "Avinngaq"; 
    case 23: 
      return "Ukaliatsiaq"; 
    case 24: 
      return "Qappik"; 
    case 25: 
      return "Aataaq"; 
    case 26: 
      return "Aaveq"; 
    case 27: 
      return "Egalussuaq"; 
    case 33: 
      return "Ippernaq"; 
    case 34: 
      return "Isunngaq"; 
    case 35: 
      return "Natseq"; 
    case 36: 
      return "Niisa"; 
    case 37: 
      return "Uppik"; 
    case 44: 
      return "Nattoralik"; 
    case 45: 
      return "Qilanngaq"; 
    case 46: 
      return "Aqisseq"; 
    case 47: 
      return "Natsersuaq"; 
    case 54: 
      return "Qippoqaq"; 
    case 55: 
      return "Arfiviit"; 
    case 56: 
      return "Alleq"; 
    case 57: 
      return "Qarsaaq"; 
    case 64:
      return "Tikaaquillik"; 
    case 65:
      return "Kapisillik"; 
    case 66:
      return "Eqaluk"; 
    case 67:
      return "Qeeraq"; 
    case 74:
      return "Qaleralik"; 
    case 75:
      return "Uugaq"; 
    case 76:
      return "Amiqok"; 
    case 77:
      return "Nerleq"; 
    default: 
      return "Unknown" ; 
  }

  return 0; 
}

const char * get_mode (const rno_g_report & report) 
{
  switch(report.mode) 
  {
    case RNO_G_NORMAL_MODE:
      return "NORMAL" ;
    case RNO_G_SBC_ONLY_MODE: 
      return "SBC_ONLY" ; 
    case RNO_G_SBC_OFF_MODE: 
      return "SBC_OFF"; 
    case RNO_G_LOW_POWER_MODE: 
     return  "LOW_POWER";
    case RNO_G_INIT :
      return "INITIALIZING" ; 
    default: 
      return  "UNKNOWN"; 
  }
  return 0; 
}

const char * get_lte_state (const rno_g_report & report) 
{
  switch(report.lte_state) 
  {
    case LTE_INIT:
      return "INIT" ;
    case LTE_OFF: 
      return "OFF" ; 
    case LTE_TURNING_ON: 
      return "TURNING_ON"; 
    case LTE_ON: 
     return  "ON";
    case LTE_TURNING_OFF :
      return "TURNING OFF" ; 
    default: 
      return  "UNKNOWN"; 
  }
  return 0; 
}

const char * get_sbc_state (const rno_g_report & report) 
{
  switch(report.sbc_state) 
  {
    case SBC_OFF: 
      return "OFF" ; 
    case SBC_TURNING_ON: 
      return "TURNING_ON"; 
    case SBC_ON: 
     return  "ON";
    case SBC_TURNING_OFF :
      return "TURNING OFF" ; 
    default: 
      return  "UNKNOWN"; 
  }
  return 0; 
}
const char * get_sbc_boot_mode (const rno_g_report & report) 
{
  switch(report.sbc_boot_mode) 
  {
    case SBC_BOOT_INTERNAL:
      return "INTERNAL" ;
    case SBC_BOOT_SDCARD: 
      return "SDCARD" ; 
    default: 
      return  "UNKNOWN"; 
  }
  return 0; 
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
    auto devEUI_b64 = json_payload.at("devEUI").as_string();
    auto msg_b64 = json_payload.at("data").as_string(); 
    int freq= json_payload.at("txInfo").at("frequency").as_int64(); 
    int rssi = json_payload.at("rxInfo").at(0).at("rssi").as_int64(); 
    auto timestr = json_payload.at("rxInfo").at(0).at("time").as_string(); 
    uint8_t confirmed = json_payload.at("confirmedUplink").as_bool(); 

    uint8_t devEUI[8] = {0}; 
    boost::beast::detail::base64::decode(devEUI, devEUI_b64.c_str(), devEUI_b64.size()); 
    int station = devEUI[7] + (devEUI[6] << 8); 

    if (verbose) 
    {
      std::cout << "From:  "  << device<< " ( " <<get_name(station) << " / " ;
      for (int i = 0; i < 8; i++) 
      {
          std::cout << std::hex << std::setfill('0') << std::setw(2) << (int) devEUI[i]; 
      }
      std::cout << std::dec<< " / " << station <<  "), port:" << port << " , count: " << count << "freq: " << freq  << "rssi: " << rssi << " dB " << std::endl; 
    }

    boost::json::object payload_json; 
    std::vector<uint8_t> bytes; 

    if (port == RNO_G_MSG_REPORT )  
    {
      rno_g_report report; 
      boost::beast::detail::base64::decode(&report, msg_b64.c_str(), msg_b64.size()); 
      bytes.resize(sizeof(report)); 
      memcpy(&bytes[0], &report, sizeof(report)); 
      
      boost::json::object report_json; 
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


    }

    else if (port == RNO_G_MSG_LTE_STATS) 
    {
      rno_g_lte_stats stats; 
      boost::beast::detail::base64::decode(&stats, msg_b64.c_str(), msg_b64.size()); 
      bytes.resize(sizeof(stats)); 
      memcpy(&bytes[0], &stats, sizeof(stats)); 

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
    }

   std::string payload_as_str = boost::json::serialize(payload_json); 

   if (verbose) 
   {
     std::cout << payload_as_str << std::endl; ; 
   }

   const char * dbvals[10]; 
   int lens[10] = {0}; 
   int binary[10] = {1,0,1,0,1,1,1,1,1,0}; 

   //network byte order
   int station_for_db = htonl(station); 
   dbvals[0] = (const char*)  &station_for_db; 
   lens[0] = sizeof(int);; 
   dbvals[1] = get_name(station); 
   int port_for_db = htonl(port);
   dbvals[2] = (const char*) &port_for_db; 
   lens[2] = 4; 
   dbvals[3] = timestr.c_str(); 
   int fcnt_for_db = htonl(count); 
   dbvals[4] = (const char*) &fcnt_for_db; 
   lens[4] = sizeof(int);
   int freq_for_db = htonl(freq);
   dbvals[5] =(const char*) &freq_for_db; 
   lens[5] = sizeof(int);
   int rssi_for_db = htonl(rssi);
   dbvals[6] =(const char*) &rssi_for_db; 
   lens[6] = sizeof(int);
   dbvals[7] = (const char*) &confirmed; 
   lens[7] = 1; 
   dbvals[8] = (const char*) &bytes[0]; 
   lens[8] = bytes.size(); 
   dbvals[9] = payload_as_str.c_str(); 

   //Insert into our database
   PGresult * res = PQexecPrepared(db, insert_stmt_name, 10,  dbvals, lens,binary,0); 
   if (PQresultStatus(res) != PGRES_COMMAND_OK) 
   {
      std::cerr << "exec failed " << PQresultStatus(res) << ": " << PQresultErrorMessage(res) << std::endl << PQerrorMessage(db) << std::endl; 
   }

   PQclear(res); 



  }
  catch (const std::exception & ex) 
  {
    std::cerr << ex.what() << std::endl; 
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


int main(int , char **) 
{

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

  if (mosquitto_connect(mosq, host, mosquitto_port, keep_alive))
  {
    std::cerr << "Unable to connect (host = " << host  << " , port = " << mosquitto_port << ")" <<  std::endl; 
  }

  signal(SIGINT, sigHandler); 


  //set up postgresql 
  
  db = PQconnectdb(pg_conn_info); 

  if (PQstatus(db) != CONNECTION_OK) 
  {
    time_to_quit = 1; 
    std::cerr << "Connection to database failed: " << PQerrorMessage(db) << std::endl; 
  }

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

  while (!time_to_quit) 
  {
    mosquitto_loop(mosq,1000, 1); 
  }


  PQfinish(db); 
  mosquitto_destroy(mosq); 
  mosquitto_lib_cleanup(); 

  return 0; 

}
