#ifndef INC_CEPS_STATE_MACHINE_CORE_HPP
#define INC_CEPS_STATE_MACHINE_CORE_HPP

#define NO_SHORTCIRCUIT_EVENTQUEUE


#include <string> 
#include <vector>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <set>
#include <unordered_set>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>


#include "ceps_all.hh"
#include "core/include/state_machine.hpp"
#include "core/include/cmdline_utils.hpp"
#include "core/include/sm_ev_comm_layer.hpp"
#include "core/include/sm_raw_frame.hpp"
#include "core/include/state_machine_simulation_core_reg_fun.hpp"
#include "core/include/state_machine_simulation_core_plugin_interface.hpp"
#include "core/include/events.hpp"


#include "log4kmw_state.hpp"
#include "log4kmw_record.hpp"
#include "log4kmw_logger.hpp"
#include "log4kmw_states.hpp"
#include "log4kmw_records.hpp"
#include "log4kmw_loggers.hpp"


namespace log4ceps = log4kmw;
namespace log4cepsloggers = log4kmw_loggers;



template<typename T, typename Q> class threadsafe_queue{
	Q data_;
	mutable std::mutex m_;
	std::condition_variable cv_;
public:
	void push(T const & elem){
		std::lock_guard<std::mutex> lk(m_);
		data_.push(elem);
		cv_.notify_one();
	}

	void wait_and_pop(T & elem){
		std::unique_lock<std::mutex> lk(m_);
		cv_.wait(lk, [this]{return !data_.empty(); });
		elem = data_.front();
		data_.pop();
	}

	void wait_for_data(){
		std::unique_lock<std::mutex> lk(m_);
		cv_.wait(lk, [this]{return !data_.empty(); });
	}

	Q& data() {return data_;}
	std::mutex& data_mutex() const {return m_;}
};

namespace std{
template <>
 struct hash<state_rep_t>
 {
   std::size_t operator()(const state_rep_t& k) const
   {

     return ((hash<string>()(k.sid_)
              ^ (hash<void*>()(k.smp_) << 1)) >> 1)
              ;
   }
 };
}

class State_machine_simulation_core;

class executionloop_context_t{
public:

        //using state_rep_t = std::uint16_t;
        using state_rep_t = int;
        using state_present_rep_t = std::uint8_t;

	executionloop_context_t(){
		ev_sync_queue.resize(1024);
	}

	void push_ev_sync_queue(int ev_id){
		ev_sync_queue[ev_sync_queue_end] = ev_id;
		++ev_sync_queue_end;
		if (ev_sync_queue_end == ev_sync_queue.size()) ev_sync_queue_end = 0;
		if (ev_sync_queue_end == ev_sync_queue_start) {
			++ev_sync_queue_start;
			if (ev_sync_queue_start == ev_sync_queue.size()) ev_sync_queue_start = 0;
		}
	}

	int front_ev_sync_queue(){
		return ev_sync_queue[ev_sync_queue_start];
	}

	bool ev_sync_queue_empty() {
		return ev_sync_queue_start == ev_sync_queue_end;
	}

	bool pop_ev_sync_queue(){
		if (ev_sync_queue_empty()) return false;
		++ev_sync_queue_start;
		if (ev_sync_queue_start == ev_sync_queue.size())
			ev_sync_queue_start = 0;
	}

	int get_parent(int state){
		return parent_vec[state];
	}

	void set_parent(int child, int state){
		if ((size_t)number_of_states > parent_vec.size())
			parent_vec.resize(number_of_states+1,0);
		parent_vec[child] = state;
	}

	void current_states_init_and_clear(){
		if (current_states.size() != (size_t)number_of_states+1) current_states.resize(number_of_states+1);
                std::memset(current_states.data(),0,(number_of_states+1)*sizeof(state_present_rep_t));
	}

	void set_inf(int state,unsigned int what,bool value){
		if ((size_t)number_of_states > inf_vec.size())
			inf_vec.resize(number_of_states+1,0);
		if (value) inf_vec[state] |=  1 << what;
		else inf_vec[state] &=  ~ (1 << what);
	}

	bool get_inf(int state, unsigned int what){
		unsigned int r = (inf_vec[state] & (1 << what));
		return r != 0;
	}

	void set_on_enter(int state,void(*fn)()){
		if ((size_t)number_of_states > on_enter.size())
			on_enter.resize(number_of_states+1,0);
		on_enter[state] = fn;
	}
	void set_on_exit(int state,void(*fn)()){
		if ((size_t)number_of_states > on_exit.size())
			on_exit.resize(number_of_states+1,0);
		on_exit[state] = fn;
	}

