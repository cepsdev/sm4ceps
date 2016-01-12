#!/bin/bash
echo -e "\e[104mRunning sm4ceps tests\e[0m"
echo 
echo -en "Test \e[4msystemparameter.ceps\e[0m                               :"
../x86/sm systemparameter.ceps --quiet >systemparameter.log 2>systemparameter.log
if [ $? -ne 0 ]; then
 echo -e "\e[38;5;196m failed \e[0m"
else
 echo -e "\e[38;5;28m passed \e[0m"
fi

echo -en "Test \e[4mthreads1.ceps\e[0m                                      :"
../x86/sm threads1.ceps --quiet >threads1.log 2>>threads1.log
if [ $? -ne 0 ]; then
 echo -e "\e[38;5;196m failed \e[0m"
else
 echo -e "\e[38;5;28m passed \e[0m"
fi

echo -en "Test \e[4mplatform_extend_fto_and_alu.ceps\e[0m                   :"
../x86/sm platform_extend_fto_and_alu.ceps --quiet >platform_extend_fto_and_alu.log 2>platform_extend_fto_and_alu.log
if [ $? -ne 0 ]; then
 echo -e "\e[38;5;196m failed \e[0m"
 echo -e "\e[38;5;196m"
 cat platform_extend_fto_and_alu.log
 echo -e "\e[0m"
else
 echo -e "\e[38;5;28m passed \e[0m"
fi

echo -en "Test \e[4mepsilon_transitions1.ceps\e[0m                          :"
../x86/sm epsilon_transitions1.ceps --quiet >epsilon_transitions1.log 2>epsilon_transitions1.log
if [ $? -ne 0 ]; then
 echo -e "\e[38;5;196m failed \e[0m"
 echo -e "\e[38;5;196m"
 cat epsilon_transitions1.log
 echo -e "\e[0m"
else
 echo -e "\e[38;5;28m passed \e[0m"
fi


echo -en "Test \e[4mconditionals1.ceps\e[0m                                 :"
../x86/sm conditionals1.ceps --quiet >conditionals1.log 2>conditionals1.log
if [ $? -ne 0 ]; then
 echo -e "\e[38;5;196m failed \e[0m"
 echo -e "\e[38;5;196m"
 cat conditionals1.log
 echo -e "\e[0m"
else
 echo -e "\e[38;5;28m passed \e[0m"
fi

echo -en "Test \e[4mon_enter_semantics.ceps\e[0m                           :"
../x86/sm on_enter_semantics.ceps --quiet >on_enter_semantics.log 2>on_enter_semantics.log
if [ $? -ne 0 ]; then
 echo -e "\e[38;5;196m failed \e[0m"
else
 echo -e "\e[38;5;28m passed \e[0m"
fi

echo -en "Test \e[4mdistributed_native_1_single_simulation_core\e[0m        :"
../x86/sm distributed_native_1_node_a.ceps distributed_native_1_node_b.ceps simulation_distributed_native_1_run_both_nodes_on_one_sim_core.ceps >distributed_native_1_single_simulation_core.log 2>distributed_native_1_single_simulation_core.log
if [ $? -ne 0 ]; then
 echo -e "\e[38;5;196m failed \e[0m"
 echo -e "\e[38;5;196m"
 cat distributed_native_1_single_simulation_core.log
 echo -e "\e[0m"
else
 echo -e "\e[38;5;28m passed \e[0m"
fi

echo -en "Test \e[4mdistributed_native_1_two_simulation_cores\e[0m          :"

../x86/sm distributed_native_1_node_a.ceps --server --rip127.0.0.1 --port4001 --rport4002\
 simulation_distributed_native_1_run_node_a.ceps >simulation_distributed_native_1_run_node_a.log 2>simulation_distributed_native_1_run_node_a.log &
my_pid1=$!
../x86/sm distributed_native_1_node_b.ceps --server --rip127.0.0.1 --port4002 --rport4001\
 simulation_distributed_native_1_run_node_b.ceps >simulation_distributed_native_1_run_node_b.log 2>simulation_distributed_native_1_run_node_b.log &
my_pid2=$!
#$(sleep 5;kill $my_pid1;kill $my_pid2) &
wait $my_pid1 
exit_status_1=$?
wait $my_pid2 
exit_status_2=$?
if [ $exit_status_1 -ne 0 ] || [ $exit_status_2 -ne 0 ]; then
 echo -e "\e[38;5;196m failed \e[0m"
 echo -e "\e[38;5;196m"
 if [ $exit_status_1 -ne 0 ]; then
  cat simulation_distributed_native_1_run_node_a.log
 fi
 if [ $exit_status_2 -ne 0 ]; then
  cat simulation_distributed_native_1_run_node_b.log
 fi
 echo -e "\e[0m"
