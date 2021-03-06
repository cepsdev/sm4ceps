#include "core/include/sm_raw_frame.hpp"
#include "core/include/state_machine_simulation_core.hpp"

#include <sys/types.h>
#include <limits>
#include <cstring>


#include "../cryptopp/sha.h"
#include "../cryptopp/filters.h"
#include "../cryptopp/hex.h"

#include "core/include/base_defs.hpp"

const int Rawframe_generator::IS_ASCII = 1;
const int Rawframe_generator::IS_UTF8 = 2;
const int Rawframe_generator::IS_BINARY = 4;
const int Rawframe_generator::IS_JSON = 8;


/*
 * http/websocket routines
*/

static std::pair<bool,std::string> get_http_attribute_content(std::string attr, std::vector<std::pair<std::string,std::string>> const & http_header){
 for(auto const & v : http_header){
	 if (v.first == attr)
		 return {true,v.second};
 }
 return {false,{}};
}

static char base64set[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string encode_base64(void * mem, size_t len){
 unsigned char * memory = (unsigned char*)mem;
 if (len == 0) return {};
 int rest = len % 3;
 size_t main_part = len - rest;
 int out_len = (len / 3) * 4;
 short unsigned int padding = 0;
 if (rest == 1) {out_len += 4;padding=2;} else if (rest == 2){ out_len +=4;padding=1;}
 std::string r;
 r.resize(out_len);
 size_t j = 0;
 size_t jo = 0;

 for(; j < main_part; j+=3,jo+=4){
  r[jo] = base64set[ *(memory + j) >> 2];
  r[jo+1] = base64set[  ( (*(memory + j) & 3) << 4)  | ( *(memory + j + 1) >> 4) ];
  r[jo+2] = base64set[ ( (*(memory + j + 1) & 0xF) << 2 )  | (*(memory + j + 2) >> 6) ];
  r[jo+3] = base64set[*(memory + j + 2) & 0x3F];
 }
 if (rest == 1){
  r[jo] = base64set[ *(memory + j) >> 2];
  r[jo+1] = base64set[ (*(memory + j) & 3) << 4];
  j+=2;jo+=2;
 } else if (rest == 2){
  r[jo] = base64set[ *(memory + j) >> 2];
  r[jo+1] = base64set[  ( (*(memory + j) & 3) << 4)  | ( *(memory + j + 1) >> 4) ];
  r[jo+2] = base64set[ (*(memory + j + 1) & 0xF) << 2 ];
  j+=3;jo+=3;
 }
 if (padding == 1) r[jo]='='; else if (padding == 2) {r[jo] = '='; r[jo+1] = '=';}
 return r;
}

static std::string encode_base64(std::string s){
 return encode_base64((void*)s.c_str(),s.length());
}

static bool field_with_content(std::string attr, std::string value,std::vector<std::pair<std::string,std::string>> const & http_header){
 auto r = get_http_attribute_content(attr,http_header);
 if (!r.first) return false;
 return r.second == value;
}

static std::tuple<bool,std::string,std::vector<std::pair<std::string,std::string>>> read_http_request(int sck,std::string& unconsumed_data){
 using header_t = std::vector<std::pair<std::string,std::string>>;
 std::tuple<bool,std::string,header_t> r;

 constexpr auto buf_size = 4096;
 char buf[buf_size];
 std::string buffer = unconsumed_data;
 std::string eom = "\r\n\r\n";
 std::size_t eom_pos = 0;

 unconsumed_data.clear();
 bool http_req_complete = false;
 ssize_t readbytes = 0;
 ssize_t buf_pos = 0;

 for(; (readbytes=recv(sck,buf,buf_size-1,0)) > 0;){
  buf[readbytes] = 0;
  for(buf_pos = 0; buf_pos < readbytes; ++buf_pos){
   if (buf[buf_pos] == eom[eom_pos])++eom_pos;else eom_pos = 0;
   if (eom_pos == eom.length()){
	http_req_complete = true;
	if (buf_pos+1 < readbytes) unconsumed_data = buf+buf_pos+1;
	buf[buf_pos+1] = 0;
	break;
   }
  }
  buffer.append(buf);
  if(http_req_complete) break;
 }

 if (http_req_complete) {
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

static std::string sha1(std::string s){
 CryptoPP::SHA1 sha1;
 std::string hash;
 auto a = new CryptoPP::StringSink(hash);
 auto b = new CryptoPP::HexEncoder(a);
 auto c = new CryptoPP::HashFilter(sha1, b);
 CryptoPP::StringSource(s, true, c);
 return hash;
}

struct websocket_frame{
 std::vector<unsigned char> payload;
 bool fin = false;
 bool rsv1 = false;
 bool rsv2 = false;
 bool rsv3 = false;
 std::uint8_t opcode = 0;
};

static std::pair<bool,websocket_frame> read_websocket_frame(int sck){
	websocket_frame r;
	std::uint16_t header;

	auto bytesread = recv(sck,&header,sizeof header,0);
    if (bytesread != sizeof header) return {false,{}};

    r.opcode = header & 0xF;
    r.fin  = header & 0x80;
    r.rsv1 = header & 0x40;
    r.rsv2 = header & 0x20;
    r.rsv3 = header & 0x10;
    bool mask = header >> 15;
    std::uint8_t payload_len_1 = (header >> 8) & 0x7F;

    size_t payload_len = payload_len_1;

    if (payload_len_1 == 126){
     std::uint16_t v;
     bytesread = recv(sck,&v,sizeof v,0);
     if (bytesread != sizeof v) return {false,{}};
     payload_len = ntohs(v);
    } else if (payload_len_1 == 127){
     std::uint64_t v;
     bytesread = recv(sck,&v,sizeof v,0);
     if (bytesread != sizeof v) return {false,{}};
     payload_len = be64toh(v);
    }

    std::uint32_t mask_key = 0;
    if (mask){
     bytesread = recv(sck,&mask_key,sizeof mask_key,0);
     if (bytesread != sizeof mask_key) return {false,{}};
    }

    constexpr size_t bufsize = 4;unsigned char buf[bufsize];
    size_t payload_bytes_read = 0;
    r.payload.resize(payload_len);

    for(;payload_bytes_read < payload_len;){
    	ssize_t toread = std::min(payload_len - payload_bytes_read,bufsize);
    	bytesread = recv(sck,(char*)buf,toread,0);
    	if (bytesread != toread) return {false,{}};
    	for(size_t i = 0; (ssize_t)i < bytesread; ++i) r.payload[payload_bytes_read+i] = buf[i] ^ ((unsigned char *)&mask_key)[ (payload_bytes_read+i) % 4];
    	payload_bytes_read += bytesread;
    }

	return {true,r};
}






extern ceps::ast::Nodebase_ptr eval_locked_ceps_expr(State_machine_simulation_core* smc,
		 State_machine* containing_smp,
		 ceps::ast::Nodebase_ptr node,
		 ceps::ast::Nodebase_ptr root_node);

ceps::ast::Nodebase_ptr ceps_interface_eval_func_callback(std::string const & id, ceps::ast::Call_parameters* params, void* context);
ceps::ast::Nodebase_ptr ceps_interface_binop_resolver( ceps::ast::Binary_operator_ptr binop,
	 	 	 	  	  	  	  	  	  	  	  	  	  	  	  ceps::ast::Nodebase_ptr lhs ,
	 	 	 	  	  	  	  	  	  	  	  	  	  	  	  ceps::ast::Nodebase_ptr rhs,
	 	 	 	  	  	  	  	  	  	  	  	  	  	  	  void* cxt,ceps::ast::Nodebase_ptr parent_node);

bool read_func_call_values(State_machine_simulation_core* smc,	ceps::ast::Nodebase_ptr root_node,
							std::string & func_name,
							std::vector<ceps::ast::Nodebase_ptr>& args);

bool Podframe_generator::readfrom_spec(ceps::ast::Nodeset const & spec)
{
 spec_ = spec;
 return true;
}

size_t Podframe_generator::header_length() {
	return header_length_;
}


static std::map<std::string,std::pair<unsigned short, bool>> typename2descr=
{
		{"bit",{1,true}},{"byte",{8,true}},
		{"int1",{1,true}},{"int2",{2,true}},{"int3",{3,true}},{"int4",{4,true}},{"int5",{5,true}},{"int6",{6,true}},{"int7",{7,true}},{"int8",{8,true}},
		{"int9",{9,true}},{"int10",{10,true}},{"int11",{11,true}},{"int12",{12,true}},{"int13",{13,true}},{"int14",{14,true}},{"int15",{15,true}},{"int16",{16,true}},
		{"int17",{17,true}},{"int18",{18,true}},{"int19",{19,true}},{"int20",{20,true}},{"int21",{21,true}},{"int22",{22,true}},{"int23",{23,true}},{"int24",{24,true}},
		{"int25",{25,true}},{"int26",{26,true}},{"int27",{27,true}},{"int28",{28,true}},{"int29",{29,true}},{"int30",{30,true}},{"int31",{31,true}},{"int32",{32,true}},
		{"int33",{33,true}},{"int34",{34,true}},{"int35",{35,true}},{"int36",{36,true}},{"int37",{37,true}},{"int38",{38,true}},{"int39",{39,true}},{"int40",{40,true}},
		{"int41",{41,true}},{"int42",{42,true}},{"int43",{43,true}},{"int44",{44,true}},{"int45",{45,true}},{"int46",{46,true}},{"int47",{47,true}},{"int48",{48,true}},
		{"int49",{49,true}},{"int50",{50,true}},{"int51",{51,true}},{"int52",{52,true}},{"int53",{53,true}},{"int54",{54,true}},{"int55",{55,true}},{"int56",{56,true}},
		{"int57",{57,true}},{"int58",{58,true}},{"int59",{59,true}},{"int60",{60,true}},{"int61",{61,true}},{"int62",{62,true}},{"int63",{63,true}},{"int64",{64,true}},

		{"uint1",{1,false}},{"uint2",{2,false}},{"uint3",{3,false}},{"uint4",{4,false}},{"uint5",{5,false}},{"uint6",{6,false}},{"uint7",{7,false}},{"uint8",{8,false}},
		{"uint9",{9,false}},{"uint10",{10,false}},{"uint11",{11,false}},{"uint12",{12,false}},{"uint13",{13,false}},{"uint14",{14,false}},{"uint15",{15,false}},{"uint16",{16,false}},
		{"uint17",{17,false}},{"uint18",{18,false}},{"uint19",{19,false}},{"uint20",{20,false}},{"uint21",{21,false}},{"uint22",{22,false}},{"uint23",{23,false}},{"uint24",{24,false}},
		{"uint25",{25,false}},{"uint26",{26,false}},{"uint27",{27,false}},{"uint28",{28,false}},{"uint29",{29,false}},{"uint30",{30,false}},{"uint31",{31,false}},{"uint32",{32,false}},
		{"uint33",{33,false}},{"uint34",{34,false}},{"uint35",{35,false}},{"uint36",{36,false}},{"uint37",{37,false}},{"uint38",{38,false}},{"uint39",{39,false}},{"uint40",{40,false}},
		{"uint41",{41,false}},{"uint42",{42,false}},{"uint43",{43,false}},{"uint44",{44,false}},{"uint45",{45,false}},{"uint46",{46,false}},{"uint47",{47,false}},{"uint48",{48,false}},
		{"uint49",{49,false}},{"uint50",{50,false}},{"uint51",{51,false}},{"uint52",{52,false}},{"uint53",{53,false}},{"uint54",{54,false}},{"uint55",{55,false}},{"uint56",{56,false}},
		{"uint57",{57,false}},{"uint58",{58,false}},{"uint59",{59,false}},{"uint60",{60,false}},{"uint61",{61,false}},{"uint62",{62,false}},{"uint63",{63,false}},{"uint64",{64,false}}


};

size_t fill_raw_chunk(std::map<std::string /*systemstate*/,
 std::map< int, ceps::ast::Nodebase_ptr> > const & encoding,
 size_t & header_length, State_machine_simulation_core* smc,
 std::vector<ceps::ast::Nodebase_ptr> pattern,
 size_t data_size,
 char* data,
 size_t bit_offs,
 std::uint32_t* info,
 size_t bit_width=sizeof(std::int64_t)*8,
 bool signed_value = true,
 bool write_data = true,
 bool host_byte_order = true
);

size_t fill_raw_chunk(std::map<std::string /*systemstate*/,
 std::map< int, ceps::ast::Nodebase_ptr> > const & encoding,
 size_t & header_length, State_machine_simulation_core* smc,
 ceps::ast::Nodebase_ptr p,
 size_t data_size,
 char* data,
 size_t bit_offs,
 std::uint32_t* info,
 size_t bit_width=sizeof(std::int64_t)*8,
 bool signed_value = true,
 bool write_data = true,
 bool host_byte_order = true) {
	using namespace ceps::ast;
	//unsigned char * data = (unsigned char *) data_;
	if (p == nullptr) return 0;
	//if (write_data) std::cout << "*** bit_width=" << bit_width << std::endl;
	if (p->kind() == ceps::ast::Ast_node_kind::structdef){
	 auto& st = ceps::ast::as_struct_ref(p);
	 auto& nm = ceps::ast::name(st);
	 if (nm == "in"){
	  return 0;
	 } else if (nm == "out"){
	 return fill_raw_chunk(encoding,header_length,smc,st.children(), data_size,data, bit_offs,info, bit_width,signed_value,write_data,host_byte_order);
	}
	auto r = typename2descr.find(nm);
	 if (r != typename2descr.end())
	  return fill_raw_chunk(encoding,header_length,smc,st.children(),data_size,data,bit_offs,info,r->second.first,r->second.second,write_data,host_byte_order);
	 return fill_raw_chunk(encoding,header_length,smc,st.children(), data_size,data, bit_offs,info, bit_width,signed_value,write_data,host_byte_order);
	}
	else if (p->kind() == ceps::ast::Ast_node_kind::identifier){
		auto& ident = ceps::ast::as_id_ref(p);
		if (ceps::ast::name(ident) == "any") return bit_width;
		else if (ceps::ast::name(ident) == "__current_frame_size") {
			auto temp_node = ceps::ast::Int(data_size,ceps::ast::all_zero_unit(), nullptr, nullptr, nullptr);
			return fill_raw_chunk(encoding,header_length,smc,&temp_node,data_size, data, bit_offs,info, bit_width,signed_value,write_data,host_byte_order);
		} else smc->fatal_(-1,std::string("Raw frame:  Unknown identifier '")+ceps::ast::name(ident)+"'");
	}else if (p->kind() == ceps::ast::Ast_node_kind::int_literal || p->kind() == ceps::ast::Ast_node_kind::float_literal){
		if (write_data){
			if (info) *info |= Rawframe_generator::IS_BINARY;
			std::uint64_t v;
			if (p->kind() == ceps::ast::Ast_node_kind::int_literal) v = (std::uint64_t)value(as_int_ref(p));
			else v = (std::uint64_t)value(as_double_ref(p));

			if (bit_width < 64){
			   if (bit_width == 1){ v =  (v ? 1 : 0) ;  }
			   else if (bit_width == 4) v &= 0xFLL;
			   else if (bit_width == 8) v &= 0xFFLL;
			   else if (bit_width == 12) v &= 0xFFFLL;
			   else if (bit_width == 16) v &= 0xFFFFLL;
			   else if (bit_width == 20) v &= 0xFFFFFLL;
			   else if (bit_width == 24) v &= 0xFFFFFFLL;
			   else if (bit_width == 28) v &= 0xFFFFFFFLL;
			   else if (bit_width == 32) v &= 0xFFFFFFFFLL;
			   else{
				   std::uint64_t w=1;
				   for(size_t i = 0; i < bit_width;++i) w |= (w << 1);
				   v &= w;
			   }
			}
			int bits_written = 0;
			if (bit_offs % 8) {
				unsigned short o = bit_offs % 8;
				unsigned short d = 8 - o ; // d is the number of bits left in the byte to write
				unsigned char w = *(data + bit_offs/8);
				if (d > bit_width) d = (unsigned short) bit_width;
				unsigned char c1=1;for(int i = 0; i < d;++i)c1 |= (c1 << 1);
				unsigned char c2=1;for(int i = 0; i < o-1;++i)c2 |= (c2 << 1);

				w = (w & c2) | ( ((unsigned char)v & c1) << o );
				*( (unsigned char*)( data + bit_offs/8)) = w;
				bits_written = d;bit_offs+=d;
			}
			if (bits_written) v = v >> bits_written;

			//INVARIANT: bit_offs points to a byte address
			if (bits_written < (int) bit_width){
				for(; bit_width-bits_written>=8;bit_offs+=8,bits_written+=8){
					*( (unsigned char*)data+bit_offs/8) = (unsigned char) v;
				    v = v >> 8;
				}
			}
			//INVARIANT: bit_offs points to a byte address
			//INVARIANT: bit_width - bits_written < 8
			if (bits_written < (int)bit_width){
				if ((int)bit_width-bits_written  == 1){
					unsigned char & target = *( (unsigned char*)(data+ bit_offs/8) );
					if (v & 1)  target |= 1;
					else target &= 0xFE;
				} else
				{
					unsigned char & target = *( (unsigned char*)(data+ bit_offs/8) );
					unsigned char c = 1;
					for(int i = 0; i < (int)bit_width-bits_written-1; ++i ) c |= c << 1;
					target = c & v;
				}
			}
		}
		return bit_width;
	} else if (p->kind() == ceps::ast::Ast_node_kind::string_literal){
		 if (info) *info |= Rawframe_generator::IS_ASCII;
		 std::string s = ceps::ast::value(ceps::ast::as_string_ref(p));
		 int corr = 0;
		 if (bit_offs % 8) corr = 8 - (bit_offs % 8);
		 bit_offs += corr;
		 if (write_data) memcpy((data+bit_offs/8),s.c_str(),s.length());
		 return s.length()*8+corr;
	} else if (p->kind() == ceps::ast::Ast_node_kind::symbol && "Systemstate" == ceps::ast::kind(ceps::ast::as_symbol_ref(p)) ) {
		if(write_data){
		 auto & state = ceps::ast::as_symbol_ref(p);
		 auto it = encoding.find(ceps::ast::name(state));
		 if (it == encoding.end()){
			auto r = eval_locked_ceps_expr(smc,nullptr,&state,nullptr);
			if (r){ return fill_raw_chunk(encoding,header_length,smc,r,data_size, data, bit_offs,info, bit_width,signed_value,write_data,host_byte_order);
			 delete r;}
		 } else {
			 auto func_it = it->second.find(bit_width*(signed_value?-1:1));
			 if (func_it == it->second.end()){
				 auto r = eval_locked_ceps_expr(smc,nullptr,&state,nullptr);
				 if (r){ return fill_raw_chunk(encoding,header_length,smc,r,data_size, data, bit_offs,info, bit_width,signed_value,write_data,host_byte_order);
				 	 delete r;}
			 } else {
				 auto func = func_it->second;
				 auto r = eval_locked_ceps_expr(smc,nullptr,func,nullptr);
				 if (r){ return fill_raw_chunk(encoding,header_length,smc,r,data_size, data, bit_offs,info, bit_width,signed_value,write_data,host_byte_order);
				 	 delete r;}
			 }
		 }
		}
		return bit_width;
	}
	else {std::stringstream ss; ss << *p; smc->fatal_(-1,"Serialization of raw frame: Illformed expression:"+ss.str());}
	return 0;
}

size_t fill_raw_chunk(std::map<std::string /*systemstate*/,
 std::map< int, ceps::ast::Nodebase_ptr> > const & encoding,
 size_t& header_length,State_machine_simulation_core* smc,
 std::vector<ceps::ast::Nodebase_ptr> pattern,
 size_t data_size,
 char* data,
 size_t bit_offs,
 std::uint32_t* info,
 size_t bit_width,
 bool signed_value,
 bool write_data,
 bool host_byte_order){

 size_t r=0;
 for(auto p : pattern){
  bool header = false;
  if (p->kind() == ceps::ast::Ast_node_kind::structdef && !write_data){
   auto& st = ceps::ast::as_struct_ref(p);
   auto& nm = ceps::ast::name(st);
   if (nm == "header") {header = true;}
  }
  auto rr = fill_raw_chunk(encoding,header_length,smc,p,data_size,data,bit_offs,info,bit_width,signed_value,write_data,host_byte_order);
  if (header) {header_length = rr; }
  r+=rr;
  bit_offs += rr;
 }
 return r;
}

size_t compute_size(size_t& header_length,State_machine_simulation_core* smc,std::vector<ceps::ast::Nodebase_ptr> pattern) {
	size_t bits = fill_raw_chunk({},header_length,smc,
            pattern,
            0,
            nullptr,
            0,
			nullptr,
            0,
			true,false,true);
	return bits / 8 + ( (bits % 8) ? 1 : 0);
}

size_t Podframe_generator::compute_size_of_msg(State_machine_simulation_core* smc,
		                                       std::vector<std::string> params,bool& failed)
{

	auto data_format = spec_["data"];

	failed = false;

	return compute_size(header_length_,smc, data_format.nodes() );
}






Rawframe_generator::gen_msg_return_t Podframe_generator::gen_msg(State_machine_simulation_core* smc,
		                          size_t& data_size,
								  std::map<std::string /*systemstate*/,
								  std::map< int, ceps::ast::Nodebase_ptr> > const & encoding){

 if (smc == nullptr) return gen_msg_return_t{0,nullptr};
 auto data_format = spec_["data"];
 if (data_format.nodes().empty()) {
  std::string id;
  auto t = spec_["id"];
  if (t.nodes().size() != 0 && t.nodes()[0]->kind() == ceps::ast::Ast_node_kind::identifier)
	id = ceps::ast::name(ceps::ast::as_id_ref(t.nodes()[0]));
  return gen_msg_return_t{0,nullptr};
 }

 ceps::ast::Nodebase_ptr frame_pattern = nullptr;
 ceps::ast::Scope scope;
 scope.children() = data_format.nodes();scope.owns_children() = false;
 frame_pattern = eval_locked_ceps_expr(smc,nullptr,&scope,nullptr);

 if (frame_pattern == nullptr) return gen_msg_return_t{0,nullptr};
 auto chunk_size = compute_size(header_length_,smc,ceps::ast::nlf_ptr(frame_pattern)->children());
 data_size = chunk_size;
 char* data = new char[chunk_size];
 bzero(data,chunk_size);
 std::uint32_t info = 0;
 fill_raw_chunk(encoding,header_length_, smc,ceps::ast::nlf_ptr(frame_pattern)->children(),chunk_size, data,0,&info);
 delete frame_pattern;
 scope.children().clear();
 if (info & Rawframe_generator::IS_BINARY) info = Rawframe_generator::IS_BINARY;
 return gen_msg_return_t{info,(void*)data};
}


//////////////////////////////////////////////////////////////////
////////////////////////////////////// READ RAW MESSAGE //////////
//////////////////////////////////////////////////////////////////
int read_raw_chunk(size_t& header_length,State_machine_simulation_core* smc,
		            std::vector<ceps::ast::Nodebase_ptr> pattern,
		            size_t data_size,
		            char* data,
		            size_t bit_offs,
		            size_t bit_width=sizeof(std::int64_t)*8,
		            bool signed_value = true,
		            bool read_data = true,
		            bool host_byte_order = true
		            );

int read_raw_chunk(size_t& header_length, State_machine_simulation_core* smc,
		               ceps::ast::Nodebase_ptr p,
		               size_t data_size,
		               char* data,
		               size_t bit_offs,
		               size_t bit_width=sizeof(std::int64_t)*8,
		               bool signed_value = true,
		               bool read_data = true,
		               bool host_byte_order = true) {
	using namespace ceps::ast;
	if (p == nullptr) return 0;
	//if (write_data) std::cout << "*** bit_width=" << bit_width << std::endl;
	if (p->kind() == ceps::ast::Ast_node_kind::structdef){
		auto& st = ceps::ast::as_struct_ref(p);
		auto& nm = ceps::ast::name(st);
		if (nm == "out"){
		  return 0;
		} else if (nm == "in"){
			return read_raw_chunk(header_length,smc,st.children(), data_size,data, bit_offs, bit_width,signed_value,read_data,host_byte_order);
		} else if (nm == "byte" || nm == "int8"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,8,true,read_data,host_byte_order);
		} else if (nm == "ubyte" || nm == "uint8"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,8,false,read_data,host_byte_order);
		} else if (nm == "ushort" || nm == "uint16"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,16,false,read_data,host_byte_order);
		} else if (nm == "short" || nm == "int16"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,16,true,read_data,host_byte_order);
		} else if (nm == "uint" || nm == "uint32"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,32,false,read_data,host_byte_order);
		} else if (nm == "int" || nm == "int32") {
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,32,true,read_data,host_byte_order);
		} else if (nm == "ulonglong" || nm == "uint64") {
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,64,false,read_data,host_byte_order);
		} else if (nm == "longlong" || nm == "int64") {
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,64,true,read_data,host_byte_order);
		}

		else if (nm == "uint25"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,25,false,read_data,host_byte_order);
		}		else if (nm == "uint26"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,26,false,read_data,host_byte_order);
		}		else if (nm == "uint27"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,27,false,read_data,host_byte_order);
		}		else if (nm == "uint28"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,28,false,read_data,host_byte_order);
		}		else if (nm == "uint29"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,29,false,read_data,host_byte_order);
		}		else if (nm == "uint30"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,30,false,read_data,host_byte_order);
		}		else if (nm == "uint31"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,31,false,read_data,host_byte_order);
		} 		else if (nm == "int25"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,25,true,read_data,host_byte_order);
		}		else if (nm == "int26"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,26,true,read_data,host_byte_order);
		}		else if (nm == "int27"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,27,true,read_data,host_byte_order);
		}		else if (nm == "int28"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,28,true,read_data,host_byte_order);
		}		else if (nm == "int29"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,29,true,read_data,host_byte_order);
		}		else if (nm == "int30"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,30,true,read_data,host_byte_order);
		}		else if (nm == "uint31"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,31,true,read_data,host_byte_order);
		}







		else if (nm == "uint24"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,24,false,read_data,host_byte_order);
		} else if (nm == "int24"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,24,true,read_data,host_byte_order);
		}else if (nm == "uint23"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,23,false,read_data,host_byte_order);
		} else if (nm == "int23"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,23,true,read_data,host_byte_order);
		}else if (nm == "uint22"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,22,false,read_data,host_byte_order);
		} else if (nm == "int22"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,22,true,read_data,host_byte_order);
		}else if (nm == "uint21"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,21,false,read_data,host_byte_order);
		} else if (nm == "int21"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,21,true,read_data,host_byte_order);
		}else if (nm == "uint20"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,20,false,read_data,host_byte_order);
		} else if (nm == "int20"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,20,true,read_data,host_byte_order);
		}else if (nm == "uint19"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,19,false,read_data,host_byte_order);
		} else if (nm == "int19"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,19,true,read_data,host_byte_order);
		}else if (nm == "uint18"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,18,false,read_data,host_byte_order);
		} else if (nm == "int18"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,18,true,read_data,host_byte_order);
		}else if (nm == "uint17"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,17,false,read_data,host_byte_order);
		} else if (nm == "int17"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,17,true,read_data,host_byte_order);
		}else if (nm == "uint15"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,15,false,read_data,host_byte_order);
		} else if (nm == "int15"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,15,true,read_data,host_byte_order);
		} else if (nm == "uint14"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,14,false,read_data,host_byte_order);
		} else if (nm == "int14"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,14,true,read_data,host_byte_order);
		}else if (nm == "uint13"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,13,false,read_data,host_byte_order);
		} else if (nm == "int13"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,13,true,read_data,host_byte_order);
		}else if (nm == "uint12"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,12,false,read_data,host_byte_order);
		} else if (nm == "int12"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,12,true,read_data,host_byte_order);
		}else if (nm == "uint11"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,11,false,read_data,host_byte_order);
		} else if (nm == "int11"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,11,true,read_data,host_byte_order);
		} else if (nm == "uint10"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,10,false,read_data,host_byte_order);
		} else if (nm == "int10"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,10,true,read_data,host_byte_order);
		}else if (nm == "uint9"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,9,false,read_data,host_byte_order);
		} else if (nm == "int9"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,9,true,read_data,host_byte_order);
		}else if (nm == "uint7"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,7,false,read_data,host_byte_order);
		} else if (nm == "int7"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,7,true,read_data,host_byte_order);
		} else if (nm == "uint6"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,6,false,read_data,host_byte_order);
		} else if (nm == "int6"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,6,true,read_data,host_byte_order);
		} else if (nm == "uint5"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,5,false,read_data,host_byte_order);
		} else if (nm == "int5"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,8,true,read_data,host_byte_order);
		} else if (nm == "uint4"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,4,false,read_data,host_byte_order);
		} else if (nm == "int4"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,4,true,read_data,host_byte_order);
		} else if (nm == "uint3"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,3,false,read_data,host_byte_order);
		} else if (nm == "int3"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,3,true,read_data,host_byte_order);
		} else if (nm == "uint2"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,2,false,read_data,host_byte_order);
		} else if (nm == "int2"){
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,2,true,read_data,host_byte_order);
		} else if (nm == "bit"){ //std::cout << "------\n";
			return read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,1,true,read_data,host_byte_order);
		} else if (nm == "header"){ //std::cout << "------\n";
			return header_length = read_raw_chunk(header_length,smc,st.children(),data_size,data,bit_offs,1,true,read_data,host_byte_order);
		} else {
			return read_raw_chunk(header_length,smc,st.children(), data_size,data, bit_offs, bit_width,signed_value,read_data,host_byte_order);
		}
	}
	else if (p->kind() == ceps::ast::Ast_node_kind::identifier){
		auto& ident = ceps::ast::as_id_ref(p);
		if (ceps::ast::name(ident) == "any") return bit_width;
		else if (ceps::ast::name(ident) == "__current_frame_size") {
			auto temp_node = ceps::ast::Int(data_size,ceps::ast::all_zero_unit(), nullptr, nullptr, nullptr);
			return read_raw_chunk(header_length,smc,&temp_node,data_size, data, bit_offs, bit_width,signed_value,read_data,host_byte_order);
		} else smc->fatal_(-1,std::string("Raw frame:  Unknown identifier '")+ceps::ast::name(ident)+"'");
	}else if (p->kind() == ceps::ast::Ast_node_kind::int_literal
			|| node_isrw_state(p)/*(p->kind() == ceps::ast::Ast_node_kind::symbol && ceps::ast::kind(ceps::ast::as_symbol_ref(p) ) == "Systemstate"  )*/ ){
		if (read_data){
			std::uint64_t v = 0;

			int bits_read = 0;
			if (bit_offs % 8) {
				unsigned short o = bit_offs % 8;
				unsigned short d = 8 - o ; // d is the number of bits left in the byte to read
				v = *( ( (unsigned char*) data) + bit_offs/8);
				v = v >> o;
				if (d > bit_width) {
				 unsigned char c1=1;for(int i = 1; i < (int)bit_width;++i)c1 |= (c1 << 1);
				 v &= c1;
				 d = (unsigned short) bit_width;
				}
				bits_read = d;bit_offs+=d;
			}

			//INVARIANT: bit_offs points to a byte address
			int bits_read_after_byte_boundary=0;
			if (bits_read< (int)bit_width){
				std::uint64_t v_temp = 0;
				for(; bit_width-bits_read>=8;bit_offs+=8,bits_read+=8,bits_read_after_byte_boundary+=8){
					v_temp |=  ((std::uint64_t) *( ( (unsigned char*) data) +bit_offs/8)) << bits_read;
				}
				v |= v_temp;
			}
			//INVARIANT: bit_offs points to a byte address
			//INVARIANT: bit_width - bits_written < 8
			if (bits_read < (int)bit_width){
 			 std::uint64_t target = *( (unsigned char*)(data+ bit_offs/8) );
 			 int bit_left = bit_width-bits_read;
 			 std::uint64_t ch=1;
 			 for(int i = 1; i < bit_left;++i ) ch |= ch << 1;
 			 target &= ch;
 			 target = target << bits_read_after_byte_boundary;
 			 v |= target;
			}
			//std::cout << "bit offset:"<< orig_bit_offs <<"; bit_width:"<< bit_width<<"; " << v << "; " << ceps::ast::value(ceps::ast::as_int_ref(p)) << std::endl;
			if (p->kind() == ceps::ast::Ast_node_kind::int_literal)
			{
				if (bit_width == 1)
				{
				 if (ceps::ast::value(ceps::ast::as_int_ref(p)) != 0){
					if (v == 0) return -2;
				 } else if (v!=0) return -2;
			    }
			    else if ((std::int64_t)v != ceps::ast::value(ceps::ast::as_int_ref(p))) return -2;
			} else {
				std::lock_guard<std::recursive_mutex>g(smc->states_mutex());
				std::string s;
				auto w = smc->get_global_states()[s = ceps::ast::name(ceps::ast::as_symbol_ref(p))];
				smc->global_systemstates_prev()[s] = w;
                if (signed_value){
                    if (bit_width == 8) v = (std::int8_t)v;
                    else if (bit_width == 16) v = (std::int16_t)v;
                    else if (bit_width == 32) v = (std::int32_t)v;
                    else {
                        std::int64_t u = -1;
                        std::uint64_t uu = u;
                        uu = uu << bit_width;
                        v = ((std::uint64_t)v) | uu;
                    }
                }


				if (w == nullptr || w->kind() != ceps::ast::Ast_node_kind::int_literal) 
					smc->get_global_states()[s] =
						new ceps::ast::Int((std::int64_t)v, ceps::ast::all_zero_unit(), nullptr, nullptr, nullptr);
				else {
					auto old_value = ceps::ast::value(ceps::ast::as_int_ref(smc->get_global_states()[s]));
					if (old_value != (std::int64_t)v) smc->global_systemstates_prev()[s] = nullptr;
					ceps::ast::value(ceps::ast::as_int_ref(smc->get_global_states()[s])) = (std::int64_t)v;
				}
			}
		}
		return bit_width;
	}
	else if (p->kind() == ceps::ast::Ast_node_kind::float_literal) {
		 smc->fatal_(-1,"Floating point numbers not supported in raw frames.");
	}
	else if (p->kind() == ceps::ast::Ast_node_kind::string_literal){
		 smc->fatal_(-1,"Strings not supported in raw frames.");
	}
	else {std::stringstream ss; ss << *p; smc->fatal_(-1,"Reading of raw frame: Illformed expression:"+ss.str());}
	return 0;
}

