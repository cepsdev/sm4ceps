#define _CRT_SECURE_NO_WARNINGS

#include "core/include/state_machine_simulation_core.hpp"
#include "core/include/sm_comm_naive_msg_prot.hpp"
#include "core/include/sm_ev_comm_layer.hpp"
#include "core/include/serialization.hpp"
#include "core/include/sm_raw_frame.hpp"
#include "core/include/sm_xml_frame.hpp"
#include "pugixml.hpp"
#ifdef __gnu_linux__

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <dlfcn.h>

#else

#endif
#include <sys/types.h>
#include <limits>
#include <cstring>

#include "core/include/base_defs.hpp"
#include <cassert>
#include <algorithm>
#include <unordered_set>
#include <list>


typedef void (*init_plugin_t)(IUserdefined_function_registry*);


void register_plugin(State_machine_simulation_core* smc,std::string const& id,ceps::ast::Nodebase_ptr (*fn) (ceps::ast::Call_parameters* ))
{
	smc->register_plugin_fn(id,fn);
}

const auto SM4CEPS_VER_MAJ = 0;
const auto SM4CEPS_VER_MINOR = .600;

int get_sm4ceps_ver_maj() { return SM4CEPS_VER_MAJ; }
double get_sm4ceps_ver_minor() { return SM4CEPS_VER_MINOR; }
int odd(int i) { return i % 2; }
int even(int i) { return !odd(i); }
int truncate(double i) { return (int)i; }
int truncate(int i) { return i; }
double mymin(double a, double b) { return std::min(a,b); }
double mymin(double a, int b) { return std::min(a, (double)b); }
double mymin(int a, double b) { return std::min((double)a, b); }
int mymin(int a, int b) { return std::min(a, b); }

extern std::vector<std::thread*> comm_threads;


bool read_func_call_values(State_machine_simulation_core* smc,	ceps::ast::Nodebase_ptr root_node,
							std::string & func_name,
							std::vector<ceps::ast::Nodebase_ptr>& args);
void comm_sender_generic_tcp_out_thread(threadsafe_queue< std::tuple<char*,size_t,size_t>, std::queue<std::tuple<char*,size_t,size_t> >>* frames,
			     State_machine_simulation_core* smc,
			     std::string ip,
			     std::string port,
			     std::string som,
			     std::string eof,
			     std::string sock_name,
			     bool reuse_socket, bool reg_socket);

void comm_generic_tcp_in_dispatcher_thread(int id,
				 Rawframe_generator* gen,
				 std::string ev_id,
				 std::vector<std::string> params,
			     State_machine_simulation_core* smc,
			     std::string ip,
			     std::string port,std::string som,std::string eof,std::string sock_name,bool reg_sock,bool reuse_sock,
			     void (*handler_fn) (int,Rawframe_generator*,std::string,std::vector<std::string> ,State_machine_simulation_core* , sockaddr_storage,int,std::string,std::string));

void comm_generic_tcp_in_thread_fn(int id,
		 Rawframe_generator* gen,
		 std::string ev_id,
		 std::vector<std::string> params,
		 State_machine_simulation_core* smc,
		 sockaddr_storage claddr,int sck,std::string som,std::string eof);

extern void print_qualified_id(std::ostream& os,std::vector<std::string> const & q_id);
extern std::string qualified_id_to_str(std::vector<std::string> const & q_id);

template<typename F, typename T> void traverse_sm(std::unordered_set<State_machine*>& m,State_machine* sm, T const & sms, F f){
	f(sm);
	
	for(auto state: sm->states()){
		if (!state->is_sm() || state->smp() == nullptr) continue;
		if (m.find(state->smp()) != m.end()) continue;
		m.insert(state->smp());
		traverse_sm(m,state->smp(),sms,f);
	}
	
	for(auto subsm: sm->children()){
		//assert(m.find(subsm) != m.end());			
		if (m.find(subsm) != m.end()) continue;
		m.insert(subsm);
		traverse_sm(m,subsm,sms,f);
	}
}

template<typename F, typename T> void traverse_sms(T const & sms, F f){
	std::unordered_set<State_machine*> m;
	for(auto sm: sms){
	 if (m.find(sm) != m.end()) continue;
	 traverse_sm(m,sm,sms,f);
	 m.insert(sm);
	}	
}

