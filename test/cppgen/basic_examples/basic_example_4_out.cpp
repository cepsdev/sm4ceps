/* out.cpp 
   CREATED Fri Jun 24 15:10:20 2016

   GENERATED BY the sm4ceps C++ Generator VERSION 0.50 (c) Tomas Prerovsky <tomas.prerovsky@gmail.com>, ALL RIGHTS RESERVED. 
   Requires C++1y compatible compiler (use --std=c++1y for g++) 
   BASED ON cepS VERSION 1.1 (Jun  1 2016) BUILT WITH GCC 5.2.1 20151010 on GNU/LINUX 64BIT (C) BY THE AUTHORS OF ceps (ceps is hosted at github: https://github.com/cepsdev/ceps.git) 

   Input files (relative paths):
      basic_example_4.ceps

   THIS IS A GENERATED FILE.

   *** DO NOT MODIFY. ***
*/




#include "out.hpp"

void init_frame_ctxts();


void user_defined_init(){
 init_frame_ctxts();
}


 void init_frame_ctxts(){
 }
 void globfuncs::S__action__a1(){
  std::cout<<std::string{R"(S::a1
)"};
  smcore_interface->queue_event("A",{systemstates::Variant{10} , systemstates::Variant{20} , systemstates::Variant{30}});
 }
 void globfuncs::S__action__a2(){
  std::cout<<std::string{R"(S::a2
)"};
  std::cout<<std::string{R"(Number of arguments = )"}<<argc()<<std::string{R"(
)"};
  std::cout<<std::string{R"(Event = )"}<<argv(0)<<std::string{R"(
)"};
  std::cout<<std::string{R"(argv(1) = )"}<<argv(1)<<std::string{R"(
)"};
  std::cout<<std::string{R"(argv(2) = )"}<<argv(2)<<std::string{R"(
)"};
  std::cout<<std::string{R"(argv(3) = )"}<<argv(3)<<std::string{R"(
)"};
  smcore_interface->queue_event("B",{systemstates::Variant{std::string{R"(A)"}} , systemstates::Variant{std::string{R"(B)"}}});
 }
 void globfuncs::S__action__a3(){
  std::cout<<std::string{R"(S::a3
)"};
  std::cout<<std::string{R"(Number of arguments = )"}<<argc()<<std::string{R"(
)"};
  std::cout<<std::string{R"(Event = )"}<<argv(0)<<std::string{R"(
)"};
  std::cout<<std::string{R"(argv(1) = )"}<<argv(1)<<std::string{R"(
)"};
  std::cout<<std::string{R"(argv(2) = )"}<<argv(2)<<std::string{R"(
)"};
  smcore_interface->queue_event("C",{systemstates::Variant{1.1} , systemstates::Variant{2.2} , systemstates::Variant{std::string{R"(AA)"}} , systemstates::Variant{std::string{R"(BB)"}} , systemstates::Variant{3}});
 }
 void globfuncs::S__action__a4(){
  std::cout<<std::string{R"(S::a4
)"};
  std::cout<<std::string{R"(Number of arguments = )"}<<argc()<<std::string{R"(
)"};
  std::cout<<std::string{R"(Event = )"}<<argv(0)<<std::string{R"(
)"};
  std::cout<<std::string{R"(argv(1) = )"}<<argv(1)<<std::string{R"(
)"};
  std::cout<<std::string{R"(argv(2) = )"}<<argv(2)<<std::string{R"(
)"};
  std::cout<<std::string{R"(argv(3) = )"}<<argv(3)<<std::string{R"(
)"};
  std::cout<<std::string{R"(argv(4) = )"}<<argv(4)<<std::string{R"(
)"};
  std::cout<<std::string{R"(argv(5) = )"}<<argv(5)<<std::string{R"(
)"};
 }


extern "C" void init_plugin(IUserdefined_function_registry* smc){
smcore_interface = smc->get_plugin_interface();
smc->register_global_init(user_defined_init);
smc->register_action("S","a1", globfuncs::S__action__a1);
smc->register_action("S","a2", globfuncs::S__action__a2);
smc->register_action("S","a3", globfuncs::S__action__a3);
smc->register_action("S","a4", globfuncs::S__action__a4);
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
