#include "cepscloud_streaming_common.h"
#include <sstream>
#include <thread>
#include <chrono>
using namespace ceps;
using namespace ceps::cloud;
using namespace ceps::misc;

ceps::cloud::Simulation_Core directory_server;
std::vector<std::string> ceps::cloud::sys_info_available_ifs;
std::map<Simulation_Core,std::vector< Downstream_Mapping > > ceps::cloud::mappings_downstream;
std::map<Simulation_Core,std::vector< Upstream_Mapping > > ceps::cloud::mappings_upstream;
std::map<Simulation_Core, std::shared_ptr<ctrl_thread_info>   > ceps::cloud::ctrl_threads;
std::map<Simulation_Core,std::vector< std::pair<Remote_Interface,Remote_Interface_Type> > > ceps::cloud::info_out_channels;
std::map<Simulation_Core,std::vector< std::pair<Remote_Interface,Remote_Interface_Type> > > ceps::cloud::info_in_channels;
std::vector<Sim_Directory::entry> ceps::cloud::global_directory;

downstream_threads_t ceps::cloud::downstream_threads;
std::set<ceps::cloud::Simulation_Core> ceps::cloud::sim_cores;
std::vector<ceps::cloud::Route> ceps::cloud::routes;
std::mutex ceps::cloud::global_mutex;

#ifdef PCAN_API
std::vector< std::pair<unsigned int, std::string> > pcan_api::all_channels  =
{
	{ PCAN_ISABUS1 , "PCAN-ISA-1" },{ PCAN_ISABUS2 , "PCAN-ISA-2" },{ PCAN_ISABUS3 , "PCAN-ISA-3" },{ PCAN_ISABUS4 , "PCAN-ISA-4" },
	{ PCAN_ISABUS5 , "PCAN-ISA-5" },{ PCAN_ISABUS6 , "PCAN-ISA-6" },{ PCAN_ISABUS7 , "PCAN-ISA-7" },{ PCAN_ISABUS8 , "PCAN-ISA-8" },
	{ PCAN_DNGBUS1 , "PCAN-DNG-1" },
	{ PCAN_PCIBUS1 , "PCAN_PCI-1" }  ,{ PCAN_PCIBUS2 , "PCAN_PCI-2" } ,{ PCAN_PCIBUS3 , "PCAN_PCI-3" } ,{ PCAN_PCIBUS4 , "PCAN_PCI-4" },
	{ PCAN_PCIBUS5 , "PCAN_PCI-5" } ,{ PCAN_PCIBUS6 , "PCAN_PCI-6" } ,{ PCAN_PCIBUS7 , "PCAN_PCI-7" } ,{ PCAN_PCIBUS8 , "PCAN_PCI-8" },
	{ PCAN_PCIBUS9 , "PCAN_PCI-9" } ,{ PCAN_PCIBUS10 , "PCAN_PCI-10" } ,{ PCAN_PCIBUS11 , "PCAN_PCI-11" } ,{ PCAN_PCIBUS12 , "PCAN_PCI-12" },
	{ PCAN_PCIBUS13 , "PCAN_PCI-13" } ,{ PCAN_PCIBUS14 , "PCAN_PCI-14" } ,{ PCAN_PCIBUS15 , "PCAN_PCI-15" } ,{ PCAN_PCIBUS16 , "PCAN_PCI-16" },
	{ PCAN_USBBUS1,"PCAN-USB-1" },{ PCAN_USBBUS2,"PCAN-USB-2" },{ PCAN_USBBUS3,"PCAN-USB-3" },{ PCAN_USBBUS4,"PCAN-USB-4" },
	{ PCAN_USBBUS5,"PCAN-USB-5" },{ PCAN_USBBUS6,"PCAN-USB-6" },{ PCAN_USBBUS7,"PCAN-USB-7" },{ PCAN_USBBUS8,"PCAN-USB-8" } ,
	{ PCAN_USBBUS9,"PCAN-USB-9" },{ PCAN_USBBUS10,"PCAN-USB-10" },{ PCAN_USBBUS11,"PCAN-USB-11" },{ PCAN_USBBUS12,"PCAN-USB-12" },
	{ PCAN_USBBUS13,"PCAN-USB-13" },{ PCAN_USBBUS14,"PCAN-USB-14" },{ PCAN_USBBUS15,"PCAN-USB-15" },{ PCAN_USBBUS16,"PCAN-USB-16" }
};