int read_raw_chunk(size_t& header_length,State_machine_simulation_core* smc,
		            std::vector<ceps::ast::Nodebase_ptr> pattern,
		            size_t data_size,
		            char* data,
		            size_t bit_offs,
		            size_t bit_width,
		            bool signed_value,
		            bool read_data,
		            bool host_byte_order
		            ){
	size_t r=0;
	for(auto p : pattern){
		auto rr = read_raw_chunk(header_length,smc,p,data_size,data,bit_offs,bit_width,signed_value,read_data,host_byte_order);
		if (rr < 0) return rr;
		r+=rr;
		bit_offs += rr;
	}
	return r;
}


bool Podframe_generator::read_msg(char* data,size_t size,
		                          State_machine_simulation_core* smc,
                                  std::vector<std::string> ,
                                  std::vector<ceps::ast::Nodebase_ptr>& )
{
	if (data == nullptr || size == 0) return true;

	auto data_format = spec_["data"];
	if (data_format.nodes().empty()) {
		std::string id;
		auto t = spec_["id"];
		if (t.nodes().size() != 0 && t.nodes()[0]->kind() == ceps::ast::Ast_node_kind::identifier)
			id = ceps::ast::name(ceps::ast::as_id_ref(t.nodes()[0]));
		smc->fatal_(-1,"Frame '"+id+"' doesn't contain a data section.");
	}
	if (0 > read_raw_chunk(header_length_, smc,data_format.nodes(),size, data,0)) return false;
	return true;
}



