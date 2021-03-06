/* out.hpp 
   CREATED Fri Jun 24 17:33:01 2016

   GENERATED BY the sm4ceps C++ Generator VERSION 0.50 (c) Tomas Prerovsky <tomas.prerovsky@gmail.com>, ALL RIGHTS RESERVED. 
   Requires C++1y compatible compiler (use --std=c++1y for g++) 
   BASED ON cepS VERSION 1.1 (Jun  1 2016) BUILT WITH GCC 5.2.1 20151010 on GNU/LINUX 64BIT (C) BY THE AUTHORS OF ceps (ceps is hosted at github: https://github.com/cepsdev/ceps.git) 

   Input files (relative paths):
      raw_frame_ex_1_basedefs.ceps
      raw_frame_ex_1_receiver.ceps

   THIS IS A GENERATED FILE.

   *** DO NOT MODIFY. ***
*/



#include <iostream>
#include <string>
#include <algorithm>
#include <map>
#include <vector>
#include <cstdlib>
#include "core/include/state_machine_simulation_core_reg_fun.hpp"
#include "core/include/state_machine_simulation_core_plugin_interface.hpp"
#include "user_defined.hpp"
#include"out_frames.hpp"

 static Ism4ceps_plugin_interface* smcore_interface; 

namespace systemstates{


 using sm4ceps_plugin_int::Variant;
 using sm4ceps_plugin_int::ev;
 using sm4ceps_plugin_int::id;

 template<typename T> class State{
   T v_;
   bool changed_ = true;
   bool default_constructed_ = true;
 public:
   State() = default; 
   State(T const & v):v_{v},default_constructed_{false} {}
   State& operator = (State const & rhs){
     if (!changed_ && !default_constructed_) changed_ = v_ != rhs.v_;
     else if (default_constructed_) {default_constructed_ = false;changed_ = true;}
     v_ = rhs.v_;
     return *this;
   }
   State& operator = (T const & rhs){
     if (!changed_ && !default_constructed_) changed_ = v_ != rhs;
     else if (default_constructed_) {changed_ = true; default_constructed_=false;}
     v_ = rhs;
     return *this;
   }
   bool changed() {auto t = changed_;changed_=false;return t;}

   T& value() {return v_;}
   T value() const {return v_;}
 
  };

 std::ostream& operator << (std::ostream& o, State<int> & v){
  o << v.value();
  return o;
 }

 std::ostream& operator << (std::ostream& o, State<double> & v){
  o << v.value();
  return o;
 }

 
 State<int>& set_value(State<int>& lhs, Variant const & rhs){lhs.value() = rhs.iv_; return lhs;}
 State<int>& set_value(State<int>& lhs, int rhs){lhs.value() = rhs; return lhs;}
 State<double>& set_value(State<double>& lhs, double rhs){lhs.value() = rhs; return lhs;}
 State<std::string>& set_value(State<std::string>& lhs, std::string rhs){lhs.value() = rhs; return lhs;}

 