std::vector< std::pair<std::string, net::can::can_info> > pcan_api::all_channels_info = {
	{ "PCAN-ISA-1",net::can::can_info{} },{ "PCAN-ISA-2",net::can::can_info{} },{ "PCAN-ISA-3",net::can::can_info{} },{ "PCAN-ISA-4",net::can::can_info{} },
	{  "PCAN-ISA-5",net::can::can_info{} },{  "PCAN-ISA-6",net::can::can_info{} },{ "PCAN-ISA-7",net::can::can_info{} },{"PCAN-ISA-8",net::can::can_info{} },
	{ "PCAN-DNG-1",net::can::can_info{} },
	{ "PCAN_PCI-1",net::can::can_info{} }  ,{ "PCAN_PCI-2",net::can::can_info{} } ,{ "PCAN_PCI-3" ,net::can::can_info{} } ,{ "PCAN_PCI-4" ,net::can::can_info{} },
	{ "PCAN_PCI-5",net::can::can_info{} } ,{ "PCAN_PCI-6",net::can::can_info{} } ,{ "PCAN_PCI-7",net::can::can_info{} } ,{ "PCAN_PCI-8",net::can::can_info{} },
	{ "PCAN_PCI-9",net::can::can_info{} } ,{ "PCAN_PCI-10",net::can::can_info{} } ,{ "PCAN_PCI-11",net::can::can_info{} } ,{ "PCAN_PCI-12" ,net::can::can_info{} },
	{ "PCAN_PCI-13" ,net::can::can_info{} } ,{ "PCAN_PCI-14",net::can::can_info{} } ,{ "PCAN_PCI-15" ,net::can::can_info{} } ,{ "PCAN_PCI-16",net::can::can_info{} },
	{ "PCAN-USB-1",net::can::can_info{} },{ "PCAN-USB-2",net::can::can_info{} },{ "PCAN-USB-3",net::can::can_info{} },{ "PCAN-USB-4",net::can::can_info{} },
	{ "PCAN-USB-5",net::can::can_info{} },{ "PCAN-USB-6",net::can::can_info{} },{ "PCAN-USB-7",net::can::can_info{} },{ "PCAN-USB-8",net::can::can_info{} } ,
	{ "PCAN-USB-9" ,net::can::can_info{} },{ "PCAN-USB-10",net::can::can_info{} },{ "PCAN-USB-11" ,net::can::can_info{} },{ "PCAN-USB-12",net::can::can_info{} },
	{ "PCAN-USB-13",net::can::can_info{} },{ "PCAN-USB-14",net::can::can_info{} },{ "PCAN-USB-15",net::can::can_info{} },{ "PCAN-USB-16" ,net::can::can_info{} }
};
#endif

#ifdef KMW_MULTIBUS_API
std::vector< std::pair<unsigned int, std::string> > kmw_api::all_channels =
{
	{ 0 , "KMW_MULTIBUS_0" },{ 1 , "KMW_MULTIBUS_1" },{ 2 , "KMW_MULTIBUS_2" },{ 3 , "KMW_MULTIBUS_3" },{ 4 , "KMW_MULTIBUS_4" },
	{ 5 , "KMW_MULTIBUS_5" },{ 6 , "KMW_MULTIBUS_6" },{ 7 , "KMW_MULTIBUS_7" }
};

std::vector< std::pair<std::string, net::can::can_info> > kmw_api::all_channels_info = {
	{"KMW_MULTIBUS_0",net::can::can_info{}},{"KMW_MULTIBUS_1",net::can::can_info{}},
	{"KMW_MULTIBUS_2",net::can::can_info{}},{"KMW_MULTIBUS_3",net::can::can_info{}},
	{"KMW_MULTIBUS_4",net::can::can_info{}},{"KMW_MULTIBUS_5",net::can::can_info{}},
	{"KMW_MULTIBUS_6",net::can::can_info{} },{"KMW_MULTIBUS_7",net::can::can_info{} }
};
kmw_api::CanStart_t kmw_api::canstart = nullptr;
kmw_api::CanOpen_t kmw_api::canopen = nullptr;
kmw_api::CanWrite_t kmw_api::canwrite = nullptr;
kmw_api::CanRead_t kmw_api::canread = nullptr;
kmw_api::CanInstall_t kmw_api::caninstall = nullptr;
#endif

