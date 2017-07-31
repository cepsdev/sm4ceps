

//Module requirements
const WebSocket = require('ws');
const os = require('os');
const { spawn,execFile} = require('child_process');
const fs = require('fs');
const path = require('path');
const express = require("express");
const http = require("http");
const chalk = require('chalk');

const host_name = os.hostname();


const ceps_executable = "./ceps";
const ceps_default_args = ["--quiet"];
const ceps_prelude = "../.ceps/prelude.ceps";
const ceps_publisher = "../.ceps/publisher.ceps";
const sim_nodes_root = "../sim_nodes";


let publish_port_start = 8181;
let publish_port_delta = 20;

let sim_core_counter = 0;
let sim_cores = [
  
];

let  sim_srcs = []; 

const command_port = 9191;
const command_ws_url = "ws://"+host_name+":"+command_port.toString();



function Sim_source() {
    this.path;
    this.pkg_info = {name:undefined,uri:undefined,author:undefined,modules:[]};
    this.main_port = undefined;
    this.pkgfile_content = undefined;
    this.name = undefined;
}
function Sim_source(name) {
    this.path;
    this.pkg_info = {name:name,uri:undefined,author:undefined,modules:[]};
    this.main_port = undefined;
    this.pkgfile_content = undefined;
    this.name = name;
}
function Sim_source(name,pkg_info) {
    this.path;
    this.pkg_info = pkg_info;
    this.main_port = undefined;
    this.pkgfile_content = undefined;
    this.name = name;
}

function instantiate_sim_info(srcs,cores){
 srcs.forEach( (src) => {
    let core = new Simcore(
     {
      url:"ws://"+host_name+":"+src.pkg_info.base_port.toString(),
      signal_url:"ws://"+host_name+":"+(src.pkg_info.base_port+1).toString(),
      command_url:"ws://"+host_name+":"+(src.pkg_info.base_port+10).toString(),
      ws:undefined,
      name:src.name,
      uri:src.pkg_info.uri,
      comm_layer : { frames : [] },
      index :  sim_core_counter++,
      src:src,
      process : undefined,
      process_launching : false
    });
    cores.push(core);
 });
}


function get_index_of_sim_src_by_name(srcs,name){
    for(let i = 0; i != srcs.length; ++i){
        if (srcs[i].name == name) return i;
    }
    return -1;
}

function get_index_of_sim_src_by_uri(srcs,name){
    for(let i = 0; i != srcs.length; ++i){
        if (srcs[i].pkg_info === undefined) continue;
        if (srcs[i].pkg_info.uri === name) return i;
    }
    return -1;
}

function uniquify_uri(srcs,name){
    let j = 0;
    let t = name;
    while(-1 != get_index_of_sim_src_by_uri(srcs,t) ){
        ++j;
        t = name + "_" + j.toString(); 
    }
    return t;
}

function uniquify_name(srcs,name){
    let j = 0;
    let t = name;
    while(-1 != get_index_of_sim_src_by_name(srcs,t) ){
        ++j;
        t = name + " (" + j.toString()+")"; 
    }
    return t;
}