	void set_initial_state(int state,int initial){
		if ((size_t)number_of_states > initial_state.size())
			initial_state.resize(number_of_states+1,0);
		initial_state[state] = initial;
	}
	void set_final_state(int state,int final){
		if ((size_t)number_of_states > final_state.size())
			final_state.resize(number_of_states+1,0);
		final_state[state] = final;
	}

	void init_state_to_children(){
		state_to_children.resize(number_of_states+1);
	}

	bool is_sm(int state){
		return get_inf(state, SM);
	}

        void do_enter_impl(State_machine_simulation_core*,int sms,std::vector<executionloop_context_t::state_present_rep_t> const & v);
        void do_enter(State_machine_simulation_core*,int* sms,int n,std::vector<executionloop_context_t::state_present_rep_t> const & v);
        void do_exit_impl(State_machine_simulation_core* smc,int sms,std::vector<executionloop_context_t::state_present_rep_t> const & v);
        void do_exit(State_machine_simulation_core* smc,int* sms,int n,std::vector<executionloop_context_t::state_present_rep_t> const & v);

        void remove_children(int sms,std::vector<executionloop_context_t::state_present_rep_t> & v){
		auto child_idx = state_to_children[sms]+1;
                for(int child;(child = children[child_idx]);++child_idx){
			v[child] = 0;
			set_inf(child,VISITED,true);
			if (!is_sm(child)) continue;
			remove_children(child,v);
		}
	}

        bool empty(int sms,std::vector<executionloop_context_t::state_present_rep_t> const & v ){
	  auto child_idx = state_to_children[sms]+1;
	  bool active_sub_states = false;
          for(int child;(child=children[child_idx]);++child_idx){
		 if (v[child]){active_sub_states = true;break;}
	  }
	  if (active_sub_states) return false;
	  return true;
	}

	void set_assoc_sm(int state,State_machine* smp){
		if ((size_t)number_of_states > assoc_sm.size())
			assoc_sm.resize(number_of_states+1,0);
		assoc_sm[state] = smp;
	}

	State_machine* get_assoc_sm(int state){
		if ((size_t)number_of_states > assoc_sm.size())
			assoc_sm.resize(number_of_states+1,0);
		return assoc_sm[state];
	}


	void set_join_state(int state,int join){
		if ((size_t)number_of_states > join_states.size())
			join_states.resize(number_of_states+1,0);
		join_states[state] = join;
	}

	int get_join_state(int state){
		if ((size_t)number_of_states > join_states.size())
			join_states.resize(number_of_states+1,0);
		return join_states[state];
	}

	int number_of_states = 0;
	int current_ev_id = 0;
	std::map<std::string,int> ev_to_id;
	std::map<int,std::string> id_to_ev;
	class transition_t{
		public:
		int smp = 0,from = 0, to = 0, ev = 0;
		bool(**guard)() = nullptr;
		void(* a1)() = nullptr;
		void(* a2)() = nullptr;
		void(* a3)() = nullptr;
		transition_t() = default;
		bool start() const {return smp != 0 && from == 0 && to == 0;}
	};
	std::vector<transition_t> transitions;
        std::vector<state_present_rep_t> current_states;
	std::unordered_map<int,int> state_to_first_transition;
	std::vector<int> ev_sync_queue;
	std::vector<int> parent_vec;
	std::vector<int> inf_vec;
	std::vector<void(*)()> on_enter;
	std::vector<void(*)()> on_exit;
	std::vector<int> initial_state;
	std::vector<int> final_state;
	std::vector<int> children;
	std::vector<int> state_to_children;
	std::vector<int> join_states;
	std::map<std::string,int> state_id_to_idx;
	std::map<int,std::string> idx_to_state_id;
	std::unordered_set<int> exported_events;
	std::vector<State_machine*> assoc_sm;

	static constexpr unsigned int INIT = 0;
	static constexpr unsigned int FINAL = 1;
	static constexpr unsigned int SM = 2;
	static constexpr unsigned int EXIT = 3;
	static constexpr unsigned int ENTER = 4;
	static constexpr unsigned int VISITED = 5;
	static constexpr unsigned int VISITING = 6;
	static constexpr unsigned int THREAD = 7;
	static constexpr unsigned int JOIN = 8;
	static constexpr unsigned int IN_THREAD = 9;
	static constexpr unsigned int REGION = 10;



	size_t ev_sync_queue_start = 0;
	size_t ev_sync_queue_end = 0;
};



class State_machine_simulation_core:public IUserdefined_function_registry, Ism4ceps_plugin_interface
{
public:
	typedef std::chrono::steady_clock clock_type;
	using states_t = std::vector<state_rep_t>;
private:
	ceps::ast::Nodeset*	current_universe_ = nullptr;
	ceps::Ceps_Environment * ceps_env_current_ = nullptr;
	std::map<std::string, ceps::ast::Nodebase_ptr> global_guards;
	std::map<std::string, ceps::ast::Nodebase_ptr> global_states_;
	std::map<std::string, ceps::ast::Nodebase_ptr> global_states_prev_;