net::can::can_info net::can::get_local_endpoint_info(std::string endpoint) {
#ifdef PCAN_API
	for (auto e : pcan_api::all_channels_info) {
		if (e.first == endpoint) return e.second;
	}
#endif
#ifdef KMW_MULTIBUS_API
	for (auto e : kmw_api::all_channels_info) {
		if (e.first == endpoint) return e.second;
	}
#endif
	throw net::exceptions::err_can("Unknown local endpoint '"+endpoint+"'");
}

void net::can::set_local_endpoint_info(std::string endpoint, net::can::can_info info) {
#ifdef PCAN_API
	for (auto& e : pcan_api::all_channels_info) {
		if (e.first == endpoint) {
			e.second = info; return;
		}
	}
#endif
#ifdef KMW_MULTIBUS_API
	for (auto& e : kmw_api::all_channels_info) {
		if (e.first == endpoint) {
			e.second = info; return;
		}
	}
#endif

	throw net::exceptions::err_can("Unknown local endpoint '" + endpoint + "'");
}

std::pair<bool, ceps::cloud::Sim_Directory::entry> ceps::cloud::get_direntry(Simulation_Core s) {
	for (auto e : ceps::cloud::global_directory) {
		if (e.sim_core == s) return std::make_pair(true, e);
	}
	return std::make_pair(false, ceps::cloud::Sim_Directory::entry{});
}

std::string ceps::cloud::display_name(ceps::cloud::Simulation_Core s) {
	auto const & d = get_direntry(s);
	if (!d.first) return s.first + ":" + s.second;
	return d.second.name;
}


std::string ceps::cloud::display_name_with_details(ceps::cloud::Simulation_Core s) {
	auto const & d = get_direntry(s);
	if (!d.first) return s.first + ":" + s.second;
	return d.second.name + " ("+d.second.short_name+")";
}

ceps::cloud::Local_Interface net::can::get_local_endpoint(std::string s) {
#ifdef PCAN_API
	for (auto e : pcan_api::all_channels) {
		if (e.second == s)
			return ceps::cloud::Local_Interface{e.second};
	}
#endif
#ifdef KMW_MULTIBUS_API
	for (auto e : kmw_api::all_channels) {
		if (e.second == s)
			return ceps::cloud::Local_Interface{ e.second };
	}
#endif

	return ceps::cloud::Local_Interface{};
}

int net::can::get_local_endpoint_handle(std::string s) {
#ifdef PCAN_API
	for (auto e : pcan_api::all_channels) {
		if (e.second == s)
			return e.first;
	}
#endif
#ifdef KMW_MULTIBUS_API
	for (auto e : kmw_api::all_channels) {
		if (e.second == s)
			return e.first;
	}
#endif

	return -1;
}

#ifdef KMW_MULTIBUS_API
 bool kmw_api::is_kmw(std::string s){
	 for (auto e : kmw_api::all_channels) {
		 if (e.second == s)
			 return true;
	 }
	 return false;
 }
#endif

#ifdef PCAN_API
 bool pcan_api::is_pcan(std::string s) {
	 for (auto e : pcan_api::all_channels) {
		 if (e.second == s)
			 return true;
	 }
	 return false;
 }
#endif

std::vector<std::string> ceps::cloud::check_available_ifs(){
	std::vector<std::string> r;
#ifdef	UNIX_API
    struct ifaddrs *addrs,*tmp;
    getifaddrs(&addrs);
    tmp = addrs;

    while (tmp)
    {
       if (tmp->ifa_name)
            r.push_back(tmp->ifa_name);
        tmp = tmp->ifa_next;
    }
    freeifaddrs(addrs);
#endif
#ifdef WIN_API
 #ifdef PCAN_API
	for (auto channel : pcan_api::all_channels) {
		unsigned int v{};
		auto rc = pcan_api::getvalue(channel.first, PCAN_CHANNEL_CONDITION, &v, sizeof(v));
		if (rc != PCAN_ERROR_OK) continue;
		if (!(v & PCAN_CHANNEL_AVAILABLE)) continue;
		r.push_back(channel.second);
	}
 #endif	
#ifdef KMW_MULTIBUS_API
	for (auto channel : kmw_api::all_channels) {
		r.push_back(channel.second);
	}
#endif
#endif
return r;
}

