/* out.cpp 
   CREATED Tue Jun 21 16:21:18 2016

   GENERATED BY the sm4ceps C++ Generator VERSION 0.50 (c) Tomas Prerovsky <tomas.prerovsky@gmail.com>, ALL RIGHTS RESERVED. 
   Requires C++1y compatible compiler (use --std=c++1y for g++) 
   BASED ON cepS VERSION 1.1 (May 28 2016) BUILT WITH GCC 5.2.1 20151010 on GNU/LINUX 64BIT (C) BY THE AUTHORS OF ceps (ceps is hosted at github: https://github.com/cepsdev/ceps.git) 
   THIS IS A GENERATED FILE. DO NOT MODIFY.
*/




#include "out.hpp"



void user_defined_init(){
 set_value(systemstates::s1 , 0);
 set_value(systemstates::extbg_luftdruck_pneumatiktank_out , 0);
 set_value(systemstates::extbg_luftdruck_pneumatiktank_in , 0);
 set_value(systemstates::extbg_luftdruck_stuetze_links_out , 0);
 set_value(systemstates::extbg_luftdruck_stuetze_links_in , 0);
 set_value(systemstates::extbg_luftdruck_stuetze_rechts_out , 0);
 set_value(systemstates::extbg_luftdruck_stuetze_rechts_in , 0);
 set_value(systemstates::extbg_zustand_mastantrieb_stoerung_kommunikation_out , 0);
 set_value(systemstates::extbg_zustand_mastantrieb_stoerung_kommunikation_in , 0);
 set_value(systemstates::extbg_zustand_mastantrieb_interner_fehler_out , 0);
 set_value(systemstates::extbg_zustand_mastantrieb_interner_fehler_in , 0);
 set_value(systemstates::extbg_zustand_horizontierantriebe_stoerung_kommunikation_out , 0);
 set_value(systemstates::extbg_zustand_horizontierantriebe_stoerung_kommunikation_in , 0);
 set_value(systemstates::extbg_zustand_antrieb_x_ok_out , 0);
 set_value(systemstates::extbg_zustand_antrieb_x_ok_in , 0);
 set_value(systemstates::extbg_zustand_antrieb_y_ok_out , 0);
 set_value(systemstates::extbg_zustand_antrieb_y_ok_in , 0);
 set_value(systemstates::extbg_zustand_talin_daten_gueltig_out , 0);
 set_value(systemstates::extbg_zustand_talin_daten_gueltig_in , 0);
 set_value(systemstates::extbg_zustand_antrieb_x_interner_fehler_out , 0);
 set_value(systemstates::extbg_zustand_antrieb_x_interner_fehler_in , 0);
 set_value(systemstates::extbg_zustand_antrieb_y_interner_fehler_out , 0);
 set_value(systemstates::extbg_zustand_antrieb_y_interner_fehler_in , 0);
 set_value(systemstates::extbg_zustand_heckverteiler_stoerung_out , 0);
 set_value(systemstates::extbg_zustand_heckverteiler_stoerung_in , 0);
 set_value(systemstates::extbg_zustand_heckverteiler_ok_out , 0);
 set_value(systemstates::extbg_zustand_heckverteiler_ok_in , 0);
 set_value(systemstates::extbg_kommunikation_ivenet_ok_out , 0);
 set_value(systemstates::extbg_kommunikation_ivenet_ok_in , 0);
}
 systemstates::State<int> s1;
 systemstates::State<int> extbg_luftdruck_pneumatiktank_out;
 systemstates::State<int> extbg_luftdruck_pneumatiktank_in;
 systemstates::State<int> extbg_luftdruck_stuetze_links_out;
 systemstates::State<int> extbg_luftdruck_stuetze_links_in;
 systemstates::State<int> extbg_luftdruck_stuetze_rechts_out;
 systemstates::State<int> extbg_luftdruck_stuetze_rechts_in;
 systemstates::State<int> extbg_zustand_mastantrieb_stoerung_kommunikation_out;
 systemstates::State<int> extbg_zustand_mastantrieb_stoerung_kommunikation_in;
 systemstates::State<int> extbg_zustand_mastantrieb_interner_fehler_out;
 systemstates::State<int> extbg_zustand_mastantrieb_interner_fehler_in;
 systemstates::State<int> extbg_zustand_horizontierantriebe_stoerung_kommunikation_out;
 systemstates::State<int> extbg_zustand_horizontierantriebe_stoerung_kommunikation_in;
 systemstates::State<int> extbg_zustand_antrieb_x_ok_out;
 systemstates::State<int> extbg_zustand_antrieb_x_ok_in;
 systemstates::State<int> extbg_zustand_antrieb_y_ok_out;
 systemstates::State<int> extbg_zustand_antrieb_y_ok_in;
 systemstates::State<int> extbg_zustand_talin_daten_gueltig_out;
 systemstates::State<int> extbg_zustand_talin_daten_gueltig_in;
 systemstates::State<int> extbg_zustand_antrieb_x_interner_fehler_out;
 systemstates::State<int> extbg_zustand_antrieb_x_interner_fehler_in;
 systemstates::State<int> extbg_zustand_antrieb_y_interner_fehler_out;
 systemstates::State<int> extbg_zustand_antrieb_y_interner_fehler_in;
 systemstates::State<int> extbg_zustand_heckverteiler_stoerung_out;
 systemstates::State<int> extbg_zustand_heckverteiler_stoerung_in;
 systemstates::State<int> extbg_zustand_heckverteiler_ok_out;
 systemstates::State<int> extbg_zustand_heckverteiler_ok_in;
 systemstates::State<int> extbg_kommunikation_ivenet_ok_out;
 systemstates::State<int> extbg_kommunikation_ivenet_ok_in;
 void globfuncs::S1__action__a1(){
  std::cout<<std::string{R"(s1=)"}<<systemstates::s1.value()<<std::string{R"(
)"};
  if ((systemstates::s1.value() < 100)) {
   smcore_interface->queue_event("E");
  } else {
   smcore_interface->queue_event("EXIT");
  }
  send(systemstates::id{"frame_zustand_komponenten"} , systemstates::id{"vcan_out"});
 }
 void globfuncs::S1__action__a2(){
  set_value(systemstates::s1 , (systemstates::s1.value() + 1));
  smcore_interface->queue_event("F");
 }