	using Logger_active_states = std::vector<int>;
	using Logger_entry_counter = int;

	using Activestates_logentry = log4ceps::State_record <Logger_entry_counter,Logger_active_states>;

	log4ceps::Logger<Activestates_logentry, log4ceps::persistence::in_memory>* active_states_logger_ = nullptr;
	int active_states_logger_ctr_ = 0;



	bool quiet_mode_= false;
	bool shutdown_threads_ = false;
	mutable std::recursive_mutex glob_states_mutex_;
	mutable std::recursive_mutex active_states_logger_mutex_;
	states_t current_states_;

	std::map<std::string,ceps::ast::Nodebase_ptr> global_funcs_;
	std::map<std::string,Rawframe_generator*> frame_generators_;

	ceps::ast::Nodeset	post_event_processing_;

	std::set<state_rep_t> remove_states_;
	bool conf_ignore_unresolved_state_id_in_directives_ = false;


	std::set<std::string> unique_events_;
	std::set<std::string>& unique_events(){return unique_events_;}
	std::set<std::string> not_transitional_events_;
	std::set<std::string>& not_transitional_events(){return not_transitional_events_;}
	std::vector< std::pair< void (*) (states_t const&,State_machine_simulation_core*,void *), void * > > states_vistors;
	void call_states_visitors() {
		for(auto & e : states_vistors) e.first(current_states_,this,e.second);
	}
	bool logtrace_ = false;
	bool start_comm_threads_ = true;
	State_machine* current_smp_ = nullptr;
	std::map<std::string,sm4ceps_plugin_int::glob_handler_t> glob_funcs_;
    std::unordered_set<std::string> exported_events_;
    bool map_ceps_payload_to_native_=false;
    bool delete_ceps_payload_=false;
    bool enforce_native_ = false;
    executionloop_context_t executionloop_context_;

public:

    decltype(executionloop_context_)& executionloop_context() {return executionloop_context_;}
    bool& enforce_native(){return enforce_native_;}
    bool is_export_event(std::string const & ev_id) const {return exported_events_.find(ev_id) != exported_events_.end();}

	decltype(glob_funcs_)& glob_funcs(){return glob_funcs_;}
	decltype(current_smp_)& current_smp(){return current_smp_;}
	decltype(current_smp_) current_smp() const {return current_smp_;}

	bool& start_comm_threads(){return start_comm_threads_;}
	bool start_comm_threads() const {return start_comm_threads_;}

	bool& logtrace() {return logtrace_;}
	bool logtrace() const {return logtrace_;}

	void reg_states_visitor(void (*f) (states_t const&,State_machine_simulation_core*,void *), void * tag){
		states_vistors.push_back(std::make_pair(f,tag));
	}
	log4ceps::Logger<Activestates_logentry, log4ceps::persistence::in_memory>* set_active_states_logger(log4ceps::Logger<Activestates_logentry, log4ceps::persistence::in_memory>* l){
		auto t = active_states_logger_;active_states_logger_ = l;
		return t;
	}

	log4ceps::Logger<Activestates_logentry, log4ceps::persistence::in_memory>* active_states_logger() const {return active_states_logger_;}

	std::recursive_mutex& active_states_logger_mutex() const {return active_states_logger_mutex_;}

	bool& conf_ignore_unresolved_state_id_in_directives() {return conf_ignore_unresolved_state_id_in_directives_;}

	ceps::ast::Nodeset&	post_event_processing(){return post_event_processing_;}


	size_t timeout_ = 0;



	//std::map<std::string, ceps::ast::Nodebase_ptr>& global_systemstates(){return global_states_;}
	std::map<std::string, ceps::ast::Nodebase_ptr>& global_systemstates_prev(){return global_states_prev_;}


	std::map<std::string,ceps::ast::Nodebase_ptr>& global_funcs() {return global_funcs_;}
	std::map<std::string,ceps::ast::Nodebase_ptr> const & global_funcs() const {return global_funcs_;}

	std::map<std::string, Rawframe_generator*>&  frame_generators() {return frame_generators_;}
	std::map<std::string, Rawframe_generator*> const &  frame_generators() const {return frame_generators_;}

	std::recursive_mutex& states_mutex() {return glob_states_mutex_;}

	states_t & current_states(){return current_states_;}