int net::inet::establish_inet_stream_connect(std::string remote, std::string port) {
	INIT_SYS_ERR_HANDLING

	int cfd = -1;
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	if ((syscall_result = getaddrinfo(remote.c_str(), port.c_str(), &hints, &result)) != 0) {
		STORE_SYS_ERR
            throw net::exceptions::err_getaddrinfo(std::string{ "("+remote+":"+port+") " }+gai_strerrorA(syscall_result));
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		cfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (cfd == -1) continue;
		if (connect(cfd, rp->ai_addr, rp->ai_addrlen) != -1) break;

		if (rp->ai_next == NULL) {
			STORE_SYS_ERR
				close(cfd); cfd = -1;
			if (result != nullptr) freeaddrinfo(result);
			THROW_ERR_INET_ARG(std::string{ "[" + remote + ":" + port + "] " });
		}
		syscall_result = errno;
		close(cfd); cfd = -1;
	}
	return cfd;
}



ceps::cloud::Simulation_Core ceps::cloud::cmdline_read_remote_host(char const * arg) {
	ceps::cloud::Simulation_Core r;
	std::string s = arg;
	std::regex word_regex("([0-9a-zA-Z.-]+):([0-9]+)");
	std::smatch m;
	if (std::regex_match(s, m, word_regex)) {
		if (m.size() == 3) {
			r.first = m[1].str();
			r.second = m[2].str();
		}
	}
	return r;
}



std::pair<bool,std::string> ceps::cloud::get_virtual_can_attribute_content(std::string attr, std::vector<std::pair<std::string,std::string>> const & http_header){
 for(auto const & v : http_header){
     if (v.first == attr)
         return {true,v.second};
 }
 return {false,{}};
}


std::tuple<bool,std::string,std::vector<std::pair<std::string,std::string>>> ceps::cloud::read_virtual_can_msg(int sck,std::string& unconsumed_data){
 using header_t = std::vector<std::pair<std::string,std::string>>;
 std::tuple<bool,std::string,header_t> r;

 constexpr auto buf_size = 4096;
 char buf[buf_size];
 std::string buffer = unconsumed_data;
 std::string eom = "\r\n\r\n";
 std::size_t eom_pos = 0;

 unconsumed_data.clear();
 bool req_complete = false;
 ssize_t readbytes = 0;
 ssize_t buf_pos = 0;

 for(; (readbytes=recv(sck,buf,buf_size-1,0)) > 0;){
  buf[readbytes] = 0;
  for(buf_pos = 0; buf_pos < readbytes; ++buf_pos){
   if (buf[buf_pos] == eom[eom_pos])++eom_pos;else eom_pos = 0;
   if (eom_pos == eom.length()){
    req_complete = true;
    if (buf_pos+1 < readbytes) unconsumed_data = buf+buf_pos+1;
    buf[buf_pos+1] = 0;
    break;
   }
  }
  buffer.append(buf);
  if(req_complete) break;
 }

 if (req_complete) {
  header_t header;
  std::string first_line;
  size_t line_start = 0;
  for(size_t i = 0; i < buffer.length();++i){
    if (i+1 < buffer.length() && buffer[i] == '\r' && buffer[i+1] == '\n' ){
        if (line_start == 0) first_line = buffer.substr(line_start,i);
        else if (line_start != i){
         std::string attribute;
         std::string content;
         std::size_t j = line_start;
         for(;j < i && buffer[j]==' ';++j);
         auto attr_start = j;
         for(;j < i && buffer[j]!=':';++j);
         attribute = buffer.substr(attr_start,j-attr_start);
         ++j;
         for(;j < i && buffer[j]==' ' ;++j);
         auto cont_start = j;
         auto cont_end = i - 1;
         for(;buffer[cont_end] == ' ';--cont_end);
         content = buffer.substr(cont_start, cont_end - cont_start + 1);
         header.push_back(std::make_pair(attribute,content));
        }
        line_start = i + 2;++i;
    }
  }
  return std::make_tuple(true,first_line,header);
 }

 return std::make_tuple(false,std::string{},header_t{});
}


