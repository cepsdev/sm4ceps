#include <sys/types.h>
#include <limits>
#include <cstring>
#include <atomic>
#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <tuple>


#include "cryptopp/sha.h"
#include "cryptopp/filters.h"
#include "cryptopp/hex.h"

#include "cepscloud_streaming_common.h"
#include "vcanstreams.h"
#include "vcan_standard_ctrls.h"
#include "cepscloud_streaming_endpoint_ws_api.h"

std::string initial_config;

#ifdef __MINGW32__
#define InetNtopA inet_ntop

extern "C" WINSOCK_API_LINKAGE LPCWSTR WSAAPI InetNtopW(INT Family, PVOID pAddr, LPWSTR pStringBuf, size_t StringBufSIze);
extern "C" WINSOCK_API_LINKAGE LPCSTR WSAAPI InetNtopA(INT Family, PVOID pAddr, LPSTR pStringBuf, size_t StringBufSize);

#endif



#ifdef KMW_MULTIBUS_API
 extern HINSTANCE kmw_multibus_dll;
#endif

extern void downstream_ctrl(
	 ceps::cloud::Simulation_Core,
	 ceps::cloud::Downstream_Mapping);

extern void upstream_ctrl(
	ceps::cloud::Simulation_Core,
	ceps::cloud::Upstream_Mapping);

extern void r2r_ctrl(
	ceps::cloud::Route);

#ifdef KMW_MULTIBUS_API
extern void downstream_ctrl_multibus(
	ceps::cloud::Simulation_Core sim_core,
	ceps::cloud::Downstream_Mapping dm);
extern void upstream_ctrl_kmw(
	ceps::cloud::Simulation_Core sim_core,
	ceps::cloud::Upstream_Mapping dm);
#endif
/*
* http/websocket routines
*/

void log(std::string);
void warn(std::string msg, bool terminate);

static std::pair<bool, std::string> get_http_attribute_content(std::string attr, std::vector<std::pair<std::string, std::string>> const & http_header) {
	for (auto const & v : http_header) {
		if (v.first == attr)
			return { true,v.second };
	}
	return { false,{} };
}