void comm_sender_generic_tcp_out_thread(
 State_machine_simulation_core::frame_queue_t* frames,
 State_machine_simulation_core* smc,
 std::string ip,
 std::string port,
 std::string som,
 std::string eof,
 std::string sock_name,
 bool reuse_socket,
 bool reg_socket)
{
 DEBUG_FUNC_PROLOGUE2
 int cfd = -1;
 struct addrinfo hints;
 struct addrinfo *result, *rp;
 auto q = frames;
 bool conn_established = false;
 bool io_err = false;
 bool socket_owner = !reuse_socket;

 char* frame = nullptr;
 size_t frame_size = 0;
 bool pop_frame = true;
 State_machine_simulation_core::dispatcher_thread_ctxt_t* ctxt = nullptr;
 State_machine_simulation_core::frame_queue_elem_t frame_info;

 for(;;)
 {
  if (io_err && socket_owner){
   if(reg_socket){
	 std::lock_guard<std::recursive_mutex>g(smc->get_reg_sock_mtx());
	 smc->get_reg_socks()[sock_name] = std::tuple<int,bool>{-1,false};
	}
	close(cfd);
	io_err = false;
  }
  rp = nullptr;result = nullptr;

  if (pop_frame) {
   q->wait_and_pop(frame_info);
   frame_size = std::get<1>(frame_info);
   frame = (decltype(frame))std::get<1>(std::get<0>(frame_info));

  }
  auto options = std::get<0>(std::get<0>(frame_info));


  pop_frame = false;
  if (!conn_established)
  {
   if (reuse_socket){
   for(;;){
    {
	 std::lock_guard<std::recursive_mutex>g(smc->get_reg_sock_mtx());
	 auto it = smc->get_reg_socks().find(sock_name);
	 if (it != smc->get_reg_socks().end() && std::get<1>(it->second) && (!io_err || cfd != std::get<0>(it->second) )){
	  cfd = std::get<0>(it->second);
	  conn_established = true;io_err = false;
	  break;
	 }
	}
    std::this_thread::sleep_for(std::chrono::microseconds(1000));continue;
   }//for
  } else {
   for(;rp == nullptr;)
   {
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_canonname = NULL;    hints.ai_addr = NULL;    hints.ai_next = NULL;
    hints.ai_family = AF_INET;    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(ip.c_str(), port.c_str(), &hints, &result) != 0){
 	std::this_thread::sleep_for(std::chrono::microseconds(1000000));continue;
    }
    for (rp = result; rp != NULL; rp = rp->ai_next) {
    cfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (cfd == -1)	continue;
    if (connect(cfd, rp->ai_addr, rp->ai_addrlen) != -1)break;
    close(cfd);
   }
   if (result != nullptr) freeaddrinfo(result);
   if (rp == nullptr) {
    std::this_thread::sleep_for(std::chrono::microseconds(1000000));continue;
   }
  }//for
  conn_established = true;
  if(reg_socket){
   std::lock_guard<std::recursive_mutex>g(smc->get_reg_sock_mtx());
   smc->get_reg_socks()[sock_name] = std::tuple<int,bool>{cfd,true};
  }
  }
  if (reuse_socket && sock_name.length() && ctxt == nullptr){
   ctxt = smc->get_dispatcher_thread_ctxt(sock_name);
  }
 }

  auto len = frame_size;
  int wr = 0;

  if (som.size())
   if ( (wr = write(cfd, som.c_str(),eof.length() )) != (int)som.length())
   {
    io_err=true;
    conn_established=false;
    continue;
   }
  if (len && frame){
   if (ctxt->websocket_server()){
	bool fin = true;
	bool ext1_len = len >= 126 && len <= 65535;
	bool ext2_len = len > 65535;

    std::uint16_t header = 0;
    if (fin) header |= 0x80;
    if(!ext1_len && !ext2_len) header |= len << 8;
    else if (ext1_len) header |= 126 << 8;
    else header |= 127 << 8;
    if (options & Rawframe_generator::IS_BINARY) header |= 2;
    else if ( (options & Rawframe_generator::IS_ASCII) || (options & Rawframe_generator::IS_JSON)) header |= 1;
    if( (wr = write(cfd, &header,sizeof header )) != sizeof header){io_err=true;conn_established=false;continue;}
    if (ext1_len)
    {
     std::uint16_t v = len;v = htons(v);
     if( (wr = write(cfd, &v,sizeof v )) != sizeof v){io_err=true;conn_established=false;continue;}
    }
    if (ext2_len)
    {
     std::uint64_t v = len;v = htobe64(v);
     if( (wr = write(cfd, &v,sizeof v )) != sizeof v){io_err=true;conn_established=false;continue;}
    }
    if ( (wr = write(cfd, frame,len )) != (int)len){io_err=true;conn_established=false;continue;}
   } else {
    if ( (wr = write(cfd, frame,len )) != (int)len)
	{
	 io_err=true;
	 conn_established=false;
	 continue;
	}
   }
  }

  if (eof.size())
   if ( (wr = write(cfd, eof.c_str(),eof.length() )) != (int)eof.length())
   {
	io_err=true;
	conn_established=false;
	continue;
   }
   if (frame != nullptr) {delete[] frame;frame=nullptr;}
   pop_frame = true;
 }
 if (conn_established && socket_owner){
  if(reg_socket){
   std::lock_guard<std::recursive_mutex>g(smc->get_reg_sock_mtx());
   smc->get_reg_socks()[sock_name] = std::tuple<int,bool>{-1,false};
  }
  close(cfd);
 }
}