 //void queue_event(std::string ev_name,std::initializer_list<Variant> vl = {});


State<int> s1;
State<int> extbg_luftdruck_pneumatiktank_out;
State<int> extbg_luftdruck_pneumatiktank_in;
State<int> extbg_luftdruck_stuetze_links_out;
State<int> extbg_luftdruck_stuetze_links_in;
State<int> extbg_luftdruck_stuetze_rechts_out;
State<int> extbg_luftdruck_stuetze_rechts_in;
State<int> extbg_zustand_mastantrieb_stoerung_kommunikation_out;
State<int> extbg_zustand_mastantrieb_stoerung_kommunikation_in;
State<int> extbg_zustand_mastantrieb_interner_fehler_out;
State<int> extbg_zustand_mastantrieb_interner_fehler_in;
State<int> extbg_zustand_horizontierantriebe_stoerung_kommunikation_out;
State<int> extbg_zustand_horizontierantriebe_stoerung_kommunikation_in;
State<int> extbg_zustand_antrieb_x_ok_out;
State<int> extbg_zustand_antrieb_x_ok_in;
State<int> extbg_zustand_antrieb_y_ok_out;
State<int> extbg_zustand_antrieb_y_ok_in;
State<int> extbg_zustand_talin_daten_gueltig_out;
State<int> extbg_zustand_talin_daten_gueltig_in;
State<int> extbg_zustand_antrieb_x_interner_fehler_out;
State<int> extbg_zustand_antrieb_x_interner_fehler_in;
State<int> extbg_zustand_antrieb_y_interner_fehler_out;
State<int> extbg_zustand_antrieb_y_interner_fehler_in;
State<int> extbg_zustand_heckverteiler_stoerung_out;
State<int> extbg_zustand_heckverteiler_stoerung_in;
State<int> extbg_zustand_heckverteiler_ok_out;
State<int> extbg_zustand_heckverteiler_ok_in;
State<int> extbg_kommunikation_ivenet_ok_out;
State<int> extbg_kommunikation_ivenet_ok_in;

struct frame_zustand_komponenten_in_ctxt_t : public sm4ceps_plugin_int::Framecontext {
 decltype(systemstates::extbg_kommunikation_ivenet_ok_in) extbg_kommunikation_ivenet_ok_in_;
 decltype(systemstates::extbg_luftdruck_stuetze_links_in) extbg_luftdruck_stuetze_links_in_;
 decltype(systemstates::extbg_luftdruck_stuetze_rechts_in) extbg_luftdruck_stuetze_rechts_in_;
 decltype(systemstates::extbg_zustand_antrieb_x_interner_fehler_in) extbg_zustand_antrieb_x_interner_fehler_in_;
 decltype(systemstates::extbg_zustand_antrieb_x_ok_in) extbg_zustand_antrieb_x_ok_in_;
 decltype(systemstates::extbg_zustand_antrieb_y_interner_fehler_in) extbg_zustand_antrieb_y_interner_fehler_in_;
 decltype(systemstates::extbg_zustand_antrieb_y_ok_in) extbg_zustand_antrieb_y_ok_in_;
 decltype(systemstates::extbg_zustand_heckverteiler_ok_in) extbg_zustand_heckverteiler_ok_in_;
 decltype(systemstates::extbg_zustand_mastantrieb_interner_fehler_in) extbg_zustand_mastantrieb_interner_fehler_in_;
 decltype(systemstates::extbg_zustand_mastantrieb_stoerung_kommunikation_in) extbg_zustand_mastantrieb_stoerung_kommunikation_in_;
 decltype(systemstates::extbg_zustand_talin_daten_gueltig_in) extbg_zustand_talin_daten_gueltig_in_;
 decltype(systemstates::s1) s1_;
 void update_sysstates();
 void read_chunk(void*,size_t);
 bool match_chunk(void*,size_t);
 void init();
 Framecontext* clone();
 virtual ~frame_zustand_komponenten_in_ctxt_t();
};
extern frame_zustand_komponenten_in_ctxt_t frame_zustand_komponenten_in_ctxt;

}
namespace guards{

 using Guard = bool(*)();
 using Guard_impl = bool ();




}
namespace globfuncs{

 extern bool in_state(std::initializer_list<systemstates::id>);
 extern void start_timer(double,sm4ceps_plugin_int::ev);
 extern void start_timer(double,sm4ceps_plugin_int::ev,sm4ceps_plugin_int::id);
 extern void start_periodic_timer(double,sm4ceps_plugin_int::ev);
 extern void start_periodic_timer(double,sm4ceps_plugin_int::ev,sm4ceps_plugin_int::id);
 extern void stop_timer(sm4ceps_plugin_int::id);
 size_t argc();
 sm4ceps_plugin_int::Variant argv(size_t);
 extern bool send(systemstates::id,systemstates::id);

systemstates::Variant handler_frame_zustand_komponenten();
void S1__action__a1();
void S1__action__a2();

}
std::ostream& operator << (std::ostream& o, systemstates::Variant const & v);