std::tuple<bool, std::string, std::vector<std::pair<std::string, std::string>>> ceps::cloud::vcan_api::send_cmd(int sock, std::string command) {
	INIT_SYS_ERR_HANDLING
		std::stringstream cmd;
	cmd << "HTTP/1.1 100\r\n";
	cmd << "cmd: " + command + "\r\n\r\n";
	auto r = send(sock, cmd.str().c_str(), cmd.str().length(), 0); STORE_SYS_ERR;
	if (r != cmd.str().length()) {
		THROW_ERR_INET
	}
	std::string unconsumed_data;
	return ceps::cloud::read_virtual_can_msg(sock, unconsumed_data);
}

static bool get_attr_values_as_lists_and_zip(
	std::vector<std::pair<std::string, std::string>> const & attrs,
	std::string attr_name1, 
	std::string attr_name2,
	std::vector<std::pair<Remote_Interface, std::string>> & result) {
	using namespace std;

	auto out_raw = ceps::cloud::get_virtual_can_attribute_content(attr_name1, attrs);
	auto types_raw = ceps::cloud::get_virtual_can_attribute_content(attr_name2, attrs);
	if (!out_raw.first || !types_raw.first)
		return false;

	vector<string> list1;
	{
		istringstream iss{ out_raw.second };
		copy(istream_iterator<string>(iss),
			istream_iterator<string>(),
			back_inserter(list1));
	}
	vector<string> list2;
	{
		istringstream iss{ types_raw.second };
		copy(istream_iterator<string>(iss),
			istream_iterator<string>(),
			back_inserter(list2));
	}
	for (size_t i = 0; i != list1.size(); ++i) {
		auto const & ch = list1[i];
		if (i < list2.size()) result.push_back(make_pair(Remote_Interface{ ch }, Remote_Interface_Type{ list2[i] }));
		else result.push_back(make_pair(Remote_Interface{ ch }, Remote_Interface_Type{ "?" }));
	}

	return true;
}

ceps::cloud::vcan_api::fetch_channels_return_t ceps::cloud::vcan_api::fetch_channels(ceps::cloud::Simulation_Core sim_core) {
	INIT_SYS_ERR_HANDLING;
	int cfd = -1;
	CLEANUP([&]() {if (cfd != -1) closesocket(cfd); })
	
	fetch_channels_return_t rv;
	cfd = net::inet::establish_inet_stream_connect(sim_core.first, sim_core.second); 
	if (cfd == -1) {
		throw net::exceptions::err_inet{"Establishing connect to '"+sim_core.first+":"+ sim_core.second+"' failed."};
	}
	try {
		auto rhr = ceps::cloud::vcan_api::send_cmd(cfd, "get_out_channels");

		if (std::get<0>(rhr))
			get_attr_values_as_lists_and_zip(
				std::get<2>(rhr),
				"out_channels",
				"types",
				rv.first);

		rhr = send_cmd(cfd, "get_in_channels");

		if (std::get<0>(rhr))
			get_attr_values_as_lists_and_zip(
				std::get<2>(rhr),
				"in_channels",
				"types",
				rv.second);
	}catch (net::exceptions::err_inet & e) {
		throw net::exceptions::err_inet("("+sim_core.first+":"+sim_core.second+") "+e.what());
	}
	return rv;
}