template<typename F> struct cleanup{
	F f_;
	cleanup(F f):f_(f){}
	~cleanup(){f_();}
};

static void comm_act_as_websocket_server(State_machine_simulation_core::dispatcher_thread_ctxt_t * ctx,
		State_machine_simulation_core* smc,sockaddr_storage claddr, int sck,std::string ev_id,std::string sock_name,bool reg_sock,bool reuse_sock){
 auto cleanup_f = [smc,reg_sock,sck,sock_name](){
  if (reg_sock){
	 	std::lock_guard<std::recursive_mutex> g(smc->get_reg_sock_mtx());
	 	smc->get_reg_socks()[sock_name] = std::tuple<int,bool>{-1,false};
  }
  close(sck);
 };
 cleanup<decltype(cleanup_f)> cl{cleanup_f};

 std::string unconsumed_data;
 auto rhr = read_http_request(sck,unconsumed_data);
 if (!std::get<0>(rhr)) return;
 auto const & attrs = std::get<2>(rhr);
 if (!field_with_content("Upgrade","websocket",attrs) || !field_with_content("Connection","Upgrade",attrs)) return;
 auto r = get_http_attribute_content("Sec-WebSocket-Key",attrs);
 if (!r.first)return;
 auto phrase = r.second+"258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
 unsigned char digest[CryptoPP::SHA::DIGESTSIZE];
 CryptoPP::SHA().CalculateDigest(digest, (unsigned char *)phrase.c_str(),phrase.length());
 auto hash = encode_base64(digest,CryptoPP::SHA::DIGESTSIZE);
 std::stringstream response;
 response
  << "HTTP/1.1 101 Switching Protocols\r\n"
  << "Upgrade: websocket\r\n"
  << "Connection: Upgrade\r\n"
  << "Sec-WebSocket-Accept: "
  << hash <<"\r\n\r\n";
 auto byteswritten = write(sck, response.str().c_str(),response.str().length());
 if (byteswritten == (ssize_t)response.str().length()){
  if (reg_sock){
	std::lock_guard<std::recursive_mutex> g(smc->get_reg_sock_mtx());
	smc->get_reg_socks()[sock_name] = std::tuple<int,bool>{sck,true};
  }
  for(;;){
   auto frm = read_websocket_frame(sck);
   if (!frm.first) break;
   std::vector<unsigned char> payload = std::move(frm.second.payload);
   while (!frm.second.fin){
    frm = read_websocket_frame(sck);
    if (!frm.first) break;
    payload.reserve(payload.size()+frm.second.payload.size());
    payload.insert(payload.end(),frm.second.payload.begin(),frm.second.payload.end());
   }
   if (!frm.first) break;
   if(frm.second.opcode == 1) {
    std::string s; s.resize(payload.size());for(size_t j = 0; j < payload.size();++j)s[j] = payload[j];//std::cout << s << std::endl;
    State_machine_simulation_core::event_t ev(ev_id);
    ev.already_sent_to_out_queues_ = true;
    ev.payload_.push_back(new ceps::ast::String(s));
 	smc->main_event_queue().push(ev);
   }
  }//for
 }
}