	bool shutdown(){return shutdown_threads_;}
	//void lock_global_states() const {states_mutex_.lock();}
	//void unlock_global_states() const {states_mutex_.unlock();}
	std::map<std::string, ceps::ast::Nodebase_ptr> & get_global_states() {return global_states_;}
	bool& quiet_mode(){return quiet_mode_;}
	bool quiet_mode() const {return quiet_mode_;}
	struct event_t {
		std::string id_;
		bool already_sent_to_out_queues_ = false;
		std::vector<ceps::ast::Nodebase_ptr> payload_;
		std::vector<sm4ceps_plugin_int::Variant> payload_native_;
		bool unique_= false;
		sm4ceps_plugin_int::Framecontext* frmctxt_ = nullptr;
		sm4ceps_plugin_int::Variant (*glob_func_)()  = nullptr;
		event_t() = default;
		event_t(const event_t &) = default;
		event_t& operator = (const event_t &) = default;
		event_t(std::string const & id,std::vector<ceps::ast::Nodebase_ptr> const & payload = {}):id_(id),payload_(payload) {}
		bool operator < (event_t const & rhs) const {
			return id_ < rhs.id_;
		}
		bool is_epsilon() const {return id_ == "";}
		event_t& operator = (event_rep_t const & rhs) { id_=rhs.sid_;payload_=rhs.payload_;payload_native_=rhs.payload_native_;return *this;}
	};
#ifdef __gnu_linux__
	struct timer_table_entry_t{
		bool in_use = false;
		bool kill = false;
		bool fresh = false;
		std::string name;
		int id;
		long period_in_ms = 0;
		long time_remaining_in_ms = 0;
		bool periodic = false;
		int fd = -1;
		event_t event;
	};

	size_t timer_table_size = 128;

	mutable std::mutex timer_table_mtx;
	std::vector<timer_table_entry_t> timer_table;
#endif

	typedef ceps::ast::Nodebase_ptr (*smcore_plugin_fn_t)(ceps::ast::Call_parameters* params);

	void register_plugin_fn(std::string const & id, smcore_plugin_fn_t fn);

	typedef void (*global_event_call_back_fn)(event_t);
	global_event_call_back_fn global_event_call_back_fn_ = nullptr;
	void set_global_event_call_back(global_event_call_back_fn fn){global_event_call_back_fn_ = fn;}



private:

	std::map<std::string,smcore_plugin_fn_t> name_to_smcore_plugin_fn;

	using main_event_queue_t = threadsafe_queue<event_t, sm4ceps::Eventqueue<event_t> /*std::queue<event_t>*/ >;
	using out_event_queue_t  = threadsafe_queue<event_t, std::queue<event_t> >;
	using out_event_queues_t = std::vector<out_event_queue_t*>;

	mutable std::mutex out_event_queues_m_;
	main_event_queue_t main_event_queue_;
	out_event_queues_t out_event_queues_;


	struct Log{
		std::ostream* os_p = nullptr;
		State_machine_simulation_core* parent_ = nullptr;
		Log() = default;
		Log(State_machine_simulation_core* parent):parent_(parent){}
		std::mutex m_;
		template <typename T> Log& operator << (T const & v)
		{
			if (parent_->quiet_mode()) return *this;
			std::lock_guard<std::mutex> print_lock(m_);
			if (os_p) { *os_p << v;}
			return *this;
		}
	};

	Log std_log_;

	std::map<std::string,threadsafe_queue< std::tuple<char*,size_t,size_t>, std::queue<std::tuple<char*,size_t,size_t> >>* > id_to_out_chan_;

public:

	threadsafe_queue< std::tuple<char*,size_t,size_t>, std::queue<std::tuple<char*,size_t,size_t> >>* get_out_channel(std::string const & s){
		return id_to_out_chan_[s];
	}

	void set_out_channel(std::string const & s, threadsafe_queue< std::tuple<char*,size_t,size_t>, std::queue<std::tuple<char*,size_t,size_t> >>* ch){
		id_to_out_chan_[s] = ch;
	}

	using type_definitions_t = std::map<std::string, ceps::ast::Struct_ptr>;
	type_definitions_t type_definitions_;
	type_definitions_t const & type_definitions() const {return type_definitions_;}
	type_definitions_t & type_definitions() {return type_definitions_;}
	state_rep_t resolve_state_qualified_id(ceps::ast::Nodebase_ptr p, State_machine* parent);
	state_rep_t resolve_state_qualified_id(std::string compound_id, State_machine* parent);

private:



	void process_statemachine_helper_handle_transitions(
						State_machine* current_statemachine,
						std::vector<State_machine::Transition>& trans,
						std::string id,
						ceps::ast::Nodeset& transitions,int& guard_ctr);

