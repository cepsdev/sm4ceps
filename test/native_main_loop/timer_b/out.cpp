/* out.cpp 
   CREATED Mon Jul 18 23:09:19 2016

   GENERATED BY the sm4ceps C++ Generator VERSION 0.50 (c) Tomas Prerovsky <tomas.prerovsky@gmail.com>, ALL RIGHTS RESERVED. 
   Requires C++1y compatible compiler (use --std=c++1y for g++) 
   BASED ON cepS VERSION 1.1 (May 28 2016) BUILT WITH GCC 5.2.1 20151010 on GNU/LINUX 64BIT (C) BY THE AUTHORS OF ceps (ceps is hosted at github: https://github.com/cepsdev/ceps.git) 

   Input files (relative paths):
      timer_b.ceps

   THIS IS A GENERATED FILE.

   *** DO NOT MODIFY. ***
*/




#include "out.hpp"

void init_frame_ctxts();


void user_defined_init(){
 set_value(systemstates::s1 , 0);
 init_frame_ctxts();
}
 systemstates::State<int> s1;


 void init_frame_ctxts(){
 }
 void globfuncs::S1__action__a1(){
  std::cout<<std::string{R"(S1.a1();
)"};
 }
 void globfuncs::S1__action__a2(){
  std::cout<<std::string{R"(S1.a2();
)"};
 }
 void globfuncs::S1__action__a3(){
  std::cout<<std::string{R"(S1.a3();
)"};
 }
 void globfuncs::S1__action__killt(){
  stop_timer(systemstates::id{"t1"});
 }
 void globfuncs::S1__action__on_enter(){
  start_periodic_timer(1 , systemstates::ev{"E"} , systemstates::id{"t1"});
 }


extern "C" void init_plugin(IUserdefined_function_registry* smc){
smcore_interface = smc->get_plugin_interface();
smc->register_global_init(user_defined_init);
smc->register_action("S1","a1", globfuncs::S1__action__a1);
smc->register_action("S1","a2", globfuncs::S1__action__a2);
smc->register_action("S1","a3", globfuncs::S1__action__a3);
smc->register_action("S1","killt", globfuncs::S1__action__killt);
smc->register_action("S1","on_enter", globfuncs::S1__action__on_enter);
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