void comm_generic_tcp_in_thread_fn(
 int id,
 Rawframe_generator* gen,
 std::string ev_id,
 std::vector<std::string> params,
 State_machine_simulation_core* smc,
 sockaddr_storage claddr,
 int sck,
 std::string som,
 std::string eof,std::string sock_name,bool reg_sock,bool reuse_sock){

 DEBUG_FUNC_PROLOGUE2
 char host[1024] = {0};
 char service[1024] = {0};
 char addrstr[2048] = {0};
 socklen_t addrlen = sizeof(struct sockaddr_storage);

 if (getnameinfo((struct sockaddr*)&claddr,addrlen,host,1024,service,1024,0) == 0)
  snprintf(addrstr,2048,"[host=%s, service=%s]",host, service);
 else
  snprintf(addrstr,2048,"[host=?,service=?]");
 DEBUG << "[comm_generic_tcp_in_thread_fn][CONN_ESTABLISHED]" << addrstr << "\n";
 if (id >= 0 && ev_id.length() == 0){ // This is a generic fixed size input stream with a handler context
  auto ctxt = smc->get_dispatcher_thread_ctxt(id);
  std::uint32_t size = 0;
  char* buffer = nullptr;
  for(;!smc->shutdown();){
   std::uint32_t new_size = 0;
   auto r = recv(sck,(char*)&new_size,sizeof(new_size),0);
   if (r != sizeof(new_size)) break;
   new_size = ntohl(new_size);
   if (new_size > size){if (buffer) delete[] buffer; buffer = new char[size = new_size];}
   if(buffer == nullptr) {smc->fatal_(-1,"Couldn't allocate memory.");}
   r = recv(sck,buffer,size,0);
   DEBUG << "[comm_generic_tcp_in_thread_fn][DATA_READ][SIZE="<<new_size<<"]"<< "\n";
   std::vector<std::string> v1;
   std::vector<ceps::ast::Nodebase_ptr> v2;
   for(auto handler_info: ctxt->handler){
    auto match = handler_info.first->read_msg(buffer,new_size,smc,v1,v2);
	if (!match) continue;
	DEBUG << "[comm_generic_tcp_in_thread_fn][MATCH_FOUND]\n";
	smc->execute_action_seq(nullptr,handler_info.second);
   }
  }
  close(sck);if (buffer) delete[] buffer;
  return;
 } else if (id >= 0){
  auto ctxt = smc->get_dispatcher_thread_ctxt(id);
  if (ctxt->websocket_server()) comm_act_as_websocket_server(ctxt,smc,claddr,sck,ev_id,sock_name,reg_sock,reuse_sock);
  return;
 }

 size_t frame_size = 0;
 if (eof.length() == 0){
  bool failed_size_computation = true;
  for(;failed_size_computation;)
  {
   frame_size = gen->compute_size_of_msg(smc,params,failed_size_computation);
   if (failed_size_computation) std::this_thread::sleep_for(std::chrono::microseconds(1));
  }
  DEBUG << "[comm_generic_tcp_in_thread_fn][FRAME_SIZE_AVAILABLE]" << addrstr << "\n";
 } else DEBUG << "[comm_generic_tcp_in_thread_fn][EOF_MODE_DETECTED]\n";


 if (eof.length())
 {
 std::string last_suffix;
 for(;!smc->shutdown();){
  char buffer_[4096];
  std::stringstream* buffer = new std::stringstream;
  auto n = 0;
  DEBUG << "[comm_generic_tcp_in_thread_fn][READ...]\n";
  for(; (n = recv(sck,buffer_,4095,0)) > 0;)
  {
   DEBUG << "[comm_generic_tcp_in_thread_fn][DATA_RECEIVED][SIZE="<<n <<"]\n";
   buffer_[n] = 0;
   std::string temp(buffer_);
   if (temp == eof) {
   }
   std::string::size_type r=0;
   bool buffer_processed = false;
   do{
    auto new_r = temp.find(eof,r);
	buffer_processed = new_r == std::string::npos;
	*buffer << temp.substr(r,new_r-r );
	if (new_r != std::string::npos){
	 DEBUG << "[comm_generic_tcp_in_thread_fn][DATA_CHUNK_READ][SIZE="<<buffer->str().length() <<"]\n";
	 r = new_r+eof.length();
     bool decode_result = false;
	 std::vector<ceps::ast::Nodebase_ptr> payload;
	 {
	  std::lock_guard<std::recursive_mutex>g(smc->states_mutex());
	  decode_result = gen->read_msg((char*)buffer->str().c_str() +som.length(),
       buffer->str().length()-som.length(),
	   smc,
	   params,payload);
	  if (decode_result) DEBUG << "[comm_generic_tcp_in_thread_fn][DATA_SUCCESSFULLY_DECODED]\n";
	  else DEBUG << "[comm_generic_tcp_in_thread_fn][FAILED_TO_DECODE_DATA]\n";
	 }
     if (decode_result){
	  if (gen->frame_ctxt != nullptr){
	   auto clone = gen->frame_ctxt->clone();
	   State_machine_simulation_core::event_t ev;
	   ev.id_ = "@@framecontext";
	   ev.frmctxt_ = clone;
	   smc->enqueue_event(ev);
     }
   	State_machine_simulation_core::event_t ev(ev_id);
   	ev.already_sent_to_out_queues_ = true;
   	if (payload.size())
     ev.payload_ = payload;
    smc->main_event_queue().push(ev);
   }
   delete buffer;
   buffer =  new std::stringstream;
  }
 } while (!buffer_processed && r < temp.length());
}
delete buffer;
if (n<=0) break;
}
} else
	for(;!smc->shutdown() && frame_size;)
	{
		char *data = nullptr;
		try
		{
			DEBUG << "[comm_generic_tcp_in_thread_fn][READING_DATA]\n";
			data = new char[frame_size];
			if (data == nullptr){DEBUG << "[ERROR_comm_generic_tcp_in_thread_fn][ALLOC_FAILED]\n";close(sck);return;}
			ssize_t already_read = 0;
			ssize_t n = 0;
			for(; (already_read < (ssize_t)frame_size) && (n = recv(sck,data+already_read,(ssize_t)frame_size-already_read,0)) > 0;already_read+=n);

			if(already_read < (ssize_t)frame_size){DEBUG << "[ERROR_comm_generic_tcp_in_thread_fn][READ_FAILED]\n";close(sck);return;}
		    DEBUG << "[comm_generic_tcp_in_thread_fn][DATA_SUCCESSFULLY_READ]\n";


		    bool decode_result = false;
		    std::vector<ceps::ast::Nodebase_ptr> payload;
		    {
		    	std::lock_guard<std::recursive_mutex>g(smc->states_mutex());
		    	decode_result = gen->read_msg(data,frame_size,smc,params,payload);
		    	if (decode_result) DEBUG << "[comm_generic_tcp_in_thread_fn][DATA_SUCCESSFULLY_DECODED]\n";
		    	else DEBUG << "[comm_generic_tcp_in_thread_fn][FAILED_TO_DECODE_DATA]\n";
		    }

		    if (decode_result){
		    	State_machine_simulation_core::event_t ev(ev_id);
		    	ev.already_sent_to_out_queues_ = true;
		    	if (payload.size())
		    		ev.payload_ = payload;
		    	smc->main_event_queue().push(ev);
		    }




		} catch(...){}

		if (data) delete[] data;
	}
	close(sck);
}