int compute_state_and_event_ids(State_machine_simulation_core* smp,
		                        std::map<std::string,State_machine*> sms,
								std::map<std::string,int>& map_fullqualified_sm_id_to_computed_idx){
	using namespace ceps::ast;
	using namespace std;
	DEBUG_FUNC_PROLOGUE;
	
	smp->executionloop_context().transitions.clear();
	smp->executionloop_context().state_to_first_transition.clear();


	std::vector<State_machine*> smsv;
	auto & ev_to_id = smp->executionloop_context().ev_to_id;
	

	for(auto sm : sms) smsv.push_back(sm.second);
	int ctr = 1;
	int ev_ctr = 1;
	traverse_sms(smsv,[&ctr,&ev_ctr,&ev_to_id](State_machine* cur_sm){
		cur_sm->idx_ = ctr++;
		for(auto it = cur_sm->states().begin(); it != cur_sm->states().end(); ++it) {
		 auto state = *it;
		 state->idx_ = ctr++;
		 if (!state->is_sm_) continue;
		 state->smp_->idx_ = state->idx_;
		}	
	 }
	);

	smp->executionloop_context().number_of_states = ctr-1;

	traverse_sms(smsv,[&ctr,&ev_ctr,&ev_to_id](State_machine* cur_sm){
	  for(auto & t : cur_sm->transitions()){

		for(auto e : t.events()){
		 if(ev_to_id.find(e.id_) != ev_to_id.end()) continue;
		 ev_to_id[e.id_] = ev_ctr;
		 ++ev_ctr;
		}
	  }
	 } 
	);

	traverse_sms(smsv,[&ctr,&ev_ctr,&ev_to_id](State_machine* cur_sm){
		for(auto it = cur_sm->states().begin(); it != cur_sm->states().end(); ++it) {
		 auto state = *it;
		 for(auto & t : cur_sm->transitions()){
			if (t.from_.is_sm_) {t.from_.idx_ = t.from_.smp_->idx_;assert(t.from_.idx_>0);}
			else if (t.from_.id_ == state->id_) {t.from_.idx_ = state->idx_;assert(t.from_.idx_>0);}
			if (t.to_.is_sm_) {t.to_.idx_ = t.to_.smp_->idx_;assert(t.to_.idx_>0);}
			else if (t.to_.id_ == state->id_) {t.to_.idx_ = state->idx_;assert(t.to_.idx_>0);}
		 }//for
		}
	 }
	);



	auto& ctx = smp->executionloop_context();
	//Build loop execution context
	traverse_sms(smsv,[&ctr,&ev_ctr,&ev_to_id, &ctx,&smsv](State_machine* cur_sm){
		executionloop_context_t::transition_t t;
		t.smp = cur_sm->idx_;
		assert(t.smp > 0);
		ctx.transitions.push_back(t);
		ctx.state_to_first_transition[t.smp] = ctx.transitions.size()-1;

		auto insert_transitions = [&ctx,&ev_to_id](State_machine* sm_from, State_machine* sm_to){
			 for(auto const & t : sm_from->transitions()){
			  if (sm_from != sm_to){
			   if (!t.from_.is_sm_) continue;
			   if (t.from_.smp_ != sm_to) continue;
			  } else {
			   if (t.from_.is_sm_) continue;
			   if (t.from_.parent_!= nullptr && t.from_.parent_ != sm_from) continue;
			  }
			  executionloop_context_t::transition_t tt;
			  tt.smp = sm_to->idx_;
			  tt.from = t.from_.idx_;
			  assert(tt.from > 0);
			  tt.to = t.to_.idx_;
			  assert(tt.to > 0);
			  for(auto  const &  ev : t.events())
			  {
				tt.ev = ev_to_id[ev.id_];
				assert(tt.ev > 0);
				break;
			  }
			  tt.guard = t.guard_native_;
			  if (t.action_.size() >= 1) tt.a1 = t.action_[0].native_func_;
			  if (t.action_.size() >= 2) tt.a1 = t.action_[1].native_func_;
			  if (t.action_.size() >= 3) tt.a1 = t.action_[2].native_func_;
			  ctx.transitions.push_back(tt);
			  if(ctx.state_to_first_transition.find(tt.from) != ctx.state_to_first_transition.end()) continue;
			  ctx.state_to_first_transition[tt.from] = ctx.transitions.size()-1;
			 }
		};


		//Fetch foreign transitions i.e. transitions with from == cur_sm
		traverse_sms(smsv,[&ctx,&cur_sm,&ev_to_id,insert_transitions](State_machine* sm){
			if (sm != cur_sm) insert_transitions(sm,cur_sm);
		});

		insert_transitions(cur_sm,cur_sm);
	 }
	);

	//Set parent information
	traverse_sms(smsv,[&ctr,&ev_ctr,&ev_to_id, &ctx](State_machine* cur_sm){
		ctx.set_inf(cur_sm->idx_,executionloop_context_t::SM,true);
		ctx.set_inf(cur_sm->idx_,executionloop_context_t::THREAD,cur_sm->is_thread_);
		ctx.set_inf(cur_sm->idx_,executionloop_context_t::JOIN,cur_sm->join_);
		if (cur_sm->join_) {
			for(auto s : cur_sm->states()){
				if(s->id_ !=cur_sm->join_state_.id_) continue;
				ctx.set_join_state(cur_sm->idx_,s->idx_);
				break;
			}

		}
		for(auto s : cur_sm->states()){
			ctx.set_parent(s->idx_,cur_sm->idx_);
			ctx.set_inf(s->idx_,executionloop_context_t::INIT,s->is_initial());
			ctx.set_inf(s->idx_,executionloop_context_t::FINAL,s->is_final());
			if (s->is_initial()) ctx.set_initial_state(cur_sm->idx_,s->idx_);
			if (s->is_final()) ctx.set_final_state(cur_sm->idx_,s->idx_);
		}
		for(auto s : cur_sm->children()){
			ctx.set_parent(s->idx_,cur_sm->idx_);
		}
	});

	//Set on_enter / on_exit information
	traverse_sms(smsv,[&ctr,&ev_ctr,&ev_to_id, &ctx](State_machine* cur_sm){
		for(auto a : cur_sm->actions()){
			if (a.id_ == "on_enter") {
			 ctx.set_on_enter(cur_sm->idx_,a.native_func_);
			 ctx.set_inf(cur_sm->idx_,executionloop_context_t::ENTER,true);
			}
			else if (a.id_ == "on_exit") {
			 ctx.set_on_exit(cur_sm->idx_,a.native_func_);
			 ctx.set_inf(cur_sm->idx_,executionloop_context_t::EXIT,true);
			}
		}
	});

	//Set children information
	ctx.init_state_to_children();
	traverse_sms(smsv,[&ctr,&ev_ctr,&ev_to_id, &ctx](State_machine* cur_sm){
		ctx.state_to_children[cur_sm->idx_] = ctx.children.size();
		ctx.children.push_back(cur_sm->idx_);
		for(auto s : cur_sm->states())ctx.children.push_back(s->idx_);
		ctx.children.push_back(0);
	});


	traverse_sms(smsv,[&map_fullqualified_sm_id_to_computed_idx,&smp](State_machine* cur_sm){
		state_rep_t srep(true,true,cur_sm,cur_sm->id(),cur_sm->idx_);
		//std::string t1;
		map_fullqualified_sm_id_to_computed_idx[ smp->get_fullqualified_id(srep)]=cur_sm->idx_;
		
		for(auto it = cur_sm->states().begin(); it != cur_sm->states().end(); ++it) {
		 auto state = *it;
		 map_fullqualified_sm_id_to_computed_idx[smp->get_full_qualified_id(*state)]=state->idx_;
		 //std::cout << t1 <<" : " << state->idx_ << " : " << state->id()<< std::endl;
		}	
	 } 
	);
	
	DEBUG << "[LOG4CEPS][GLOBAL_STATE_COUNT=" << ctr <<"]\n";
	for(auto e : map_fullqualified_sm_id_to_computed_idx){
	 DEBUG << "[LOG4CEPS]["<< e.first << " => " << e.second <<"]\n";
	}

	return ctr;
}
bool State_machine_simulation_core::register_action_impl(std::string state_machine_id,
		                                                 std::string action,
														 void(*fn)(),State_machine* parent){
	if (state_machine_id.length() == 0)
	{
		if (parent == nullptr) return false;
		bool r = false;
		for(auto & a : parent->actions()){
			if (a.id_ != action) continue;
			a.native_func() = fn;
			r = true;
		}
		for(auto & t: parent->transitions()){
		  if(t.orig_parent() != nullptr && parent != t.orig_parent())//transition was moved
		    continue;
			for (auto & a : t.actions()){
				if (a.id_ != action) continue;
				a.native_func() = fn;
				r = true;
			}
		}
		for(auto foreign_smp: parent->smps_containing_moved_transitions()){
		 for(auto & t: foreign_smp->transitions()){
		  if(t.orig_parent() !=  parent )//transition was moved
		    continue;
			for (auto & a : t.actions()){
				if (a.id_ != action) continue;
				a.native_func() = fn;
				r = true;
			}
		 }
		}
		return r;
	}

	auto i = state_machine_id.find_first_of('.');
	std::string base;
	std::string rest;

	if (i == std::string::npos) base = state_machine_id;
	else {base = state_machine_id.substr(0,i);rest =  state_machine_id.substr(i+1);}
	if (parent == nullptr){
		auto it = State_machine::statemachines.find(base);
		if (it == State_machine::statemachines.end()) {
			return false;
		}
		return register_action_impl(rest,action,fn,it->second);
	}

	for(auto smp: parent->children()){
		if (smp->id() == base)
			return register_action_impl(rest,action,fn,smp);
	}

	for(auto s: parent->states()){
		if(!s->is_sm_) continue;
		if (s->id() == base)
			return register_action_impl(rest,action,fn,s->smp_);
	}
	return false;
}