function make_sim_src_info_given_sim_dir(directory,subd,uniquify_name_clbk,uniquify_uri_clbk ){
 let files = fs.readdirSync(path.join(directory,subd));
 let sim_src = undefined;
 files.forEach( (fname) => {
      if (fname == "package.json"){
          let pkg_json_path = path.join(directory,subd,"package.json");
          let pkg_ceps_path = path.join(directory,subd,"package.ceps");
          
          let content = fs.readFileSync(pkg_json_path, 'utf8');
          let jsn = JSON.parse(content);
          let modified = false;
          if (! ("name" in jsn) ){
              console.warn("***Warning. Skipping '"+pkg_json_path+"': no name field.");
              return;
          }
          let orig_name = jsn.name;
          jsn.name = uniquify_name_clbk(jsn.name);
          modified = jsn.name != orig_name; 
          if (! ("modules" in jsn) || jsn.modules.length === undefined || jsn.modules.length === 0 ){
              console.warn("***Warning. Skipping '"+pkg_json_path+"': 'modules' field missing or illformed.");
              return;
          }
          if (!("uri" in jsn)){
              modified = true;
              let s = jsn.name;
              s = uniquify_uri_clbk(s.replace(/\s/g,"_").replace(/\(/g,"").replace(/\)/g,"").replace(/\//g,"").replace(/\./g,"").replace(/:/g,"") );
              console.warn("***Warning. No 'uri' field in '"+pkg_json_path+"': a default value was generated '"+s+"'.");
              jsn.uri = s;
          }
          if (!("base_port" in jsn)){
              modified = true;
              jsn.base_port = publish_port_start;
              publish_port_start += publish_port_delta;
          }

          sim_src = new Sim_source(jsn.name,jsn);
         
          if (modified || !fs.existsSync(pkg_ceps_path)){
           let d = new Date(Date.now());
           let fromatted = d.toLocaleString();
           fs.writeFileSync(pkg_ceps_path,
           "/* Generated by cepscloud@Node.js "+fromatted+" */\npackage {\n"+
           "  name { \""+sim_src.name+"\"; };\n"+
           "  uri { \""+sim_src.pkg_info.uri+"\"; };\n"+
           "};\n"+
           "val publisher_baseport = \""+sim_src.pkg_info.base_port+"\";\n"+
           "val publisher_cmd_port = \""+(sim_src.pkg_info.base_port+10)+"\";\n"+
           "val publisher_signal_port = \""+(sim_src.pkg_info.base_port+1)+"\";\n",           
           'utf8');
          }
          sim_src.path = subd;
      }
  } ); 
 return sim_src;
}

function walk_dir_and_fetch_sim_src_infos(directory){
 let subds = fs.readdirSync(directory);
 let r = [];
 if (subds.length == 0) return r;
 subds.forEach( (subd) => {
  let sim_src = make_sim_src_info_given_sim_dir(directory,subd,(s)=>{return uniquify_name(r,s);},(s)=>{return uniquify_uri(r,s);});
  if (sim_src != undefined) r.push(sim_src);
 });
 return r;
}

function Simcore(){
 this.url=undefined;
 this.signal_url=undefined;
 this.command_url=undefined;
 this.ws=undefined;
 this.name=undefined;
 this.uri=undefined;
 this.comm_layer = { frames : [] };
 this.index =undefined;
 this.src = undefined;
 this.process = undefined;
 this.process_launching = false;
 this.dont_launch = false;
}

function Simcore(p){
 this.url=p.url;
 this.signal_url=p.signal_url;
 this.command_url=p.command_url;
 this.ws=p.ws;
 this.name=p.name;
 this.uri=p.uri;
 this.comm_layer = p.comm_layer;
 this.index =p.index;
 this.src = p.src;
 this.process = p.process;
 this.process_launching = false;
 this.dont_launch = false;
}

Simcore.prototype.get_status = function () { return "N/A";}
Simcore.prototype.get_description = function () { return "N/A";}

/*sim_cores.push(new Simcore(  { 
      url:"ws://"+host_name+":8181",
      signal_url:"ws://"+host_name+":8182",
      command_url:"ws://"+host_name+":8192",
      ws:undefined,
      name:"Not Available",
      uri:"?",
      comm_layer : { frames : [] },
      index :  sim_core_counter
    }));
*/

function get_sim_core_by_name(name){
 for(let e of sim_cores)
     if (e.name === name) return e;
 return undefined;
}

function get_sim_core_by_uri(uri){
 for(let e of sim_cores)
     if (e.uri === uri) return e;
 return undefined;
}

function sim_core_init(sim_core){

 let frame_names_received = (msg) => {
     sim_core.ws.removeListener('message', frame_names_received);
     sim_core.comm_layer.frames = JSON.parse(msg);
     sim_core.index = ++ sim_core_counter;
     console.log("Simulation Core '"+sim_core.name+"'@"+sim_core.uri+" online.");
     let frame_counter = 0;
     let sig_counter = 0;
     sim_core.signals = [];
     for(let frame of sim_core.comm_layer.frames){
         frame.index = frame_counter++;
         for(let sig of frame.signals){
             sig.index = sig_counter++;
             sim_core.signals.push(sig.name);             
         }
         //console.log(frame);
     }
    console.log(sim_core.ws._socket.remoteAddress);
 };
 let sim_uri_received = (msg) => {
    sim_core.uri = msg;
    sim_core.ws.removeListener('message', sim_uri_received);
    sim_core.ws.on("message",frame_names_received);
    sim_core.ws.send('comm_layer_frames',{});
 };
 
 let sim_name_received = (msg) =>  {
    sim_core.name = msg;
    sim_core.ws.removeListener('message', sim_name_received);
    sim_core.ws.on("message",sim_uri_received);
    sim_core.ws.send('sim_uri',{});
 };

 sim_core.ws.on("message",sim_name_received);
 sim_core.ws.send('sim_name',{});
}


function check_remote_sim_cores() {
    for(let core_info of sim_cores){
     if (core_info.dont_launch){
         if (core_info.ws) core_info.ws.close();
         core_info.ws = undefined;
         continue;
     }
     if (core_info.process === undefined){
         core_info.process_launching = true;          
         if (core_info.ws) {core_info.ws.close();}
         core_info.ws = undefined;

         let call_sequence = []; 
         call_sequence.push(`${ceps_prelude}`);
         call_sequence.push(path.join(`${sim_nodes_root}`,`${core_info.src.path}`,"package.ceps"));
         core_info.src.pkg_info.modules.forEach( (f) => {call_sequence.push(path.join(`${sim_nodes_root}`,`${core_info.src.path}`,`${f}`)); });
         call_sequence.push(`${ceps_publisher}`);
         call_sequence.push(path.join(`${sim_nodes_root}`,`${core_info.src.path}`,"driver.ceps"));
         call_sequence = call_sequence.concat(ceps_default_args);
         console.log(chalk.bold(`${ceps_executable} `,call_sequence.join(" ")));
         let p = spawn(`${ceps_executable}`,call_sequence);
         p.stdout.on('data', (data) => {
            console.log(chalk.yellow(`${data}`));
         });
         p.stderr.on('data', (data) => {
            console.log(chalk.redBright(`${data}`));
         });
         p.on('close', (code) => {
            core_info.process = undefined;
            core_info.process_launching = false;
            core_info.dont_launch = true;
            console.log(chalk.red(`Simulation Core "${core_info.src.name}" exited with code ${code}`));
         });
        core_info.process = p;
        setTimeout(() => {core_info.process_launching=false;},3000);
        //Overview generation
        let overview_gen_script = "";
        let modules = "";
        for(let i = 0; i != core_info.src.pkg_info.modules.length ; ++i){
          let m = core_info.src.pkg_info.modules[i];
          modules += " " + m;
        }

        //overview_gen_script += "cp make_report.ceps "+path.join(sim_nodes_root,core_info.src.path)+"\n";
        fs.writeFileSync(path.join(sim_nodes_root,core_info.src.path,"make_report.ceps"), 'make_stddoc{out{"overview.ejs";};img_path_prefix{"img/'+core_info.src.pkg_info.uri+'/";}; };');
        overview_gen_script += "cp make_svgs.sh "+path.join(sim_nodes_root,core_info.src.path)+"\n";
        overview_gen_script += "cp basic_style.ceps "+path.join(sim_nodes_root,core_info.src.path)+"\n";
        overview_gen_script += "cp dot_props.ceps "+path.join(sim_nodes_root,core_info.src.path)+"\n";
        overview_gen_script += 'cd '+path.join(sim_nodes_root,core_info.src.path)+"\npwd\n";        
        overview_gen_script += '../../server/ceps ../../.ceps/prelude.ceps basic_style.ceps dot_props.ceps '+modules+' --ignore_simulations --dot_gen --dot_gen_one_file_per_top_level_statemachine --post_processing make_report.ceps\n';
        overview_gen_script += "./make_svgs.sh\n";
        overview_gen_script += "mkdir -p ../../server/public/img/"+core_info.src.pkg_info.uri+"\n";
        overview_gen_script += "cp *.svg ../../server/public/img/"+core_info.src.pkg_info.uri+"\n";
        

        fs.writeFileSync("gen_overview_"+core_info.src.pkg_info.uri+".sh",overview_gen_script);
        let overview_gen_process = spawn("sh",["gen_overview_"+core_info.src.pkg_info.uri+".sh"]);
         overview_gen_process.stdout.on('data', (data) => {
            console.log(chalk.yellow(`${data}`));
         });
         overview_gen_process.stderr.on('data', (data) => {
            console.log(chalk.yellow(`${data}`));
         });
         overview_gen_process.on('close', (code) => {
            console.log(chalk.yellow("gen_overview_"+core_info.src.pkg_info.uri+".sh exited with code "+`${code}`));
         });
        

     } else if (core_info.ws === undefined && !core_info.process_launching){
         core_info.ws = new WebSocket(core_info.url);
         core_info.ws.on("error", () => {core_info.ws=undefined;} );
         core_info.ws.on("open", () => {sim_core_init(core_info);} );
         core_info.ws.on("close", () => {core_info.ws=undefined;} );
     }
    }
}

let app = express();
let publicPath = path.resolve(__dirname, "public");

app.set("views", path.resolve(__dirname, "views"));
app.set("view engine", "ejs");

app.use(express.static(publicPath));

app.get("/", function(req, res) {
    res.render("index",{ page_title:"Home",
                         sim_cores : sim_cores,
                         sim_core : undefined,
                        command_ws_url:command_ws_url});
});

app.get(/^\/(signaldetails__([0-9]+)__([0-9]+))|(\w*)$/, function(req, res) {
 if (req.params[3] != undefined) {
    let score = get_sim_core_by_uri(req.params[3]);
    if (score != undefined) {
         res.render("sim_main",{ page_title: score.name,
                                 sim_core : score, command_ws_url:command_ws_url,sim_nodes_root:sim_nodes_root }); 
    }
 } else {
   let score_idx = req.params[1];
   let sig_idx = req.params[2];
   let score = undefined;
   for(let s of sim_cores) if (s.index == score_idx){ score=s;break; }
   if (score === undefined) {res.status=404;res.send("404");return;} 
   let signalname = score.signals[sig_idx];
   if (signalname === undefined) {res.status=404;res.send("404");return;}
   let sig = undefined;
   for(let f of score.comm_layer.frames)
    for (let s of f.signals) if (s.index == sig_idx){sig = s;break;}
   if (sig === undefined) {res.status=404;res.send("404");return;}
   
   res.render("signal_details",{ page_title: score.name +"-"+ signalname,
                                 sim_core : score,
                                 signal:sig,
                                 signal_ws:score.signal_url});   
 }
});




function create_fibex_based_simulation(client_msg,ws){
    //let sim_src = make_sim_src_info_given_sim_dir(directory,subd,(s)=>{return uniquify_name(r,s);},(s)=>{return uniquify_uri(r,s);});
    fs.readdir(sim_nodes_root,(err,files)=>{
        if (err){ws.send(JSON.stringify({ok:false}));return;}
        function find_match (s){for(let i = 0; i != files.length;++i)if(files[i]==s)return i;return -1;}
        let i=0;
        let s = client_msg.name;
        s = s.replace(/\s/g,"_").replace(/\//g,"").replace(/\./g,"").replace(/:/g,"");
        let subd=s;
        while(0 <= find_match(subd)){
            ++i;
            subd = s+"_"+i.toString();
        }
        fs.mkdir(path.join(sim_nodes_root,subd),(err)=>{
          if (err){ws.send(JSON.stringify({ok:false}));return;}
          let pkg_json_sent = false;
          let driver_ceps_sent = false; 
          for(let i = 0; i!=client_msg.file_names.length;++i){
              if(client_msg.file_names[i] === "package.json") pkg_json_sent = true;
              if(client_msg.file_names[i] === "driver.ceps") driver_ceps_sent = true;
              fs.writeFileSync(path.join(sim_nodes_root,subd,client_msg.file_names[i]),client_msg.data[i]);
          }
          ws.send(JSON.stringify({ok:true}));
          if (!pkg_json_sent){
              let jsn = {name:client_msg.name,modules:[]};
              for(let i = 0; i!=client_msg.file_names.length;++i){
                  if (client_msg.file_names[i].match(/xml$/)) jsn.modules.push(client_msg.file_names[i]);
              }
              for(let i = 0; i!=client_msg.file_names.length;++i){
                  if (!client_msg.file_names[i].match(/xml$|driver\.ceps/)) jsn.modules.push(client_msg.file_names[i]);
              }
              jsn.modules.push("driver.ceps");  
              fs.writeFileSync(path.join(sim_nodes_root,subd,"package.json"),JSON.stringify(jsn));
          }
          if(!driver_ceps_sent) fs.writeFileSync(path.join(sim_nodes_root,subd,"driver.ceps"),"Simulation{};");
          let sim_src = make_sim_src_info_given_sim_dir(sim_nodes_root,subd,(s)=>{return uniquify_name(sim_srcs,s);},(s)=>{return uniquify_uri(sim_srcs,s);});
          if (sim_src == undefined)
            ws.send(JSON.stringify({ok:false}));
          else
            setTimeout( () => { 
                ws.send(JSON.stringify({ok:true}));
                instantiate_sim_info([sim_src],sim_cores);
                let sim_core = sim_cores[sim_cores.length-1];
                setTimeout( () => {
                    if (sim_core.process != undefined) ws.send(JSON.stringify({ok:true,uri:sim_core.uri}));
                    else {
                        ws.send(JSON.stringify({ok:false}));
                    }
                }, 3000);
            }, 3000 );
          console.log (sim_src);
        });
    });
}

const ws_command = new WebSocket.Server({port: command_port});
ws_command.on("connection", function connection(ws){
    ws.on("message",function incoming(message){
        let msg = JSON.parse(message);
        let sim_src = undefined;
        if (msg.cmd == "create_fibex_based_simulation"){
           create_fibex_based_simulation(msg,ws);       
          //ws.send(JSON.stringify({ok:true}));  
        }
    });
});


sim_srcs = walk_dir_and_fetch_sim_src_infos("../sim_nodes");
instantiate_sim_info(sim_srcs,sim_cores);
check_remote_sim_cores();
setInterval(check_remote_sim_cores,500);
http.createServer(app).listen(3000);