ceps::cloud::Sim_Directory ceps::cloud::fetch_directory_entries(ceps::cloud::Simulation_Core sim_core) {
	INIT_SYS_ERR_HANDLING;
	int cfd = -1;
	CLEANUP([&]() {if (cfd != -1) closesocket(cfd); })

	Sim_Directory r;
	cfd = net::inet::establish_inet_stream_connect(sim_core.first, sim_core.second);
	if (cfd == -1) {
		throw net::exceptions::err_inet{ "Establishing connect to '" + sim_core.first + ":" + sim_core.second + "' failed." };
	}

	std::string hostname;

	{
		char buffer[512] = { 0 };
		if (gethostname(buffer, 512) != 0) throw net::exceptions::err_inet{ "WS_API getsockname failed." };
		hostname = buffer;
	}

	auto rhr = ceps::cloud::vcan_api::send_cmd(cfd, "get_known_sim_cores\r\nid-token: "+hostname);
	if (!std::get<0>(rhr)) return r;

	using namespace std;

	auto raw_data = ceps::cloud::get_virtual_can_attribute_content("known_sim_cores", std::get<2>(rhr));
	auto config_data = ceps::cloud::get_virtual_can_attribute_content("config", std::get<2>(rhr));
	if (config_data.first) {
		if (config_data.second.length()) initial_config = config_data.second;
	}

	

	for (std::size_t j = 0; raw_data.second.length() > j;) {
		if (raw_data.second[j] != '$') { ++j; continue; }
		j += 1;
		std::string name;
		std::string short_name;
		std::string host;
		auto k = j;
		for (; raw_data.second[k] != '\t' && k < raw_data.second.length(); ++k);
		if (raw_data.second[k] != '\t')  continue;
		name = raw_data.second.substr(j, k - j); k++; j = k;
		for (; raw_data.second[k] != '\t' && k < raw_data.second.length(); ++k);
		if (raw_data.second[k] != '\t')  continue;
		short_name = raw_data.second.substr(j, k - j); k++; j = k;
		for (; raw_data.second[k] != '$' && k < raw_data.second.length(); ++k);
		if (raw_data.second[k] != '$')  continue;
		host = raw_data.second.substr(j, k - j);
		j = k+1;
		/*std::cout << name << std::endl;
		std::cout << short_name << std::endl;
		std::cout << host << std::endl;*/
		if (name.length() == 0 || short_name.length() == 0 || host.length() == 0) continue;
		Sim_Directory::entry e;
		e.name = name;
		e.short_name = short_name;
		if (host[0] == ':') host = sim_core.first + host;
		e.sim_core = cmdline_read_remote_host(host.c_str());
		if (e.sim_core.first.length() == 0 || e.sim_core.second.length() == 0) continue;
		r.entries.push_back(e);
	}
	return r;
}

ceps::cloud::streaming_endpoints_directory ceps::cloud::fetch_streaming_endpoints(ceps::cloud::Simulation_Core sim_core) {
	INIT_SYS_ERR_HANDLING;
	int cfd = -1;
	CLEANUP([&]() {if (cfd != -1) closesocket(cfd); })

	streaming_endpoints_directory r;
	cfd = net::inet::establish_inet_stream_connect(sim_core.first, sim_core.second);
	if (cfd == -1) {
		throw net::exceptions::err_inet{ "Establishing connect to '" + sim_core.first + ":" + sim_core.second + "' failed." };
	}
	auto rhr = ceps::cloud::vcan_api::send_cmd(cfd, "get_known_stream_endpoints");

	if (!std::get<0>(rhr)) return r;
	using namespace std;

	auto raw_data = ceps::cloud::get_virtual_can_attribute_content("known_stream_endpoints", std::get<2>(rhr));
	for (std::size_t j = 0; raw_data.second.length() > j;) {
		if (raw_data.second[j] != '$') { ++j; continue; }
		j += 1;
		std::string port;
		std::string host;
		auto k = j;
		for (; raw_data.second[k] != '$' && k < raw_data.second.length(); ++k);
		if (raw_data.second[k] != '$')  continue;
		host = raw_data.second.substr(j, k - j);
		if (host[0] == ':') host = sim_core.first + host;
		auto se = cmdline_read_remote_host(host.c_str());
		if (se.first.length() == 0 || se.second.length() == 0) continue;
		r.push_back(se);
	}
	return r;
}

bool ceps::cloud::check_and_query_sim_cores(
	std::vector<ceps::cloud::Simulation_Core> cores,
	std::vector<ceps::cloud::Sim_Directory::entry>& new_directory,
	std::vector< std::pair < std::vector < std::pair < ceps::cloud::Remote_Interface, std::string >>, std::vector<std::pair<ceps::cloud::Remote_Interface, std::string>>>>& channel_infos,
	bool wait_for_directory_hosts)
{
	if (!cores.size()) return false;
	std::vector<ceps::cloud::Sim_Directory::entry> t;
	std::set<ceps::cloud::Simulation_Core> already_visted;

	for (auto const & core : cores) {
		try {
			auto dir = ceps::cloud::fetch_directory_entries(core);
			for (auto d : dir.entries) {
				if (already_visted.find(d.sim_core) != already_visted.end()) continue;
				t.push_back(d);
				already_visted.insert(d.sim_core);
			}
		}
		catch (net::exceptions::err_inet const &) {
            if(wait_for_directory_hosts)std::this_thread::sleep_for (std::chrono::seconds(1));
		}
		catch (ceps::cloud::exceptions::err_vcan_api const &) {

		}
	}

	channel_infos.clear();
	std::vector<bool> valid_entries; valid_entries.resize(t.size(), false);
	std::vector<std::future<ceps::cloud::vcan_api::fetch_channels_return_t>> handles;
	for (auto e : t)
		handles.push_back(std::async(std::launch::async, ceps::cloud::vcan_api::fetch_channels, e.sim_core));
	for (std::size_t i = 0; i != handles.size(); ++i) {
		try {
			auto h = handles[i].get();
			valid_entries[i] = true;
			channel_infos.push_back(h);
		}
		catch (net::exceptions::err_inet const &) {
		}
		catch (ceps::cloud::exceptions::err_vcan_api const &) {
		}
	}
	new_directory.clear();
	for (std::size_t i = 0; i != valid_entries.size(); ++i) {
		if (!valid_entries[i]) continue;
		new_directory.push_back(t[i]);
	}
	return new_directory.size();
}