bool State_machine_simulation_core::register_action(std::string state_machine_id,std::string action, void(*fn)()){
	auto r = register_action_impl(state_machine_id,action,fn,nullptr);
	if (!r)
			this->fatal_(-1,"Failed to register native implementation for '"+state_machine_id+"."+action+"'");
	return true;
}


void State_machine_simulation_core::register_frame_ctxt(sm4ceps_plugin_int::Framecontext* ctxt, std::string receiver_id){
	if (ctxt == nullptr) return;
	auto thrd_ctxt = this->get_dispatcher_thread_ctxt(receiver_id);
	if(thrd_ctxt ==nullptr) this->fatal_(-1,"State_machine_simulation_core::register_frame_ctxt: unknown receiver id '"+receiver_id+"'");
	thrd_ctxt->get_native_handler().push_back(ctxt);

}

void State_machine_simulation_core::processs_content(Result_process_cmd_line const& result_cmd_line,State_machine **entry_machine)
{
	using namespace ceps::ast;
	using namespace std;
	DEBUG_FUNC_PROLOGUE;

	this->enforce_native() = result_cmd_line.enforce_native;

	regfn("sm4ceps_version_major",get_sm4ceps_ver_maj);
	regfn("sm4ceps_version_minor", get_sm4ceps_ver_minor);
	regfn("odd", odd);
	regfn("even", even);
	regfn("truncate",static_cast<int(*)(double)>(truncate));
	regfn("truncate",static_cast<int(*)(int)>(truncate));
	regfn("min", static_cast<double(*)(double,double)> (mymin));
	regfn("min", static_cast<double(*)(int, double)> (mymin));
	regfn("min", static_cast<double(*)(double, int)> (mymin));
	regfn("min", static_cast<int(*)(int, int)> (mymin));

	ceps::ast::Nodeset ns = current_universe();

	auto statemachines = ns[all{"Statemachine"}];
	auto globalfunctions = ns["global_functions"];
	auto frames = ns[all{"raw_frame"}];
	auto xmlframes = ns[all{"xml_frame"}];
	auto all_sender = ns[all{"sender"}];
	auto all_receiver = ns[all{"receiver"}];
	auto unique_event_declarations = ns["unique"];
	auto no_transitions_declarations = ns["no_transitions"];
	auto exported_events = ns["export"];

	start_comm_threads() = !generate_cpp_code();


	//handle unique definitions

	if (unique_event_declarations.size()){
		for (auto const & e : unique_event_declarations.nodes()){
			if (e->kind() != ceps::ast::Ast_node_kind::symbol || ceps::ast::kind(ceps::ast::as_symbol_ref(e)) != "Event")
				fatal_(-1,"unique definition requires a list of events.");
			auto  & ev = ceps::ast::as_symbol_ref(e);
			std::string id = ceps::ast::name(ev);
			this->unique_events().insert(id);
		}
	}
	//handle no_transitions definitions

	if (no_transitions_declarations.size()){
		for (auto const & e : no_transitions_declarations.nodes()){
			if (e->kind() != ceps::ast::Ast_node_kind::symbol || ceps::ast::kind(ceps::ast::as_symbol_ref(e)) != "Event")
				fatal_(-1,"no_transitions definition requires a list of events.");
			auto  & ev = ceps::ast::as_symbol_ref(e);
			std::string id = ceps::ast::name(ev);
			this->not_transitional_events().insert(id);
		}
	}

	post_event_processing() = ns["post_event_processing"];

	auto can_frames = ns[all{"frame"}];
	frames.nodes().insert(frames.nodes().end(),can_frames.nodes().begin(),can_frames.nodes().end());


	for(auto p:globalfunctions.nodes())
	{
		if(p->kind() != ceps::ast::Ast_node_kind::structdef) continue;
		std::string id = ceps::ast::name(ceps::ast::as_struct_ref(p));
		global_funcs()[id] = p;
	}

	if (statemachines.empty())
	 if(warn_) warn_(WARN_NO_STATEMACHINES,"");

	for (auto statemachine_ : statemachines)
	{
		auto statemachine = statemachine_["Statemachine"];
		process_statemachine(statemachine,"",nullptr,1,0);
	}//for
	auto entry_ = ns["main"];

	if (!entry_.empty() && entry_machine != nullptr) {
		string entry_name = name(as_id_ref(entry_.nodes()[0]));
		*entry_machine = State_machine::statemachines[entry_name ];
		if (entry_machine == nullptr)
		  fatal_(-1,"No statemachine with id "+entry_name+" found: No main statemachine defined.");
	}


	//BACK PATCH IMPORTS

	for(;;)
	{

		bool imports_resolved = false;
		auto s_t = State_machine::statemachines;
		for(auto & sim_ : s_t)
		{
			DEBUG << "[RESOLVE_IMPORTS][ID =  " << sim_.second->id_ << "]\n";
			auto & sim = sim_.second;
			if (sim->definition_complete() || sim->unresolved_imports().size() == 0) continue;

			if (!resolve_imports(*sim))
				{
				 fatal_(-1,sim->id()+": Error while resolving imports");
				}
			imports_resolved = true;
		}
		if (!imports_resolved) break;
	}

	DEBUG << "[BACK PATCH JOINS]\n";

	for(auto & sim_ : State_machine::statemachines)
	{
		if (!sim_.second->join()) continue;
		if (!sim_.second->join_state().unresolved()) continue;

		DEBUG << "[BACK PATCH JOIN STATE][SM=" << sim_.first <<"]\n";
		State_machine::State s;
		resolve_q_id(sim_.second, sim_.second->join_state().q_id(), s);
		sim_.second->join_state() = s;

	}


	DEBUG << "[BACK PATCH TRANSITIONS]\n";


	//STEP#1 resolve transitions
	DEBUG << "[BACK PATCH TRANSITIONS][STEP(1):RESOLVE_TRANSITIONS]\n";
	std::set<State_machine*> machines_with_transitions_which_may_have_foreign_source;
	for(auto & sim_ : State_machine::statemachines)
	{
		for(size_t i = 0; i < sim_.second->transitions().size(); ++i)
		{
			auto & t = sim_.second->transitions()[i];
			if (t.from_.unresolved()) {
				DEBUG << "[RESOLVE FROM]\n";
				State_machine::State s;
				resolve_q_id(sim_.second, t.from_.q_id(), s);
				auto orig_q_id = t.from_.q_id();
				t.from_ = s;
				assert(t.from_.is_sm_ || t.from_.smp_ != nullptr);
				assert(t.from_.id_.size());
				assert(!t.from_.unresolved());

				if (print_debug_info_){
					state_rep_t sr,sr2;
					sr.valid_ = sr2.valid_ = true;
					sr.is_sm_ = s.is_sm_;
					sr.sid_ = s.id_;
					sr.smp_ = s.smp_;
					sr2.is_sm_ = true;
					sr2.sid_ = sim_.second->id_;
					sr2.smp_ = sim_.second;
					DEBUG << "[RESOLVE Q-ID STATE][SM=" << get_fullqualified_id(sr2) << "]"
						  <<"[" << qualified_id_to_str(orig_q_id)<< " ==> " << get_fullqualified_id(sr) << "]\n";
				}

				if (t.from_.parent() == sim_.second) t.from_.parent_ = nullptr;
				machines_with_transitions_which_may_have_foreign_source.insert(sim_.second);

			}
			if (t.to_.unresolved()){
				DEBUG << "[RESOLVE TO]\n";
				State_machine::State s;
				resolve_q_id(sim_.second, t.to_.q_id(), s);
				auto orig_q_id = t.to_.q_id();
				t.to_ = s;
				assert(t.to_.is_sm_ || t.to_.smp_ != nullptr);
				assert(t.to_.id_.size());
				assert(!t.to_.unresolved());
				if (print_debug_info_){
					state_rep_t sr,sr2;
					sr.valid_ = sr2.valid_ = true;
					sr.is_sm_ = s.is_sm_;
					sr.sid_ = s.id_;
					sr.smp_ = s.smp_;
					sr2.is_sm_ = true;
					sr2.sid_ = sim_.second->id_;
					sr2.smp_ = sim_.second;
					DEBUG << "[RESOLVE Q-ID STATE][SM=" << get_fullqualified_id(sr2) << "][" << qualified_id_to_str(orig_q_id)<< " ==> " << get_fullqualified_id(sr) << "]\n";
				}
			}
		}//for
	}//for


	DEBUG << "[BACK PATCH TRANSITIONS][STEP(1):MOVE_FOREIGN_TRANSITIONS]\n";
	for(auto sm :  machines_with_transitions_which_may_have_foreign_source)
	{
		auto it = std::stable_partition(sm->transitions().begin(),sm->transitions().end(),
				[&] (State_machine::Transition const & t) {
					return !t.from_.is_foreign(sm);
					}
				);
		if (it == sm->transitions().end()) continue;

		std::for_each(it,sm->transitions().end(),
		 [&](State_machine::Transition & t) {
			DEBUG << "[TRANSITION_MOVE(PRE)][FROM="<< get_full_qualified_id(t.from_) << "][TO=" << get_full_qualified_id(t.to_) << "]\n";
			auto parent = t.from_.parent();t.from_.parent_=nullptr;t.from_.q_id_.clear();
			if(!t.from_.is_sm_)t.from_.smp_ = parent;
			assert(sm != parent);
			t.orig_parent() = sm;
			parent->transitions().push_back(t);
			sm->smps_containing_moved_transitions().push_back(parent);

			DEBUG << "[TRANSITION_MOVE(POST)][FROM="<< get_full_qualified_id(t.from_) << "][TO=" << get_full_qualified_id(t.to_) << "]\n";
			}
		);
		sm->transitions().erase(it,sm->transitions().end());
	}

	for (auto m: State_machine::statemachines)
	{
		for(auto& t: m.second->transitions())
		{
			assert(!t.from().unresolved());
			assert(!t.to().unresolved());
			assert(t.from().id_.size() > 0);
			assert(t.to().id_.size() > 0);
		}
	}

	DEBUG << "[PROCESSING TYPE DEFINITIONS][START]\n";

	auto typedefs = ns["typedef"];
	if (typedefs.size())
		for(auto node: typedefs.nodes())
		{
			if(node->kind() != ceps::ast::Ast_node_kind::structdef) continue;
			auto td = ceps::ast::as_struct_ptr(node);
			type_definitions()[ceps::ast::name(*td)] = td;
			DEBUG << "[PROCESSING TYPE DEFINITIONS][TYPEDEF] "<< ceps::ast::name(*td) <<"\n";
		}
	DEBUG << "[PROCESSING TYPE DEFINITIONS][END]\n";

	//Handle CALL frames

	for(auto rawframe_ : frames)
	{
		auto rawframe = rawframe_["raw_frame"];
		if(!rawframe.size()) rawframe = rawframe_["frame"];

		auto gen = new Podframe_generator;
		if (rawframe["id"].size() != 1)
			fatal_(-1,"raw_frame definition: missing id.");
		if (rawframe["id"].nodes()[0]->kind() != ceps::ast::Ast_node_kind::identifier)
			fatal_(-1,"raw_frame definition: id must be an unbound identifier.");

		std::string id = ceps::ast::name(ceps::ast::as_id_ref(rawframe["id"].nodes()[0]));
		DEBUG << "[PROCESSING RAW_FRAME_SPEC("<< id <<")][START]\n";
		gen->readfrom_spec(rawframe);
		frame_generators()[id] = gen;
		DEBUG << "[PROCESSING RAW_FRAME_SPEC][FINISHED]\n";
	}

	for(auto xmlframe_ : xmlframes)
	{
		auto xmlframe = xmlframe_["xml_frame"];
		auto gen = new Xmlframe_generator;
		if (xmlframe["id"].size() != 1)
			fatal_(-1,"xml_frame definition: missing id.");
		if (xmlframe["id"].nodes()[0]->kind() != ceps::ast::Ast_node_kind::identifier)
			fatal_(-1,"xml_frame definition: id must be an unbound identifier.");

		std::string id = ceps::ast::name(ceps::ast::as_id_ref(xmlframe["id"].nodes()[0]));
		DEBUG << "[PROCESSING XML_FRAME_SPEC("<< id <<")][START]\n";
		gen->readfrom_spec(xmlframe);
		frame_generators()[id] = gen;
		DEBUG << "[PROCESSING XML_FRAME_SPEC][FINISHED]\n";
	}

	//Handle CALL sender
	for(auto sender_ : all_sender)
	{
		DEBUG << "[PROCESSING SENDER]\n";
		auto sender = sender_["sender"];
		auto when = sender["when"];
		auto emit = sender["emit"];
		auto transport  = sender["transport"];

		if (transport["generic_tcp_out"].empty() && 0==transport["use"].size()) {
			//Handle user defined transport layers
			std::string call_name="(NULL)";
			if (transport.nodes().size() && transport.nodes()[0]->kind() == ceps::ast::Ast_node_kind::structdef)
				call_name = ceps::ast::name(ceps::ast::as_struct_ref(transport.nodes()[0]));
			auto r = handle_userdefined_sender_definition(call_name,sender);
			
			if (!r) 
				fatal_(-1, "Sender definition: '"+call_name+"' unknown CAL-identifier (Communication Abstraction Layer).\n");
			continue;
		}

		std::string sock_name;
		std::string port;
		std::string ip;
		std::string channel_id;
		bool reuse_sock = false;
		bool reg_socket = false;

		//std::cout << when << std::endl;
		bool condition_defined = true;
		bool emit_defined = true;

		if (when.size() != 1 || when.nodes()[0]->kind() != ceps::ast::Ast_node_kind::symbol || "Event" != ceps::ast::kind(ceps::ast::as_symbol_ref( when.nodes()[0])) )
			condition_defined=false;
		if ( emit.size() != 1 || emit.nodes()[0]->kind() != ceps::ast::Ast_node_kind::identifier)
			emit_defined=false;

		if (transport["use"].size() )
		{
			if (transport["use"].nodes()[0]->kind() == ceps::ast::Ast_node_kind::identifier){
				reuse_sock = true;
				sock_name = ceps::ast::name(ceps::ast::as_id_ref(transport["use"].nodes()[0]));
			}
		} else {
			if (!transport["generic_tcp_out"]["port"].empty()) port = transport["generic_tcp_out"]["port"].as_str();
			if (!transport["generic_tcp_out"]["ip"].empty()) ip = transport["generic_tcp_out"]["ip"].as_str();
		}


        if (sender["id"].size()) {
        	auto p = sender["id"].nodes()[0];
        	if (p->kind() != ceps::ast::Ast_node_kind::identifier) fatal_(-1,"Id field in sender definition should contain a single id");
        	channel_id = sock_name = ceps::ast::name(ceps::ast::as_id_ref(p));
        	reg_socket = true;
        }


		std::string ev_id;
		if (condition_defined) ev_id = ceps::ast::name(ceps::ast::as_symbol_ref( when.nodes()[0]));
		std::string frame_id;
		if (emit_defined) frame_id = ceps::ast::name(ceps::ast::as_id_ref( emit.nodes()[0]));
		std::string eof="";
		std::string som="";

		if (transport["eom"].size()){
			auto  v = transport["eom"].nodes();
			//DOESN'T WORK!!!!!!: GCC BUG ??????: auto&  v = transport["generic_tcp_out"]["eof"].nodes();
			for(auto  e : v)
			{
				if (e->kind() == ceps::ast::Ast_node_kind::string_literal)
					eof.append(ceps::ast::value(ceps::ast::as_string_ref(e)));
				else if (e->kind() == ceps::ast::Ast_node_kind::int_literal && ceps::ast::value(ceps::ast::as_int_ref(e)) != 0)
					{eof.append(" "); eof[eof.length()-1] =  ceps::ast::value(ceps::ast::as_int_ref(e));}
			}
		}
		if (transport["som"].size()){
			auto  v = transport["som"].nodes();
			//DOESN'T WORK!!!!!!: GCC BUG ??????: auto&  v = transport["generic_tcp_out"]["eof"].nodes();
			for(auto  e : v)
			{
				if (e->kind() == ceps::ast::Ast_node_kind::string_literal)
					som.append(ceps::ast::value(ceps::ast::as_string_ref(e)));
				else if (e->kind() == ceps::ast::Ast_node_kind::int_literal && ceps::ast::value(ceps::ast::as_int_ref(e)) != 0)
					{som.append(" "); som[som.length()-1] =  ceps::ast::value(ceps::ast::as_int_ref(e));}
			}
		}



		if (condition_defined && emit_defined){
		 DEBUG << "[PROCESSING_EVENT_TRIGGERED_SENDER][ev_id="
			   << ev_id << "]"
			   <<"[frame_id="
			   << frame_id
			   <<"]"
			   <<"[ip="
			   << ip
			   << "]"
			   <<  "[port="
			   << port
			   <<"][eom='"<<eof<<"']"
			   <<"][som='"<<som<<"']"
			   <<"\n";

		 auto it = frame_generators().find(frame_id);
		 if (it == frame_generators().end()) fatal_(-1,"sender declaration: Unknown frame with id '"+frame_id+"'");
		 auto gen = it->second;
		 event_triggered_sender_t descr;
		 descr.event_id_ = ev_id;
		 descr.frame_gen_ = gen;
		 descr.frame_id_ = frame_id;
		 descr.frame_queue_ = new threadsafe_queue< std::tuple<char*,size_t,size_t>, std::queue<std::tuple<char*,size_t,size_t> >>;

		 if (start_comm_threads()){
		  comm_threads.push_back(new std::thread{comm_sender_generic_tcp_out_thread,descr.frame_queue_,this,ip,port,som,eof,sock_name,reuse_sock,reg_socket });
		  running_as_node() = true;
		 }

		 event_triggered_sender().push_back(descr);
		} else if (!condition_defined && !emit_defined && channel_id.length()){
			 DEBUG << "[PROCESSING_UNCONDITIONED_SENDER]"
				   <<"\n";
			 auto channel = new threadsafe_queue< std::tuple<char*,size_t,size_t>, std::queue<std::tuple<char*,size_t,size_t> >>;
			 this->set_out_channel(channel_id,channel);
			 if (start_comm_threads()){
			  running_as_node() = true;
			  comm_threads.push_back(
					 new std::thread{comm_sender_generic_tcp_out_thread,
				                     channel,
				                     this,
				                     ip,
				                     port,
				                     som,
				                     eof,
				                     sock_name,
				                     reuse_sock,
				                     reg_socket });
			 }
		} else {
            warn_(-1,"Sender definition incomplete, will be ignored.");
		}

	}
	//Handle CALL receiver
	for(auto receiver_ : all_receiver)
	{
		DEBUG << "[PROCESSING RECEIVER][START]\n";
		auto receiver = receiver_["receiver"];
		auto when = receiver["when"];
		auto emit = receiver["emit"];
		auto transport  = receiver["transport"];
		std::string port;
		std::string ip;
		bool reuse_sock=false;
		bool reg_sock = false;

		std::string sock_name;
		bool no_when_emit = false;

		if (transport["generic_tcp_in"].empty() && transport["use"].empty()) {
			//Handle user defined transport layers
			std::string call_name = "(NULL)";
			if (transport.nodes().size() && transport.nodes()[0]->kind() == ceps::ast::Ast_node_kind::structdef)
				call_name = ceps::ast::name(ceps::ast::as_struct_ref(transport.nodes()[0]));
			auto r = handle_userdefined_receiver_definition(call_name, receiver);

			if (!r)
				fatal_(-1, "Receiver definition: '" + call_name + "' unknown CAL-identifier (Communication Abstraction Layer).\n");
			continue;
		}


		if (when.empty() && emit.empty()) no_when_emit = true;
		else if (when.size() != 1 || when.nodes()[0]->kind() != ceps::ast::Ast_node_kind::identifier)
            {warn_(-1,"Illformed receiver definition."); continue;}

		if (transport["generic_tcp_in"].empty() && transport["use"].empty())
            {warn_(-1,"Illformed receiver definition."); continue;}

		if (!transport["use"].empty()){
			if (transport["use"].nodes()[0]->kind() == ceps::ast::Ast_node_kind::identifier){
					reuse_sock = true;
					sock_name = ceps::ast::name(ceps::ast::as_id_ref(transport["use"].nodes()[0]));
			} else fatal_(-1,"Receiver definition illformed");
		} else
		{
			if (!transport["generic_tcp_in"]["port"].empty()) port = transport["generic_tcp_in"]["port"].as_str();
			if (!transport["generic_tcp_in"]["ip"].empty()) ip = transport["generic_tcp_in"]["ip"].as_str();
		}

		std::string eof;
		std::string som;


		if (receiver["id"].size()) {
			auto p = receiver["id"].nodes()[0];
			if (p->kind() != ceps::ast::Ast_node_kind::identifier) fatal_(-1,"Id field in receiver definition should contain a single id");
			if (!reuse_sock){ sock_name = ceps::ast::name(ceps::ast::as_id_ref(p)); reg_sock = true; }
		}


		if (transport["eom"].size()) {
			//eof = transport["generic_tcp_in"]["eof"].as_str();
			auto  v = transport["eom"].nodes();
			//DOESN'T WORK!!!!!!: GCC BUG ??????: auto&  v = transport["generic_tcp_out"]["eof"].nodes();
			for(auto  e : v)
			{
				if (e->kind() == ceps::ast::Ast_node_kind::string_literal)
					eof.append(ceps::ast::value(ceps::ast::as_string_ref(e)));
				else if (e->kind() == ceps::ast::Ast_node_kind::int_literal && ceps::ast::value(ceps::ast::as_int_ref(e)) != 0)
					{eof.append(" "); eof[eof.length()-1] =  ceps::ast::value(ceps::ast::as_int_ref(e));}
			}
		}
		if (transport["som"].size()){
			auto  v = transport["som"].nodes();
			//DOESN'T WORK!!!!!!: GCC BUG ??????: auto&  v = transport["generic_tcp_out"]["eof"].nodes();
			for(auto  e : v)
			{
				if (e->kind() == ceps::ast::Ast_node_kind::string_literal)
					som.append(ceps::ast::value(ceps::ast::as_string_ref(e)));
				else if (e->kind() == ceps::ast::Ast_node_kind::int_literal && ceps::ast::value(ceps::ast::as_int_ref(e)) != 0)
					{som.append(" "); som[som.length()-1] =  ceps::ast::value(ceps::ast::as_int_ref(e));}
			}
		}

		std::string ev_id;
		std::vector<std::string> ev_params;

		if (emit.size() == 1 && emit.nodes()[0]->kind() == ceps::ast::Ast_node_kind::symbol && "Event" == ceps::ast::kind(ceps::ast::as_symbol_ref( emit.nodes()[0])) )
			ev_id = ceps::ast::name(ceps::ast::as_symbol_ref( emit.nodes()[0]));
		else if (emit.size() == 1 && emit.nodes()[0]->kind() == ceps::ast::Ast_node_kind::func_call)
		{
			std::vector<ceps::ast::Nodebase_ptr> args;
			read_func_call_values(this,	emit.nodes()[0],ev_id,args);
			for(auto p:args){
				if (p->kind() == ceps::ast::Ast_node_kind::identifier)
				{
					ev_params.push_back(ceps::ast::name(ceps::ast::as_id_ref(p)));
				}
			}
		}

		std::string frame_id;
		if (!when.nodes().empty()) frame_id = ceps::ast::name(ceps::ast::as_id_ref( when.nodes()[0]));
		if (!no_when_emit){
			DEBUG << "[PROCESSING EVENT_TRIGGERING_RECEIVER][ev_id="<< ev_id << "]"<<"[frame_id="<< frame_id <<"]" <<"[ip="<< ip << "]" <<  "[port=" << port <<"]" <<"\n";

			auto it = frame_generators().find(frame_id);

			if (it == frame_generators().end()) fatal_(-1,"receiver declaration: Unknown frame with id '"+frame_id+"'");

			auto gen = it->second;

			if (start_comm_threads()){
			 comm_threads.push_back(new std::thread{comm_generic_tcp_in_dispatcher_thread,
												   -1,
												   gen,
												   ev_id,
												   ev_params,
												   this,
												   ip,
												   port,
												   som,
												   eof,
												   sock_name,reg_sock,reuse_sock,
												   comm_generic_tcp_in_thread_fn });
			running_as_node() = true;
			}
		} else {
			DEBUG << "[PROCESSING_UNCONDITIONED_RECEIVER]" <<"[ip="<< ip << "]" <<  "[port=" << port <<"]" <<"\n";
			auto handlers = receiver[all{"on_msg"}];
			if (handlers.empty()) fatal_(-1,"on_msg required in receiver definition.");

			int dispatcher_id=-1;
			auto ctxt = allocate_dispatcher_thread_ctxt(dispatcher_id);
			DEBUG << "[PROCESSING_UNCONDITIONED_RECEIVER]" <<"[dispatcher_id="<< dispatcher_id << "]\n";

			for(auto const & handler_ : handlers){
				auto const & handler = handler_["on_msg"];
				auto frame_id_ = handler["frame_id"];
				auto handler_func_ = handler["handler"];

				if (frame_id_.size() != 1 || frame_id_.nodes()[0]->kind() != ceps::ast::Ast_node_kind::identifier)
					fatal_(-1,"Receiver definition illformed: frame_id not an identifier / wrong number of arguments.");
				if (handler_func_.size() != 1 || handler_func_.nodes()[0]->kind() != ceps::ast::Ast_node_kind::identifier)
					fatal_(-1,"Receiver definition illformed: handler not an identifier / wrong number of arguments.");

				auto frame_id = ceps::ast::name(ceps::ast::as_id_ref(frame_id_.nodes()[0]));
				auto handler_id = ceps::ast::name(ceps::ast::as_id_ref(handler_func_.nodes()[0]));
				auto it_frame = frame_generators().find(frame_id);
				if (it_frame == frame_generators().end()) fatal_(-1,"Receiver definition: on_msg : frame_id unknown.");
				auto it_func = global_funcs().find(handler_id);
				if (it_func == global_funcs().end()) fatal_(-1,"Receiver definition: on_msg : function unknown.");
				ctxt->handler.push_back(std::make_pair(it_frame->second,it_func->second));
			}
			if (start_comm_threads()){
			 comm_threads.push_back(new std::thread{comm_generic_tcp_in_dispatcher_thread,
												   dispatcher_id,
												   nullptr,
												   "",
												   ev_params,
												   this,
												   ip,
												   port,
												   som,
												   eof,
												   sock_name,reg_sock,reuse_sock,
												   comm_generic_tcp_in_thread_fn });
			 running_as_node() = true;
			}

		}
	}

	for(auto p: exported_events.nodes()){
	 if (p->kind() != ceps::ast::Ast_node_kind::symbol || ceps::ast::kind(ceps::ast::as_symbol_ref(p)) != "Event"){
	   std::stringstream ss;
	   ss << *p;
	   fatal_(-1,"export section: '"+ss.str()+"' is not an event.");
	  }
	  exported_events_.insert(ceps::ast::name(ceps::ast::as_symbol_ref(p)));
	}
	




#ifdef __gnu_linux__
	for(auto const & plugin_lib : result_cmd_line.plugins){
		void* handle = dlopen( plugin_lib.c_str(), RTLD_LAZY);
		if (handle == nullptr)
			fatal_(-1,dlerror());

		dlerror();
	    void* init_fn_ = dlsym(handle,"init_plugin");
	    if (init_fn_ == nullptr)
	    	fatal_(-1,dlerror());
	    auto init_fn = (init_plugin_t)init_fn_;
	    init_fn(this);
	}
#endif
#ifdef _WIN32
/*	for (auto const & plugin_lib : result_cmd_line.plugins) {
		auto handle = dlopen(plugin_lib.c_str(), RTLD_NOW);
		if (handle == nullptr)
			smc->fatal_(-1, dlerror());

		auto init_fn_ = dlsym(handle, "init_plugin");
		if (init_fn_ == nullptr)
			smc->fatal_(-1, dlerror());
		auto init_fn = (init_plugin_t)init_fn_;
		init_fn(smc, register_plugin);
	}*/
#endif

	if (logtrace()){
			std::map<std::string,int> map_fullqualified_sm_id_to_computed_idx;
			auto number_of_states = compute_state_and_event_ids(this,State_machine::statemachines,map_fullqualified_sm_id_to_computed_idx);
			std::string fout("mfqsmid.ceps");
			std::ofstream o(fout);
			if(!o) fatal_(-1,"Couldn't write '"+fout+"'");
			o << "map{\n";
			for(auto e : map_fullqualified_sm_id_to_computed_idx){
			 o <<" " << e.second <<";" << "\"" << e.first << "\";\n";
			}
			o << "};";

			DEBUG << "[LOG4CEPS][Mapping file'" << fout <<"' written.]\n";
			log4kmw::get_value<0>(log4kmw_states::Current_states) = log4kmw::Dynamic_bitset(number_of_states);
			log4kmw_loggers::logger_Trace.logger().init(log4kmw::persistence::memory_mapped_file("trace.bin", 1024*1024*8, true));
			DEBUG << "[LOG4CEPS][Trace log 'trace.bin' initialized.]\n";
	} else if (enforce_native()) {
	 std::map<std::string,int> map_fullqualified_sm_id_to_computed_idx;
	 auto number_of_states = compute_state_and_event_ids(this,State_machine::statemachines,map_fullqualified_sm_id_to_computed_idx);

	 if (result_cmd_line.print_transition_tables){
	  for(auto e : map_fullqualified_sm_id_to_computed_idx){
	   std::cout <<" " << e.second <<";" << "\"" << e.first << "\";";
	   if (executionloop_context().get_inf(e.second,executionloop_context_t::SM))std::cout <<" compound state";
	   if (executionloop_context().get_inf(e.second,executionloop_context_t::INIT))std::cout <<" initial state";
	   if (executionloop_context().get_inf(e.second,executionloop_context_t::FINAL))std::cout <<" final state";
	   if (executionloop_context().get_inf(e.second,executionloop_context_t::THREAD))std::cout <<" thread";
	   if (executionloop_context().get_inf(e.second,executionloop_context_t::JOIN)){
		   std::cout <<" join="<< executionloop_context().get_join_state(e.second);
	   }
	   if (executionloop_context().get_inf(e.second,executionloop_context_t::EXIT))std::cout <<" on_exit";
	   if (executionloop_context().get_inf(e.second,executionloop_context_t::ENTER))std::cout <<" on_enter";

	   std::cout << "\n";
	  }

	  for(auto e:executionloop_context().ev_to_id){
	   std::cout <<" " << e.second <<";" << "\"" << e.first;
 	   std::cout << "\";\n";
	  }
	  int j = 0;
	  for(auto const & t : executionloop_context().transitions){
	   std::cout << j++ << " ";
       std::cout << t.smp << ": ";
       if (t.start())
      	 std::cout << "START\n";
       else std::cout << t.from << "->"<<t.to<<" /"<<t.ev<<" g="<<t.guard << " a1= " << ((long long)t.a1) << " a2= "<< ((long long)t.a2) << std::endl;
	  }
	  for(auto const & s : executionloop_context().state_to_first_transition){
		std::cout << "first("<< s.first<<") == "<< s.second << std::endl;
	  }
	  for(int i = 1; i <= executionloop_context().number_of_states;++i){
	 	std::cout << "parent("<< i <<") == "<< executionloop_context().get_parent(i) << std::endl;
	  }
	  std::cout << "children-vector:\n";
	  for(auto s : executionloop_context().children ) {
		  std::cout << s <<";";
	  }
	  std::cout << "\n";
	 }//print transition tables
   }


	if(enforce_native()){
	 std::vector<State_machine*> smsv;
	 std::vector<std::string> r;
	 auto tt = this;

	 for(auto sm :State_machine::statemachines) smsv.push_back(sm.second);

	 traverse_sms(smsv,[&r,&tt](State_machine* cur_sm){
		for(auto it = cur_sm->actions().begin(); it != cur_sm->actions().end(); ++it) {
		 auto& act = *it;
		 if(act.body() != nullptr && act.native_func() == nullptr){
		  state_rep_t st;
		  st.is_sm_ = true;st.smp_=cur_sm;st.valid_=true;st.sid_=cur_sm->id();
		  r.push_back(tt->get_fullqualified_id(st)+"."+act.id());
		  }
		}
		for(auto const & t : cur_sm->transitions()){
		  for(auto it = t.actions().begin(); it != t.actions().end(); ++it) {
		    auto& act = *it;
		    if(act.body() != nullptr && act.native_func() == nullptr){
		      state_rep_t st;
		      st.is_sm_ = true;st.smp_=cur_sm;st.valid_=true;st.sid_=cur_sm->id();
		      r.push_back(tt->get_fullqualified_id(st)+"."+act.id()+" (action referred to in transition)");
		   }
		  }
		}
	    });

	 if(r.size()){
	  std::string s = "No native implementation for following state machines registered:\n";
	  for(auto & e:r){
	   s+= "  "; s+= e; s+="\n";
	  }
	  this->fatal_(-1,s);

	 }
	}
	if(generate_cpp_code()){
		do_generate_cpp_code(ceps_env_current(),current_universe(),global_guards,result_cmd_line);
	}

	auto simulations = ns[all{"Simulation"}];
	if (simulations.size())
	{
		for (auto simulation_ : simulations)
		{
			auto simulation = simulation_["Simulation"];
			process_simulation(simulation,ceps_env_current(),current_universe());
		}//for
	}
}
