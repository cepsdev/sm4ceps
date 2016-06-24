/* out.hpp 
   CREATED Fri Jun 24 15:10:19 2016

   GENERATED BY the sm4ceps C++ Generator VERSION 0.50 (c) Tomas Prerovsky <tomas.prerovsky@gmail.com>, ALL RIGHTS RESERVED. 
   Requires C++1y compatible compiler (use --std=c++1y for g++) 
   BASED ON cepS VERSION 1.1 (Jun  1 2016) BUILT WITH GCC 5.2.1 20151010 on GNU/LINUX 64BIT (C) BY THE AUTHORS OF ceps (ceps is hosted at github: https://github.com/cepsdev/ceps.git) 

   Input files (relative paths):
      basic_example_3.ceps

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

void S__action__a1();
void S__action__a2();
void S__action__a3();

}
std::ostream& operator << (std::ostream& o, systemstates::Variant const & v);