	void process_statemachine(	ceps::ast::Nodeset& sm_definition,
								std::string prefix,
								State_machine* parent,
								int depth,
								int thread_ctr,
								bool is_thread = false);
	event_rep_t resolve_event_qualified_id(ceps::ast::Nodebase_ptr p, State_machine* parent);
	void process_simulation(ceps::ast::Nodeset& sim,ceps::Ceps_Environment& ceps_env,ceps::ast::Nodeset& universe);
	void eval_guard_assign(ceps::ast::Binary_operator & root);
	void eval_state_assign(ceps::ast::Binary_operator & root,std::string const &);
	void add(states_t& states, state_rep_t s);
	void print_info(states_t& states_from, states_t& states_to,std::set<state_rep_t>const & new_states_triggered_set,std::set<state_rep_t> const& );
	void print_info(states_t const& states);
	void trans_hull_of_containment_rel(states_t& states_in,states_t& states_out);
	bool eval_to_bool(ceps::ast::Nodebase_ptr p);
	bool compute_successor_states_kernel_under_event(event_rep_t ev,
													 states_t& states,
													 std::map<state_rep_t,state_rep_t>& pred,
													 states_t& states_without_transition,
													 ceps::Ceps_Environment& ceps_env,
													 ceps::ast::Nodeset universe,
													 std::map<state_rep_t,std::vector<State_machine::Transition::Action> >& associated_actions,
													 std::set<state_rep_t> & remove_states,
													 std::set<state_rep_t>& removed_states);

public:
	std::string get_fullqualified_id(state_rep_t const & s,std::string delim = ".");
	std::string get_full_qualified_id(State_machine::State const& s);
	bool is_assignment_op(ceps::ast::Nodebase_ptr n);
	bool is_assignment_to_guard(ceps::ast::Binary_operator & node);
	bool is_assignment_to_state(ceps::ast::Binary_operator & node,std::string& lhs_id);

	Log& log(){return std_log_;}


	ceps::ast::Nodebase_ptr ceps_interface_eval_func(State_machine* active_smp,std::string const & , ceps::ast::Call_parameters*);
private:
	bool (*on_empty_event_queue_handler_)(std::string&) =  nullptr;
	bool (*on_queued_event_handler_)(std::string const &)  = nullptr;
	bool (*step_handler_ )() = nullptr;

	bool resolve_imports(State_machine & );
	void guards_in_expr(ceps::ast::Nodebase_ptr  expr, std::set<std::string> & v);
	ceps::ast::Nodebase_ptr unfold(ceps::ast::Nodebase_ptr expr,
								   std::map<std::string, ceps::ast::Nodebase_ptr>& guard_to_interpretation,
								   std::set<std::string>& path,states_t const & states);
	bool eval_guard(ceps::Ceps_Environment& ceps_env,std::string const & guard_name,std::vector<state_rep_t> const& current_states );
	bool contains_sm_func_calls(ceps::ast::Nodebase_ptr  expr);
	void update_asserts(states_t const & reached_states);


public:
	int timed_events_active_ = 0;
	void enqueue_event(event_t ev,bool update_out_queues = false);
	void inc_timed_events() {++timed_events_active_;}
	void dec_timed_events() {--timed_events_active_;}
	bool timed_events_pending() {return timed_events_active_;}

	clock_type clock_;
	clock_type::time_point base_time_point_;

	struct assert_t
	{
		enum type{ALWAYS,NEVER,EVENTUALLY,ASSERT};
		enum quantifier{NONE,ALL,AT_LEAST_ONE,EXACTLY_ONE};
		type type_;
		quantifier quantifier_;
		std::string id_;
		std::string description_;
		bool satisfied_ = false;
		states_t states_;
		std::vector<assert_t> triggered_asserts_;
		std::vector<ceps::ast::Nodebase_ptr> guards_;
	};
	std::vector<assert_t> active_asserts_;
	std::map<State_machine*,std::string> sm_to_id_;

	bool running_as_node_ = false;


	void process_event_from_remote(nmp_header,char* data);
	void exec_action_timer(std::vector<ceps::ast::Nodebase_ptr>& args,bool);
	bool exec_action_timer(double,sm4ceps_plugin_int::ev,sm4ceps_plugin_int::id,bool periodic_timer,sm4ceps_plugin_int::Variant (*fp)() = nullptr);
	bool is_global_event(std::string const & ev_name);


	struct event_triggered_sender_t{
		std::string event_id_;
		std::string frame_id_;
		Rawframe_generator* frame_gen_;
		threadsafe_queue< std::tuple<char*,size_t,size_t>, std::queue<std::tuple<char*,size_t,size_t> >>* frame_queue_;
		std::thread* thread_;
	};
	std::vector<event_triggered_sender_t>event_triggered_sender_;

public:
	std::vector<event_triggered_sender_t>& event_triggered_sender(){return event_triggered_sender_;};
	bool running_as_node() const {return running_as_node_;}
	bool& running_as_node() {return running_as_node_;}
	bool print_debug_info_ = false;
	static int SM_COUNTER;