void ceps::cloud::check_and_query_sim_cores(bool wait_for_directory_host) {

	if (directory_server == ceps::cloud::Simulation_Core{}) return;
	for (;;) {
		ceps::cloud::channel_infos_t channel_infos;
		bool any_channels = false;
		if (!ceps::cloud::check_and_query_sim_cores(
			{ directory_server },
				ceps::cloud::global_directory,
				channel_infos, wait_for_directory_host)) {
				ceps::cloud::sim_cores.clear();
			}
		else {
			for (std::size_t i = 0; i != channel_infos.size(); ++i) {
					any_channels = true;
					auto const & ch_info = channel_infos[i];
					ceps::cloud::info_out_channels[ceps::cloud::global_directory[i].sim_core] = ch_info.first;
					ceps::cloud::info_in_channels[ceps::cloud::global_directory[i].sim_core] = ch_info.second;
					ceps::cloud::sim_cores.insert(ceps::cloud::global_directory[i].sim_core);
			}
		}
		if (!wait_for_directory_host) break;
        if (!any_channels) std::this_thread::sleep_for (std::chrono::seconds(1));
		else break;
	}
}



void gateway_fn(Simulation_Core sim_core,
                std::shared_ptr<gateway_thread_info> ctrl,
                int local_sck,
                int extended_can,
                std::string remote_interface){
    //std::cout <<local_sck<<":"<< extended_can << std::endl;
    int remote_sck = -1;
    auto destructor = [&](){
        if(remote_sck!=-1)close(remote_sck);close(local_sck);ctrl->down=true;
    };
    cleanup<decltype(destructor)> trigger_destructor {destructor};
    struct addrinfo hints;
    struct addrinfo *result, *rp;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int syscall_result=0;

    if ((syscall_result = getaddrinfo(sim_core.first.c_str(), sim_core.second.c_str(), &hints, &result)) != 0) {
      return;
    }
    for (rp = result; rp != NULL; rp = rp->ai_next) {
      remote_sck = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
      if (remote_sck == -1) continue;
      if (connect(remote_sck, rp->ai_addr, rp->ai_addrlen) != -1) break;
      syscall_result = errno;
      close(remote_sck);remote_sck=-1;
    }
    if (result != nullptr) freeaddrinfo(result);

    if (rp == nullptr) {
      return;
    }

    {
     //Subscribe
     std::stringstream cmd;
     cmd << "HTTP/1.1 100\r\n";
     cmd << "cmd: subscribe_out_channel\r\n";
     cmd << "out_channel: "<< remote_interface <<"\r\n\r\n";

     auto r = send(remote_sck,cmd.str().c_str(),cmd.str().length(),0);
     if (r != cmd.str().length()) {
         return;
     }
    }
    //std::cout <<local_sck<<":"<< extended_can << std::endl;
    /*for(;!ctrl->shutdown;){
      std::uint32_t l=0;
      auto r = recv(remote_sck,&l,sizeof(l),0);
      if (r != sizeof(l)) return;
      l = ntohl(l);
      struct can_frame can_message{0};
      if ( l != sizeof(can_frame) ) return;
      r = recv(remote_sck,&can_message,sizeof(can_frame),0);
      if (extended_can) can_message.can_id |= CAN_EFF_FLAG;
      r = send(local_sck, &can_message, sizeof(struct can_frame),0);
      if(r!=sizeof(struct can_frame)) return;
    }*/
}