else
 echo -e "\e[38;5;28m passed \e[0m"
fi

echo -en "Test \e[4mdistributed_raw_frame_1_two_simulation_cores\e[0m       :"

../x86/sm distributed_native_1_node_a.ceps \
 distributed_raw_frame_1_message_definition.ceps channels_node_a_rawframe.ceps simulation_distributed_native_1_run_node_a.ceps >distributed_raw_frame_1_one_simulation_core_node_a.log 2>distributed_raw_frame_1_one_simulation_core_node_a.log &
my_pid1=$!
../x86/sm distributed_native_1_node_b.ceps \
 distributed_raw_frame_1_message_definition.ceps channels_node_b_rawframe.ceps simulation_distributed_native_1_run_node_b.ceps >distributed_raw_frame_1_one_simulation_core_node_b.log 2>distributed_raw_frame_1_one_simulation_core_node_b.log &
my_pid2=$!
#$(sleep 5;kill $my_pid1;kill $my_pid2) &
wait $my_pid1 
exit_status_1=$?
wait $my_pid2 
exit_status_2=$?
if [ $exit_status_1 -ne 0 ] || [ $exit_status_2 -ne 0 ]; then
 echo -e "\e[38;5;196m failed \e[0m"
 echo -e "\e[38;5;196m"
 if [ $exit_status_1 -ne 0 ]; then
  cat distributed_raw_frame_1_one_simulation_core_node_a.log
 fi
 if [ $exit_status_2 -ne 0 ]; then
  cat distributed_raw_frame_1_one_simulation_core_node_b.log
 fi
 echo -e "\e[0m"
else
 echo -e "\e[38;5;28m passed \e[0m"
fi

echo -en "Test \e[4mdistr_via_canbus_loopback_1_two_simulation_cores\e[0m   :"

../x86/sm distributed_native_1_node_b.ceps \
 distributed_raw_frame_1_message_definition.ceps channels_node_b_rawframe_canbus_loopback.ceps simulation_distributed_native_1_run_node_b.ceps >distributed_raw_frame_1_one_simulation_core_node_b_canbus.log 2>distributed_raw_frame_1_one_simulation_core_node_b_canbus.log &
my_pid2=$!
sleep 2
../x86/sm distributed_native_1_node_a.ceps distributed_raw_frame_1_message_definition.ceps channels_node_a_rawframe_canbus_loopback.ceps simulation_distributed_native_1_run_node_a.ceps >distributed_raw_frame_1_one_simulation_core_node_a_canbus.log 2>distributed_raw_frame_1_one_simulation_core_node_a_canbus.log &
my_pid1=$!

#$(sleep 5;kill $my_pid1;kill $my_pid2) &
wait $my_pid1 
exit_status_1=$?
wait $my_pid2 
exit_status_2=$?
if [ $exit_status_1 -ne 0 ] || [ $exit_status_2 -ne 0 ]; then
 echo -e "\e[38;5;196m failed \e[0m"
 echo -e "\e[38;5;196m"
 if [ $exit_status_1 -ne 0 ]; then
  cat distributed_raw_frame_1_one_simulation_core_node_a_canbus.log
 fi
 if [ $exit_status_2 -ne 0 ]; then
  cat distributed_raw_frame_1_one_simulation_core_node_b_canbus.log
 fi
 echo -e "\e[0m"
else
 echo -e "\e[38;5;28m passed \e[0m"
fi


# echo -en "Test \e[4mplugin_support\e[0m                                     :"
# cd plugin_sample_1
# make clean > /dev/null 2> /dev/null
# make > plugin_support_make_step.log 2> plugin_support_make_step.log
# if [ $? -ne 0 ]; then
#  echo -e "\e[38;5;196m failed (build step)"
#  cat plugin_support_make_step.log
#  echo -e "\e[0m"
# else
#   ../../x86/sm example.ceps --quiet --pluginAntriebe.so > plugin_support_run_step.log 2> plugin_support_run_step.log
#  if [ $? -ne 0 ]; then
#   echo -e "\e[38;5;196m failed \e[0m"
#  else
#   echo -e "\e[38;5;28m passed \e[0m"
#  fi
# fi
# 
# cd ..