void comm_generic_tcp_in_dispatcher_thread(int id,
				 Rawframe_generator* gen,
				 std::string ev_id,
				 std::vector<std::string> params,
			     State_machine_simulation_core* smc,
			     std::string ip,
			     std::string port,std::string som,std::string eof,std::string sock_name,bool reg_sock,bool reuse_sock,
			     void (*handler_fn) (int,Rawframe_generator*,std::string,std::vector<std::string> ,State_machine_simulation_core* , sockaddr_storage,int,std::string,std::string,std::string,bool,bool))
{
 std::vector<std::thread*> client_handler_threads;
 socklen_t addrlen = sizeof(struct sockaddr_storage);
 struct sockaddr_storage claddr = {0};
 int cfd = -1;

 if (reuse_sock){
  for(;;){
   {
	std::lock_guard<std::recursive_mutex>g(smc->get_reg_sock_mtx());
	auto it = smc->get_reg_socks().find(sock_name);
	if (it != smc->get_reg_socks().end() && std::get<1>(it->second)){
	 cfd = std::get<0>(it->second);
	 new std::thread(*handler_fn,id,gen,ev_id,params,smc,claddr,cfd,som,eof,sock_name,reg_sock, reuse_sock);
	 return;
	}
   }
   std::this_thread::sleep_for(std::chrono::microseconds(1000));continue;
  }//for
 }
 struct addrinfo hints = {0};
 struct addrinfo* result, * rp;
 int lfd = -1;

 hints.ai_canonname = nullptr;
 hints.ai_addr = nullptr;
 hints.ai_next = nullptr;
 hints.ai_socktype = SOCK_STREAM;
 hints.ai_family = AF_INET;
 hints.ai_flags = AI_PASSIVE;// | AI_NUMERICSERV;

 if (getaddrinfo(nullptr,port.c_str(),&hints,&result) != 0)
   	smc->fatal_(-1,"getaddrinfo failed");
 int optval=1;

 for(rp=result;rp;rp=rp->ai_next)
 {
  lfd = socket(rp->ai_family,rp->ai_socktype,rp->ai_protocol);
  if(lfd == -1) continue;
  if (setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,(char*)&optval,sizeof(optval))) smc->fatal_(-1,"setsockopt");
  if (bind(lfd,rp->ai_addr,rp->ai_addrlen) == 0) break;
  close(lfd);
 }
 if (!rp) smc->fatal_(-1,"comm_dispatcher_thread:Could not bind socket to any address.port="+port);
 if (listen(lfd,5)==-1)smc->fatal_(-1,"listen");
 freeaddrinfo(result);
 for(;!smc->shutdown();)
 {
  cfd = accept(lfd, (struct sockaddr*) &claddr, &addrlen);
  if (reg_sock){
   std::lock_guard<std::recursive_mutex> g(smc->get_reg_sock_mtx());
   smc->get_reg_socks()[sock_name] = std::tuple<int,bool>{cfd,false};
 }
 if (cfd == -1){
  continue;
 }
 if (handler_fn)
  client_handler_threads.push_back(new std::thread(*handler_fn,id,gen,ev_id,params,smc,claddr,cfd,som,eof,sock_name,reg_sock,reuse_sock));
 else close(cfd);
 }
 for(auto tp: client_handler_threads) tp->join();
}







