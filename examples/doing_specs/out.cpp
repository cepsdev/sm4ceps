/* out.cpp 
   CREATED Fri Oct  7 14:28:02 2016

   GENERATED BY THE sm4ceps C++ GENERATOR VERSION 0.80 (c) 2016 Tomas Prerovsky <tomas.prerovsky@gmail.com>, ALL RIGHTS RESERVED. 
   BASED ON cepS VERSION 1.1 (Oct  7 2016) BUILT WITH GCC 5.2.1 20151010 on GNU/LINUX 64BIT (C) BY THE AUTHORS OF ceps (ceps is hosted at github: https://github.com/cepsdev/ceps.git) 

   Input files:
      abstract_statemachine.ceps

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
 init_frame_ctxts();
}


 void init_frame_ctxts(){
 }
 guards::Guard guards::AbstractStatemachine2_D_State1_D_guard_1 = guards::guard_impl_1;
 guards::Guard guards::State1Implementation_D_guard_1 = guards::guard_impl_2;
 guards::Guard guards::State2Implementation_D_guard_1 = guards::guard_impl_3;
 bool guards::guard_impl_1(){
  return 1;
 }
 bool guards::guard_impl_2(){
  return 1;
 }
 bool guards::guard_impl_3(){
  return 1;
 }
 void globfuncs::AbstractStatemachine2__State1__action__a(){
  std::cout<<std::string{R"(State1Implementation.a()
)"};
 }
 void globfuncs::AbstractStatemachine2__State1__action__on_enter(){
  std::cout<<std::string{R"(State1Implementation.on_enter();
)"};
 }
 void globfuncs::AbstractStatemachine2__State1__action__on_exit(){
  std::cout<<std::string{R"(State1Implementation.on_exit();
)"};
 }
 void globfuncs::State1__action__a(){
  std::cout<<std::string{R"(State1Implementation.a()
)"};
 }
 void globfuncs::State1__action__on_enter(){
  std::cout<<std::string{R"(State1Implementation.on_enter();
)"};
 }
 void globfuncs::State1__action__on_exit(){
  std::cout<<std::string{R"(State1Implementation.on_exit();
)"};
 }
 void globfuncs::State2__action__a(){
  std::cout<<std::string{R"(State1Implementation.a()
)"};
 }
 void globfuncs::State1Implementation__action__a(){
  std::cout<<std::string{R"(State1Implementation.a()
)"};
 }
 void globfuncs::State1Implementation__action__on_enter(){
  std::cout<<std::string{R"(State1Implementation.on_enter();
)"};
 }
 void globfuncs::State1Implementation__action__on_exit(){
  std::cout<<std::string{R"(State1Implementation.on_exit();
)"};
 }
 void globfuncs::State2Implementation__action__a(){
  std::cout<<std::string{R"(State1Implementation.a()
)"};
 }


extern "C" void init_plugin(IUserdefined_function_registry* smc){
smcore_interface = smc->get_plugin_interface();
smc->register_global_init(user_defined_init);
smc->register_guard("AbstractStatemachine2.State1.guard_1",&guards::AbstractStatemachine2_D_State1_D_guard_1);
smc->register_guard("State1Implementation.guard_1",&guards::State1Implementation_D_guard_1);
smc->register_guard("State2Implementation.guard_1",&guards::State2Implementation_D_guard_1);
smc->register_action("AbstractStatemachine2.State1","a", globfuncs::AbstractStatemachine2__State1__action__a);
smc->register_action("AbstractStatemachine2.State1","on_enter", globfuncs::AbstractStatemachine2__State1__action__on_enter);
smc->register_action("AbstractStatemachine2.State1","on_exit", globfuncs::AbstractStatemachine2__State1__action__on_exit);
smc->register_action("State1","a", globfuncs::State1__action__a);
smc->register_action("State1","on_enter", globfuncs::State1__action__on_enter);
smc->register_action("State1","on_exit", globfuncs::State1__action__on_exit);
smc->register_action("State2","a", globfuncs::State2__action__a);
smc->register_action("State1Implementation","a", globfuncs::State1Implementation__action__a);
smc->register_action("State1Implementation","on_enter", globfuncs::State1Implementation__action__on_enter);
smc->register_action("State1Implementation","on_exit", globfuncs::State1Implementation__action__on_exit);
smc->register_action("State2Implementation","a", globfuncs::State2Implementation__action__a);
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
