#ifndef rno_g_lora_common_h
#define rno_g_lora_common_h

#include "rno-g-control.h" 


inline int get_station_from_daqbox(int box) 
{
  switch(box) 
  {
    case 1: 
      return 23; 
    case 2: 
      return 13; 
    case 3: 
      return 24;
    case 4: 
      return 15;
    case 5: 
      return 23;
    case 6: 
      return 21; 
    case 7: 
      return 22; 
    case 8: 
      return 11; 
    case 9: 
      return 12; 
    case 10:
      return 14; 
    default: 
      return -1; 
  }
  return 0; 
}

inline int get_daqbox_from_station(int station) 
{
  switch(station) 
  {
    case 11: 
      return 8; 
    case 12: 
      return 9; 
    case 13: 
      return 2; 
    case 14: 
      return 10; 
    case 15: 
      return 4; 
    case 21: 
      return 6; 
    case 22: 
      return 7;
    case 23: 
      return 1; 
    case 24: 
      return 3; 
    case 25: 
      return 1; 
    default: 
      return 0; 
  }
  return -1; 
}



inline const char * get_name (int station)
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

inline const char * get_mode (const rno_g_report & report) 
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

inline const char * get_lte_state (const rno_g_report & report) 
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

inline const char * get_sbc_state (const rno_g_report & report) 
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
inline const char * get_sbc_boot_mode (const rno_g_report & report) 
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

#define DEFAULT_PG_CONN_INFO "dbname=rno_g_lora"

#endif 