	main_event_queue_t& main_event_queue() {return main_event_queue_;}

	out_event_queue_t* out_event_queue(int idx) {
		std::lock_guard<std::mutex> lk(out_event_queues_m_);
		if (idx >= 0 && (size_t)idx >= out_event_queues_.size()) return nullptr; return out_event_queues_[(size_t)idx];
	}
	int allocate_out_event_queue(){
		std::lock_guard<std::mutex> lk(out_event_queues_m_);
		out_event_queues_.push_back(new out_event_queue_t);
		return out_event_queues_.size()-1;
	}


	void (*fatal_)(int, std::string const & ) = nullptr;
	void (*warn_)(int, std::string const & ) = nullptr;

	std::pair<bool,std::string> get_qualified_id(State_machine* sm) {
		auto it = sm_to_id_.find(sm);
		if (it == sm_to_id_.end()) return std::make_pair(false,"");
		return std::make_pair(true,it->second);
	}
	void set_qualified_id(State_machine* sm,std::string id){sm_to_id_[sm] = id;}
	void start_processing_init_script(ceps::ast::Nodeset& sim,int& pos,states_t states);


	State_machine_simulation_core(std::string const & prelude = {})
	{
		ceps_env_current_ = new ceps::Ceps_Environment (prelude);
		current_universe_ = new ceps::ast::Nodeset();
		std_log_.parent_ = this;
	}

	void reset_universe() {current_universe_ = new ceps::ast::Nodeset();}

	void set_log_stream(std::ostream* os_p) {std_log_.os_p = os_p; }
	ceps::ast::Nodeset& current_universe() const {return *current_universe_;}
	ceps::Ceps_Environment& ceps_env_current() const {return *ceps_env_current_;}

	void set_fatal_error_handler( void (*fatal)(int, std::string const & )   ) {fatal_ = fatal;}
	void set_non_fatal_error_handler( void (*warn)(int, std::string const & )   ) {warn_ = warn;}


	void set_on_empty_event_queue_handler( bool (*handler)(std::string&) ) {on_empty_event_queue_handler_ = handler;}
	void set_on_queued_event_handler( bool (*handler)(std::string const &) ) {on_queued_event_handler_ = handler;}
	void set_step_handler (bool (*handler) ()) {step_handler_ = handler ;}


	static const int ERR_FILE_OPEN_FAILED = 0;
	static const int ERR_CEPS_PARSER      = 1;
	static const int WARN_XML_PROPERTIES_MISSING_PREFIX_DEFINITION = 100;
	static const int WARN_CANNOT_FIND_TEMPLATE = 101;
	static const int WARN_NO_INVOCATION_MAPPING_AND_NO_TABLE_DEF_FOUND = 103;
	static const int WARN_TESTCASE_ID_ALREADY_DEFINED = 104;
	static const int WARN_TESTCASE_EMPTY = 105;
	static const int WARN_NO_STATEMACHINES = 106;

	/*
	Iterates through a vector of file names, evaluates each file with ceps, appends resulting node set to the 'universe'.
	Node set resulting from evaluation is supplemented with internal variables (like paths).
	*/
	void process_files(	std::vector<std::string> const & file_names,
						std::string& last_file_processed);
	void processs_content(Result_process_cmd_line const& result_cmd_line,State_machine **entry_machine = nullptr);
	void leave_sm(State_machine* smp,states_t & states,std::set<State_machine*>& sms_exited,std::vector<State_machine*>& on_exit_seq);
	void enter_sm(	bool triggered_by_immediate_child_state, /*true <=> state which triggered enter is a pure state => No automatic trigger of Initial*/
					State_machine* smp,
					std::set<State_machine*>& sms_entered,
					std::vector<State_machine*>& on_enter_seq,
					std::set<state_rep_t>& new_states_set,
					std::vector<State_machine::Transition::Action>& on_enter_sm_derived_action_list,
					states_t const& current_states);
	ceps::ast::Nodebase_ptr execute_action_seq(State_machine* containing_smp,ceps::ast::Nodebase_ptr ac_seq);
	bool fetch_event(event_rep_t& ev,ceps::ast::Nodeset& sim,int& pos,states_t& states,
			bool& states_updated, std::vector<State_machine*>& on_enter_seq,bool ignore_handler = true, bool ignore_ev_queue =  false,bool exit_if_start_found = false);
	void simulate(ceps::ast::Nodeset sim,states_t& states_in,ceps::Ceps_Environment& ceps_env,ceps::ast::Nodeset& universe);
	void simulate_purly_native(ceps::ast::Nodeset sim,
			                                     states_t& states_in,
			                                     ceps::Ceps_Environment& ceps_env,
			                                     ceps::ast::Nodeset& universe);
	bool print_debug_info(bool b) {bool t = print_debug_info_; print_debug_info_ = b;return t;}
	bool resolve_q_id(State_machine* smp, std::vector<std::string> const & q_id, State_machine::State & s);
	bool kill_named_timer(std::string const & timer_id);
        bool kill_named_timer_main_timer_table(std::string const & timer_id);


