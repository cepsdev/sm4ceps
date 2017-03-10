/* out.cpp 
   CREATED Fri Mar 10 15:25:14 2017

   GENERATED BY THE sm4ceps C++ GENERATOR VERSION 0.80 (c) 2016 Tomas Prerovsky <tomas.prerovsky@gmail.com>, ALL RIGHTS RESERVED. 
   BASED ON cepS VERSION 1.1 (Mar 10 2017) BUILT WITH GCC 5.2.1 20151010 on GNU/LINUX 64BIT (C) BY THE AUTHORS OF ceps (ceps is hosted at github: https://github.com/cepsdev/ceps.git) 

   Input files:
      prelude.ceps
      can3.xml
      some_other_stuff.ceps

   THIS IS A GENERATED FILE.

   *** DO NOT MODIFY. ***
*/




#include "out.hpp"

#include<cmath>
using namespace std;




 std::ostream& systemstates::operator << (std::ostream& o, systemstates::State<int> & v){
  o << v.value();
  return o;
 }

 std::ostream& systemstates::operator << (std::ostream& o, systemstates::State<double> & v){
  o << v.value();
  return o;
 }

 
 systemstates::State<int>& systemstates::set_value(systemstates::State<int>& lhs, Variant const & rhs){
  if (rhs.what_ == sm4ceps_plugin_int::Variant::Double) {
    int v = rhs.dv_;
    lhs.set_changed(lhs.value() != v); lhs.value() = v;
  } else if (rhs.what_ == sm4ceps_plugin_int::Variant::Int) {
   lhs.set_changed(lhs.value() != rhs.iv_); lhs.value() = rhs.iv_;
  }
  return lhs;
 }

 systemstates::State<int>& systemstates::set_value(systemstates::State<int>& lhs, int rhs){lhs.set_changed(lhs.value() != rhs);lhs.value() = rhs; return lhs;}
 systemstates::State<double>& systemstates::set_value(systemstates::State<double>& lhs, double rhs){lhs.set_changed(lhs.value() != rhs);lhs.value() = rhs; return lhs;}
 systemstates::State<double>& systemstates::set_value(systemstates::State<double>& lhs, Variant const & rhs){
  if (rhs.what_ == sm4ceps_plugin_int::Variant::Int) {
   lhs.set_changed(lhs.value() != rhs.iv_); lhs.value() = rhs.iv_;}
  else if  (rhs.what_ == sm4ceps_plugin_int::Variant::Double){
   lhs.set_changed(lhs.value() != rhs.dv_); lhs.value() = rhs.dv_;}

  return lhs;
 }

 systemstates::State<std::string>& systemstates::set_value(systemstates::State<std::string>& lhs, std::string rhs){lhs.set_changed(lhs.value() != rhs);lhs.value() = rhs; return lhs;}



void init_frame_ctxts();


void user_defined_init(){
 set_value(systemstates::rpm , 0);
 set_value(systemstates::rpm_speed_src , 0);
 set_value(systemstates::speed , 0);
 set_value(systemstates::t , (2 * systemstates::rpm.value()));
 init_frame_ctxts();
}
 systemstates::State<double> systemstates::rpm;
 systemstates::State<double> systemstates::rpm_speed_src;
 systemstates::State<double> systemstates::speed;
 systemstates::State<int> systemstates::t;


 void init_frame_ctxts(){
  systemstates::Engine_RPM_in_ctxt.init();
 }
 

//Frame Context Definitions

void systemstates::Engine_RPM_in_ctxt_t::update_sysstates(){
 }
void systemstates::Engine_RPM_in_ctxt_t::read_chunk(void* chunk,size_t){
  raw_frm_dcls::Engine_RPM_in& in = *((raw_frm_dcls::Engine_RPM_in*)chunk);
 }
bool systemstates::Engine_RPM_in_ctxt_t::match_chunk(void* chunk,size_t chunk_size){
  if(chunk_size != 0) return false;
  raw_frm_dcls::Engine_RPM_in& in = *((raw_frm_dcls::Engine_RPM_in*)chunk);
  return true;
 }
void systemstates::Engine_RPM_in_ctxt_t::init(){
 }
sm4ceps_plugin_int::Framecontext*  systemstates::Engine_RPM_in_ctxt_t::clone(){
  return new Engine_RPM_in_ctxt_t(*this);
 }
 systemstates::Engine_RPM_in_ctxt_t::~Engine_RPM_in_ctxt_t (){
   }
 systemstates::Engine_RPM_in_ctxt_t systemstates::Engine_RPM_in_ctxt;


extern "C" void init_plugin(IUserdefined_function_registry* smc){
smcore_interface = smc->get_plugin_interface();
smc->register_global_init(user_defined_init);
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
void globfuncs::start_periodic_timer(double t,sm4ceps_plugin_int::Variant (*fp)(),sm4ceps_plugin_int::id id_){smcore_interface->start_periodic_timer(t,fp,id_);}
void globfuncs::start_periodic_timer(double t,sm4ceps_plugin_int::Variant (*fp)()){smcore_interface->start_periodic_timer(t,fp);}
void globfuncs::stop_timer(sm4ceps_plugin_int::id id_){smcore_interface->stop_timer(id_);}
bool globfuncs::in_state(std::initializer_list<sm4ceps_plugin_int::id> state_ids){return smcore_interface->in_state(state_ids);}