extern "C" void init_plugin(IUserdefined_function_registry* smc){
smcore_interface = smc->get_plugin_interface();
smc->register_global_init(user_defined_init);
smc->register_action("S1","a1", globfuncs::S1__action__a1);
smc->register_action("S1","a2", globfuncs::S1__action__a2);

}
std::ostream& operator << (std::ostream& o, systemstates::Variant const & v)
{
 if (v.what_ == systemstates::Variant::Int)
  o << v.iv_;
 else if (v.what_ == systemstates::Variant::Double)
  o << std::to_string(v.dv_);
 else if (v.what_ == systemstates::Variant::String)
  o << v.sv_;
 else
  o << "(Undefined)";
 return o;
}

size_t globfuncs::argc(){
 return smcore_interface->argc();
}sm4ceps_plugin_int::Variant globfuncs::argv(size_t j){
 return smcore_interface->argv(j);
}
void globfuncs::start_timer(double t,sm4ceps_plugin_int::ev ev_){ smcore_interface->start_timer(t,ev_); }
void globfuncs::start_timer(double t,sm4ceps_plugin_int::ev ev_,sm4ceps_plugin_int::id id_){smcore_interface->start_timer(t,ev_,id_);}
void globfuncs::start_periodic_timer(double t ,sm4ceps_plugin_int::ev ev_){smcore_interface->start_periodic_timer(t,ev_);}
void globfuncs::start_periodic_timer(double t,sm4ceps_plugin_int::ev ev_,sm4ceps_plugin_int::id id_){smcore_interface->start_periodic_timer(t,ev_,id_);}
void globfuncs::stop_timer(sm4ceps_plugin_int::id id_){smcore_interface->stop_timer(id_);}