	std::map<std::string, ceps::ast::Nodebase_ptr>&  guards(){return global_guards;}
	//std::map<std::string, ceps::ast::Nodebase_ptr>&  states(){return global_states_;}

	event_t current_event_;
	event_t& current_event() {return current_event_;}

	friend void run_state_machine_simulation(State_machine_simulation_core* smc,Result_process_cmd_line const& result_cmd_line);

	struct dispatcher_thread_ctxt_t{
		 std::vector<std::pair<Rawframe_generator*,ceps::ast::Nodebase_ptr>> handler;
		 std::vector<sm4ceps_plugin_int::Framecontext*> native_handler;
		 std::string id_;
		 std::mutex m_;
		 std::condition_variable cv_;
		 bool start_ = false;
		 std::string& id(){return id_;}
		 std::string const & id() const {return id_;}
		 dispatcher_thread_ctxt_t() = default;

		 void request_start() {
			std::lock_guard<std::mutex> lk(m_);
			start_ = true;
			cv_.notify_one();
		 }
		 void wait_for_start_request(){
			 std::unique_lock<std::mutex> lk(m_);
		 	 cv_.wait(lk, [this]{return start_; });
		 }
		 decltype(native_handler)& get_native_handler() {return native_handler;}
	};
private:
	std::map<std::string,int> registered_sockets_;
	std::recursive_mutex registered_sockets_mtx_;
	std::set<state_rep_t> assert_not_in_end_states_;
	std::set<state_rep_t> assert_in_end_states_;
	std::vector<dispatcher_thread_ctxt_t*> dispatcher_thread_ctxt_;
	bool generate_cpp_code_ = false;

	std::vector<std::pair<bool,event_t>> ev_short_circuit_vec_;
	size_t free_entries_in_short_circuit_vec_ = 32;


public:
	bool generate_cpp_code() const {return generate_cpp_code_;}
	bool& generate_cpp_code() {return generate_cpp_code_;}

	std::recursive_mutex& get_reg_sock_mtx(){return registered_sockets_mtx_;}
	std::map<std::string,int>& get_reg_socks(){return registered_sockets_;}
	dispatcher_thread_ctxt_t* allocate_dispatcher_thread_ctxt(int & i) {
		i = dispatcher_thread_ctxt_.size();dispatcher_thread_ctxt_.push_back(new dispatcher_thread_ctxt_t{});
		return dispatcher_thread_ctxt_[dispatcher_thread_ctxt_.size()-1];
	}
	dispatcher_thread_ctxt_t* get_dispatcher_thread_ctxt(int i) {return dispatcher_thread_ctxt_[i];}
	dispatcher_thread_ctxt_t* get_dispatcher_thread_ctxt(std::string name) {
		//std::cout << "lookup "<< name << std::endl;
		for(auto  e: dispatcher_thread_ctxt_)
			if (e->id() == name) return e;
		return nullptr;
	}
	void request_start_for_all_dispatchers(){
		for(auto & e: dispatcher_thread_ctxt_) e->request_start();
	}

	void do_generate_cpp_code(ceps::Ceps_Environment& ceps_env,ceps::ast::Nodeset& universe,std::map<std::string, ceps::ast::Nodebase_ptr> const & all_guards,Result_process_cmd_line const& result_cmd_line);

	//CAL (Sender)

	bool handle_userdefined_sender_definition(std::string call_name, ceps::ast::Nodeset const & ns);
	bool handle_userdefined_receiver_definition(std::string call_name, ceps::ast::Nodeset const & ns);

	bool register_action(std::string state_machine_id,std::string action, void(*fn)());
	bool register_action_impl(std::string state_machine_id,std::string action, void(*fn)(),State_machine* parent);

	using global_init_t = void (*)();
	global_init_t global_init = nullptr;
	global_init_t & user_supplied_global_init() {return global_init;}

	virtual bool register_global_init(global_init_t fn){
		user_supplied_global_init() = fn;
		return true;
	}

	std::unordered_map<std::string,bool(**)()> user_supplied_guards;

	virtual bool register_guard(std::string name,bool(**fn)()){
		user_supplied_guards[name] = fn;
		return true;
	}