static char base64set[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string encode_base64(void * mem, size_t len) {
	unsigned char * memory = (unsigned char*)mem;
	if (len == 0) return {};
	int rest = len % 3;
	size_t main_part = len - rest;
	int out_len = (len / 3) * 4;
	short unsigned int padding = 0;
	if (rest == 1) { out_len += 4; padding = 2; }
	else if (rest == 2) { out_len += 4; padding = 1; }
	std::string r;
	r.resize(out_len);
	size_t j = 0;
	size_t jo = 0;

	for (; j < main_part; j += 3, jo += 4) {
		r[jo] = base64set[*(memory + j) >> 2];
		r[jo + 1] = base64set[((*(memory + j) & 3) << 4) | (*(memory + j + 1) >> 4)];
		r[jo + 2] = base64set[((*(memory + j + 1) & 0xF) << 2) | (*(memory + j + 2) >> 6)];
		r[jo + 3] = base64set[*(memory + j + 2) & 0x3F];
	}
	if (rest == 1) {
		r[jo] = base64set[*(memory + j) >> 2];
		r[jo + 1] = base64set[(*(memory + j) & 3) << 4];
		j += 2; jo += 2;
	}
	else if (rest == 2) {
		r[jo] = base64set[*(memory + j) >> 2];
		r[jo + 1] = base64set[((*(memory + j) & 3) << 4) | (*(memory + j + 1) >> 4)];
		r[jo + 2] = base64set[(*(memory + j + 1) & 0xF) << 2];
		j += 3; jo += 3;
	}
	if (padding == 1) r[jo] = '='; else if (padding == 2) { r[jo] = '='; r[jo + 1] = '='; }
	return r;
}

static std::string encode_base64(std::string s) {
	return encode_base64((void*)s.c_str(), s.length());
}

static bool field_with_content(std::string attr, std::string value, std::vector<std::pair<std::string, std::string>> const & http_header) {
	auto r = get_http_attribute_content(attr, http_header);
	if (!r.first) return false;
	return r.second == value;
}

static std::tuple<bool, std::string, std::vector<std::pair<std::string, std::string>>> read_http_request(int sck, std::string& unconsumed_data) {
	using header_t = std::vector<std::pair<std::string, std::string>>;
	std::tuple<bool, std::string, header_t> r;

	constexpr auto buf_size = 4096;
	char buf[buf_size];
	std::string buffer = unconsumed_data;
	std::string eom = "\r\n\r\n";
	std::size_t eom_pos = 0;

	unconsumed_data.clear();
	bool http_req_complete = false;
	ssize_t readbytes = 0;
	ssize_t buf_pos = 0;

	for (; (readbytes = recv(sck, buf, buf_size - 1, 0)) > 0;) {
		buf[readbytes] = 0;
		for (buf_pos = 0; buf_pos < readbytes; ++buf_pos) {
			if (buf[buf_pos] == eom[eom_pos])++eom_pos; else eom_pos = 0;
			if (eom_pos == eom.length()) {
				http_req_complete = true;
				if (buf_pos + 1 < readbytes) unconsumed_data = buf + buf_pos + 1;
				buf[buf_pos + 1] = 0;
				break;
			}
		}
		buffer.append(buf);
		if (http_req_complete) break;
	}

	if (http_req_complete) {
		header_t header;
		std::string first_line;
		size_t line_start = 0;
		for (size_t i = 0; i < buffer.length(); ++i) {
			if (i + 1 < buffer.length() && buffer[i] == '\r' && buffer[i + 1] == '\n') {
				if (line_start == 0) first_line = buffer.substr(line_start, i);
				else if (line_start != i) {
					std::string attribute;
					std::string content;
					std::size_t j = line_start;
					for (; j < i && buffer[j] == ' '; ++j);
					auto attr_start = j;
					for (; j < i && buffer[j] != ':'; ++j);
					attribute = buffer.substr(attr_start, j - attr_start);
					++j;
					for (; j < i && buffer[j] == ' '; ++j);
					auto cont_start = j;
					auto cont_end = i - 1;
					for (; buffer[cont_end] == ' '; --cont_end);
					content = buffer.substr(cont_start, cont_end - cont_start + 1);
					header.push_back(std::make_pair(attribute, content));
				}
				line_start = i + 2; ++i;
			}
		}
		return std::make_tuple(true, first_line, header);
	}

	return std::make_tuple(false, std::string{}, header_t{});
}

static std::string sha1(std::string s) {
	CryptoPP::SHA1 sha1;
	std::string hash;
	auto a = new CryptoPP::StringSink(hash);
	auto b = new CryptoPP::HexEncoder(a);
	auto c = new CryptoPP::HashFilter(sha1, b);
	CryptoPP::StringSource(s, true, c);
	return hash;
}

struct websocket_frame {
	std::vector<unsigned char> payload;
	bool fin = false;
	bool rsv1 = false;
	bool rsv2 = false;
	bool rsv3 = false;
	std::uint8_t opcode = 0;
};

static std::pair<bool, websocket_frame> read_websocket_frame(int sck) {
	websocket_frame r;
	std::uint16_t header;

	auto bytesread = recv(sck, (char*)&header, sizeof header, 0);
	if (bytesread != sizeof header) return { false,{} };

	r.opcode = header & 0xF;
	r.fin = header & 0x80;
	r.rsv1 = header & 0x40;
	r.rsv2 = header & 0x20;
	r.rsv3 = header & 0x10;
	bool mask = header >> 15;
	std::uint8_t payload_len_1 = (header >> 8) & 0x7F;

	size_t payload_len = payload_len_1;

	if (payload_len_1 == 126) {
		std::uint16_t v;
		bytesread = recv(sck, (char*)&v, sizeof v, 0);
		if (bytesread != sizeof v) return { false,{} };
		payload_len = ntohs(v);
	}
	else if (payload_len_1 == 127) {
		std::uint64_t v;
		bytesread = recv(sck, (char*)&v, sizeof v, 0);
		if (bytesread != sizeof v) return { false,{} };
		payload_len = be64toh(v);
	}

	std::uint32_t mask_key = 0;
	if (mask) {
		bytesread = recv(sck, (char*)&mask_key, sizeof mask_key, 0);
		if (bytesread != sizeof mask_key) return { false,{} };
	}

	constexpr size_t bufsize = 4; unsigned char buf[bufsize];
	size_t payload_bytes_read = 0;
	r.payload.resize(payload_len);

	for (; payload_bytes_read < payload_len;) {
		ssize_t toread = std::min(payload_len - payload_bytes_read, bufsize);
		bytesread = recv(sck, (char*)buf, toread, 0);
		if (bytesread != toread) return { false,{} };
		for (size_t i = 0; (ssize_t)i < bytesread; ++i) r.payload[payload_bytes_read + i] = buf[i] ^ ((unsigned char *)&mask_key)[(payload_bytes_read + i) % 4];
		payload_bytes_read += bytesread;
	}

	return { true,r };
}

static std::string get_sim_name(ceps::cloud::Simulation_Core s) {
	auto const & d = ceps::cloud::get_direntry(s);
	if (!d.first) return s.first + ":" + s.second;
	return d.second.name + "@" + s.first + ":" + s.second;
}


std::mutex glbl_mtx;
std::vector<std::thread*> downstream_threads;
std::vector<std::thread*> upstream_threads;
std::vector<std::thread*> route_threads;

std::vector<std::pair<ceps::cloud::Simulation_Core, ceps::cloud::Stream_Mapping>> down_streams;
std::vector<std::pair<ceps::cloud::Simulation_Core, ceps::cloud::Stream_Mapping>> up_streams;
std::vector<ceps::cloud::Route> routes;

std::uint64_t current_token;
std::mutex current_token_mtx;

auto increase_current_token() -> decltype(current_token) {
	std::lock_guard<std::mutex> g(current_token_mtx);
	return ++current_token;
}

auto get_current_token() -> decltype(current_token) {
	std::lock_guard<std::mutex> g(current_token_mtx);
	return current_token;
}

void Websocket_interface::handle_config_cmd(std::string const & s) {
    ::log("Websocket_interface::handle_config_cmd()");
	increase_current_token();//mutes all running communication threads, which will terminate eventually 
	current_configuration = s;
	bool config_down_stream = false;
	bool config_up_stream = false;
	bool config_route = false;

	std::lock_guard<std::mutex> g(glbl_mtx);
	down_streams.clear();
	up_streams.clear();
	routes.clear();

	std::stringstream in{ s };
	std::string l;
	in >> l; // read first line, i.e. cmd[0]
	while (in) {

		char buffer[2049] = { 0 };
		in.getline(buffer, 2048);
		l = buffer;
		if (l == "CONFIG_DOWN_STREAM") { config_down_stream = true; config_up_stream = false; config_route = false; continue; }
		else if (l == "CONFIG_UP_STREAM") { config_down_stream = false; config_up_stream = true; config_route = false; continue; }
		else if (l == "CONFIG_ROUTE") { config_down_stream = false; config_up_stream = false; config_route = true; continue; }

		if (l == "ENTRY") {
			std::string sim_name;
			std::string channel;
			std::string local_channel;
			std::string sim_name_from;
			std::string channel_from;
			std::string sim_name_to;
			std::string channel_to;
			ceps::cloud::Simulation_Core sim_core;
			ceps::cloud::Simulation_Core sim_core_from;
			ceps::cloud::Simulation_Core sim_core_to;


			if (!config_route) {
				in.getline(buffer, 2048); sim_name = buffer;
				in.getline(buffer, 2048); channel = buffer;
				in.getline(buffer, 2048); local_channel = buffer;
				for (auto s : ceps::cloud::sim_cores) {
					if (get_sim_name(s) == sim_name) {
						sim_core = s;
						break;
					}
				}
			}
			else {
				in.getline(buffer, 2048); sim_name_from = buffer;
				in.getline(buffer, 2048); channel_from = buffer;
				in.getline(buffer, 2048); sim_name_to = buffer;
				in.getline(buffer, 2048); channel_to = buffer;
				for (auto s : ceps::cloud::sim_cores) {
					if (get_sim_name(s) == sim_name_from) {
						sim_core_from = s;
						break;
					}
				}
				for (auto s : ceps::cloud::sim_cores) {
					if (get_sim_name(s) == sim_name_to) {
						sim_core_to = s;
						break;
					}
				}
			}

			if (!config_route && (sim_core == ceps::cloud::Simulation_Core{}) )
			{
				warn("Ignoring config cmd, unknown sim core.", false);
				continue;
			}
			if (config_route && (sim_core_to == ceps::cloud::Simulation_Core{} || sim_core_to == ceps::cloud::Simulation_Core{}))
			{
				warn("Ignoring config cmd, unknown sim core.", false);
				continue;
			}

			if (config_down_stream) {
				down_streams.push_back(std::make_pair(sim_core, ceps::cloud::Stream_Mapping{ local_channel , channel }));
				ceps::cloud::Downstream_Mapping dm{
					ceps::cloud::Local_Interface{ local_channel },
					ceps::cloud::Remote_Interface{ channel }
				};
			    auto dwn_ctrl = global_ctrlregistry.get_down_stream_ctrl(ceps::cloud::get_down_stream_type(sim_core, channel), local_channel);
				if (dwn_ctrl != nullptr){
                    log(ceps::cloud::get_down_stream_type(sim_core, channel) + "/"+ local_channel);
					downstream_threads.push_back(
						new std::thread{ dwn_ctrl,
										 sim_core,
										 dm,
						                 global_ctrlregistry.get_down_stream_hook(ceps::cloud::get_down_stream_type(sim_core, channel), local_channel)});
                } else warn("No downstreamctrl found for "+ceps::cloud::get_down_stream_type(sim_core, channel) + "/" + local_channel,false);

			}
			if (config_up_stream) {
				up_streams.push_back(std::make_pair(sim_core, ceps::cloud::Stream_Mapping{ local_channel , channel }));

				ceps::cloud::Upstream_Mapping um{
					ceps::cloud::Local_Interface{ local_channel },
					ceps::cloud::Remote_Interface{ channel }
				};
				auto up_ctrl = global_ctrlregistry.get_up_stream_ctrl(ceps::cloud::get_up_stream_type(sim_core, channel), local_channel);
				if (up_ctrl != nullptr) {
					upstream_threads.push_back(
						new std::thread{ up_ctrl,
						sim_core,
						um });
				}
				else warn("No upstreamctrl found for " + ceps::cloud::get_up_stream_type(sim_core, channel) + "/" + local_channel, false);
			}
			if (config_route) {
				auto rt = ceps::cloud::Route{ std::make_pair(sim_core_from,channel_from), std::make_pair(sim_core_to,channel_to) };
				routes.push_back(rt);
				route_threads.push_back(new std::thread{
					r2r_ctrl,
					rt
				});
			}
		}
	}
}

void Websocket_interface::handler(int sck) {
	
	auto shutdown_update_thread = std::shared_ptr<std::atomic_bool>(new std::atomic_bool{ false });

	auto cleanup_f = [this, sck]() {
		using namespace std;
		lock_guard<std::mutex> lg(handler_threads_status_mutex_);
		for (auto& s : handler_threads_status_)
			if (get<1>(s) && get<0>(s) && get<2>(s) == sck)
			{
				get<1>(s) = false; get<2>(s) = -1; close(sck);
			}

	};
	cleanup<decltype(cleanup_f)> cl{ cleanup_f };


	std::string unconsumed_data;
	auto rhr = read_http_request(sck, unconsumed_data);
	if (!std::get<0>(rhr)) return;

	auto const & attrs = std::get<2>(rhr);
	if (!field_with_content("Upgrade", "websocket", attrs) || !field_with_content("Connection", "Upgrade", attrs)) return;
	auto r = get_http_attribute_content("Sec-WebSocket-Key", attrs);
	if (!r.first)return;

	auto phrase = r.second + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
	unsigned char digest[CryptoPP::SHA::DIGESTSIZE];
	CryptoPP::SHA().CalculateDigest(digest, (unsigned char *)phrase.c_str(), phrase.length());
	auto hash = encode_base64(digest, CryptoPP::SHA::DIGESTSIZE);
	std::stringstream response;
	response << "HTTP/1.1 101 Switching Protocols\r\n" << "Upgrade: websocket\r\n" << "Connection: Upgrade\r\n"
		<< "Sec-WebSocket-Accept: " << hash << "\r\n\r\n";

	auto byteswritten = send(sck, response.str().c_str(), response.str().length(), 0);
	if (byteswritten != (ssize_t)response.str().length()) return;


	auto send_reply = [sck](std::string const & reply) -> bool {
		auto len = reply.length();
		bool fin = true;
		bool ext1_len = len >= 126 && len <= 65535;
		bool ext2_len = len > 65535;

		std::uint16_t header = 0;
		if (fin) header |= 0x80;
		if (!ext1_len && !ext2_len) header |= len << 8;
		else if (ext1_len) header |= 126 << 8;
		else header |= 127 << 8;
		header |= 1;
		auto wr = send(sck, (char*)&header, sizeof header,0);
		if (wr != sizeof header) return false;
		if (ext1_len)
		{
			std::uint16_t v = len; v = htons(v);
			if ((wr = send(sck, (char*)&v, sizeof v,0)) != sizeof v) return false;
		}
		if (ext2_len)
		{
			std::uint64_t v = len; v = htobe64(v);
			if ((wr = send(sck, (char*)&v, sizeof v,0)) != sizeof v) return false;
		}
		if ((wr = send(sck, (char*)reply.c_str(), len,0)) != (int)len) return false;
		return true;
	};

	for (;;) {
		auto frm = read_websocket_frame(sck);
		if (!frm.first) break;
		std::vector<unsigned char> payload = std::move(frm.second.payload);
		while (!frm.second.fin) {
			frm = read_websocket_frame(sck);
			if (!frm.first) break;
			payload.reserve(payload.size() + frm.second.payload.size());
			payload.insert(payload.end(), frm.second.payload.begin(), frm.second.payload.end());
		}
		if (!frm.first) break;
		if (frm.second.opcode == 1) {
			std::string s;
			s.resize(payload.size());
			for (size_t j = 0; j < payload.size(); ++j)s[j] = payload[j];
			//handle command

			std::stringstream hs{ s };
			std::vector<std::string> cmd;
			for (; hs;) {
				std::string l;
				hs >> l; cmd.push_back(l);
			}
			//std::cout << s  << std::endl;
			std::string reply = "{\"ok\": false}";

			if (cmd.size() != 0) {
				decltype(cmd) args;
				for (std::size_t i = 1; i != cmd.size(); ++i) if (cmd[i].length()) { args.push_back(cmd[i]); }
				//std::cout << args.size() << std::endl;

				if (cmd[0] == "GET_INFO" && args.size() == 0) {
					reply = "{\"ok\":true, \"value\" : \"cepSCloud's Virtual CAN Gateway (C) Tomas Prerovsky <tomas.prerovsky@gmail.com>, ALL RIGHTS RESERVED.\"}";
				}
				else if (cmd[0] == "CONFIG_STREAMS") {
					handle_config_cmd(s);
					reply = "{\"ok\":true}";
				} else if (cmd[0] == "GET_CONFIGURATION") {
					std::lock_guard<std::mutex> g(glbl_mtx);
					std::stringstream sreply;
					sreply << "\"simcore_infos\": [";
					std::vector<ceps::cloud::Simulation_Core> scores;
					std::copy(ceps::cloud::sim_cores.begin(), ceps::cloud::sim_cores.end(), std::back_inserter(scores));
					
					for (std::size_t si = 0; si != scores.size(); ++si) {
						auto & s = scores[si];
						sreply << "{";
						auto const & d = ceps::cloud::get_direntry(s);
						sreply << "\"name\": \"" << get_sim_name(s) << "\",";
						sreply << "\"out\": [";
						for (std::size_t i = 0; i != ceps::cloud::info_out_channels[s].size();++i) {
							auto e = ceps::cloud::info_out_channels[s][i];
							sreply << "{ \"name\" : \"" << e.first << "\", \"type\": \"" << e.second << "\"}";
							if (i + 1 != ceps::cloud::info_out_channels[s].size()) sreply << ",";
						}
						sreply << "],";
						sreply << "\"in\": [";
						for (std::size_t i = 0; i != ceps::cloud::info_in_channels[s].size(); ++i) {
							auto e = ceps::cloud::info_in_channels[s][i];
							sreply << "{ \"name\" : \"" << e.first << "\", \"type\": \"" << e.second << "\"}";
							if (i + 1 != ceps::cloud::info_in_channels[s].size()) sreply << ",";
						}
						sreply << "]";
						sreply << "}";
						if (si + 1 != scores.size()) sreply << ",";
					}

					sreply << "],";

					sreply << "\"local_endpoints\": [";
					for (std::size_t i = 0; ceps::cloud::sys_info_available_ifs.size() != i;++i) {
						auto e = ceps::cloud::sys_info_available_ifs[i];
						sreply << "\"" << e << "\"";
						if (i + 1 != ceps::cloud::sys_info_available_ifs.size()) sreply << ",";
					}
					sreply << "],";

					sreply << "\"downstreams\": [";
					for (std::size_t i = 0; i != down_streams.size(); ++i) {
						auto const & e = down_streams[i];
						sreply << "{";
						sreply << "\"sim_name\": \"" << get_sim_name(e.first) << "\" ,";
						sreply << "\"host\": \"" << e.first.first << "\" ,";
						sreply << "\"port\": \"" << e.first.second << "\" ,";
						sreply << "\"channel\": \"" << e.second.second << "\" ,";
						sreply << "\"local_channel\": \"" << e.second.first << "\"";
						sreply << "}";
						if (i + 1 != down_streams.size()) sreply << ",";					
					}
					sreply << "],";

					sreply << "\"upstreams\": [";
					for (std::size_t i = 0; i != up_streams.size(); ++i) {
						auto const & e = up_streams[i];
						sreply << "{";
						sreply << "\"sim_name\": \"" << get_sim_name(e.first) << "\" ,";
						sreply << "\"host\": \"" << e.first.first << "\" ,";
						sreply << "\"port\": \"" << e.first.second << "\" ,";
						sreply << "\"channel\": \"" << e.second.second << "\" ,";
						sreply << "\"local_channel\": \"" << e.second.first << "\"";
						sreply << "}";
						if (i + 1 != up_streams.size()) sreply << ",";
					}
					sreply << "],";

					sreply << "\"routes\": [";
					for (std::size_t i = 0; i != routes.size(); ++i) {
						const auto& e = routes[i];
						sreply << "{ \"from\":";
						 sreply << "{\"sim_name\": \"" << get_sim_name(e.from.first) << "\" ,";
						 sreply << "\"host\": \"" << e.from.first.first << "\" ,";
						 sreply << "\"port\": \"" << e.from.first.second << "\" ,";
						 sreply << "\"channel\": \"" << e.from.second << "\"";
						 sreply << "},";
						sreply << "\"to\":";
						 sreply << "{\"sim_name\": \"" << get_sim_name(e.to.first) << "\" ,";
						 sreply << "\"host\": \"" << e.to.first.first << "\" ,";
						 sreply << "\"port\": \"" << e.to.first.second << "\" ,";
						 sreply << "\"channel\": \"" << e.to.second << "\"";
						 sreply << "}";
						sreply << "}";
						if (i + 1 != routes.size()) sreply << ",";
					}
					sreply << "]";

					reply = "{\"ok\":true, \"value\" : {"+sreply.str()+"} }";
				}
			}//cmd.size()!=0
			if (!send_reply(reply)) break;
		}
	}//for
}


void Websocket_interface::dispatcher() {
	socklen_t addrlen = sizeof(struct sockaddr_storage);
    struct sockaddr_storage claddr = { 0 };
	int cfd = -1;
	struct addrinfo hints = { 0 };
	struct addrinfo* result, *rp;
	int lfd = -1;

	hints.ai_canonname = nullptr;
	hints.ai_addr = nullptr;
	hints.ai_next = nullptr;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_family = AF_INET;
	hints.ai_flags = AI_PASSIVE;// | AI_NUMERICSERV;

	std::string hostname;

	{
		char buffer[512] = { 0 };
		if (gethostname(buffer, 512) != 0) throw net::exceptions::err_inet{ "WS_API getsockname failed." };
		hostname = buffer;
	}

	
	struct sockaddr_in sa = { 0 };
	sa.sin_family = AF_INET;
	lfd = socket(AF_INET, SOCK_STREAM, 0);
	if (lfd == -1) throw net::exceptions::err_inet{"socket() failed"};
	if (bind(lfd, (struct sockaddr*) &sa, sizeof(sa)) != 0) throw net::exceptions::err_inet{ "bind() failed" };
	if (listen(lfd, 128) == -1) throw net::exceptions::err_inet{ "listen() failed" };

    log(directory_server_name_);

	int listening_port;
	{
		struct sockaddr_in sin;
		socklen_t len = sizeof(sin);
		if (getsockname(lfd, (struct sockaddr *)&sin,&len ) != 0)
			throw net::exceptions::err_inet{ "WS_API getsockname failed." };
		listening_port = ntohs(sin.sin_port);

		char buffer[512] = { 0 };
        inet_ntop(AF_INET, &sin.sin_addr, buffer, 512);
		//std::cout << buffer << std::endl;
	}

	{
		INIT_SYS_ERR_HANDLING;
		int cfd = -1;
        CLEANUP([&]() {if (cfd != -1) closesocket(cfd); });
        log(directory_server_name_);


		cfd = net::inet::establish_inet_stream_connect(directory_server_name_, directory_server_port_);
		if (cfd == -1) {
			throw net::exceptions::err_inet{ "Establishing connect to '" + directory_server_name_ + ":" + directory_server_port_ + "' failed." };
		}
		auto rhr = ceps::cloud::vcan_api::send_cmd(cfd, "register_stream_endpoint\r\nhost: "+hostname+"\r\nport: "+std::to_string(listening_port));
		
		if (!std::get<0>(rhr)) throw ceps::cloud::exceptions::err_vcan_api{ "register_stream_endpoint failed." };
	}

	for (;;)
	{		
		cfd = accept(lfd, (struct sockaddr*) &claddr, &addrlen);
		if (cfd == -1)  continue;
		{
			using namespace std;
			lock_guard<std::mutex> lg(handler_threads_status_mutex_);
			thread_status_type* pp = nullptr;
			for (auto& s : handler_threads_status_) if (!get<1>(s) && get<0>(s)) {
				get<0>(s)->join(); delete get<0>(s); get<0>(s) = nullptr; if (!pp) pp = &s;
			}//for
			auto tp = new thread(&Websocket_interface::handler, this, cfd);
			if (pp) { *pp = thread_status_type{ tp,true,cfd }; }
			else handler_threads_status_.push_back(thread_status_type{ tp,true,cfd });
		}
	}
}

std::thread* Websocket_interface::start() {
	if (initial_config.length()) handle_config_cmd(initial_config);
	return dispatcher_thread_ = new std::thread(&Websocket_interface::dispatcher, this);
}
