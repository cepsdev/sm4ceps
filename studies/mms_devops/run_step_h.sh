

../../x86/ceps rollout.ceps.lex myrollout.rll sm4myrollout.ceps --dot_gen --dot_gen_one_file_per_top_level_statemachine
for e in *.dot ; do
 dot $e -Tsvg -o web/${e%%.dot}.svg
done
../../x86/ceps rollout.ceps.lex rollout_scenario.ceps.lex myrollout.rll sm4myrollout_2.ceps scenario_1.rsc --dot_gen --dot_gen_one_file_per_top_level_statemachine --ws_api 1063 