	decltype(user_supplied_guards)& get_user_supplied_guards() {return user_supplied_guards;}
	decltype(user_supplied_guards) const & get_user_supplied_guards() const {return user_supplied_guards;}




	//Userdefined functions (will be replaced by variadic templates)

	void regfn(std::string name, int(*fn) ());
	void regfn(std::string name, double(*fn) ());
	void regfn(std::string name, int (*fn) (int) );
	void regfn(std::string name, double (*fn) (int));
	void regfn(std::string name, int (*fn) (double));
	void regfn(std::string name, double (*fn) (double));
	void regfn(std::string name, int(*fn) (int,int));
	void regfn(std::string name, double(*fn) (int,int));
	void regfn(std::string name, int(*fn) (double,int));
	void regfn(std::string name, double(*fn) (double, int));
	void regfn(std::string name, int(*fn) (int, double));
	void regfn(std::string name, double(*fn) (int, double));
	void regfn(std::string name, int(*fn) (double, double));
	void regfn(std::string name, double(*fn) (double, double));
	void regfn(std::string name, int(*fn) (std::string));
	void regfn(std::string name, std::string(*fn) (std::string));
	void regfn(std::string name, int(*fn) (int,int,int,int,int,int));

	Ism4ceps_plugin_interface* get_plugin_interface() {return this;}
	void queue_event(std::string ev_name,std::initializer_list<sm4ceps_plugin_int::Variant> vl = {});
	void sync_queue_event(int ev_id);
	size_t argc();
	sm4ceps_plugin_int::Variant argv(size_t);
	void start_timer(double,sm4ceps_plugin_int::ev);
	void start_timer(double,sm4ceps_plugin_int::ev,sm4ceps_plugin_int::id);
	void start_periodic_timer(double,sm4ceps_plugin_int::ev);
	void start_periodic_timer(double,sm4ceps_plugin_int::ev,sm4ceps_plugin_int::id);
	void start_periodic_timer(double,sm4ceps_plugin_int::Variant (*fp)());
	void start_periodic_timer(double,sm4ceps_plugin_int::Variant (*fp)(),sm4ceps_plugin_int::id);
	void stop_timer(sm4ceps_plugin_int::id);
	void send_raw_frame(void*,size_t,size_t,std::string const &);
	void register_frame_ctxt(sm4ceps_plugin_int::Framecontext* ctxt, std::string receiver_id);
	bool in_state(std::initializer_list<sm4ceps_plugin_int::id> state_ids);
	void register_global_function(std::string name,sm4ceps_plugin_int::Variant (*fp)());


	private:
		std::map<std::string, int(*) ()> regfntbl_i_;
		std::map<std::string, double(*) ()> regfntbl_d_;
		std::map<std::string, int(*) (int)> regfntbl_ii_;
		std::map<std::string, double(*) (int)> regfntbl_di_;
		std::map<std::string, int(*) (double)> regfntbl_id_;
		std::map<std::string, double(*) (double)> regfntbl_dd_;

		std::map<std::string, int(*) (int,int)> regfntbl_iii_;
		std::map<std::string, double(*) (int, int)> regfntbl_dii_;
		std::map<std::string, int(*) (double, int)> regfntbl_idi_;
		std::map<std::string, double(*) (double, int)> regfntbl_ddi_;
		std::map<std::string, int(*) (int, double)> regfntbl_iid_;
		std::map<std::string, double(*) (int, double)> regfntbl_did_;
		std::map<std::string, int(*) (double, double)> regfntbl_idd_;
		std::map<std::string, double(*) (double, double)> regfntbl_ddd_;
		std::map<std::string,  int(*) (int,int,int,int,int,int)> regfntbl_iiiiiii_;
		std::map<std::string,  int(*) (std::string)> regfntbl_is_;
		std::map<std::string,  std::string(*) (std::string)> regfntbl_ss_;



};

struct ceps_interface_eval_func_callback_ctxt_t{
	State_machine_simulation_core* smc;
	State_machine* active_smp;
};

namespace sm4ceps{

template<> struct Eventqueue_traits<State_machine_simulation_core::event_t>{using id_t =std::string;};

}

bool is_unique_event(State_machine_simulation_core::event_t const & ev);
std::string const & id(State_machine_simulation_core::event_t const & ev);
bool state_machine_sim_core_default_stepping();
void init_state_machine_simulation(int argc, char ** argv,State_machine_simulation_core* smc,Result_process_cmd_line& result_cmd_line);
void run_state_machine_simulation(State_machine_simulation_core* smc,Result_process_cmd_line const& result_cmd_line);
void state_machine_simulation_fatal(int code, std::string const & msg );
void state_machine_simulation_warn(int code, std::string const & msg);
bool node_isrw_state(ceps::ast::Nodebase_ptr node);

#endif
