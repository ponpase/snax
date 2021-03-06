# !/bin/bash

pkill snaxnode
clisnax wallet stop
rm -rf /tmp/snaxnode_data
rm -rf ~/snax-wallet
./snaxnode.sh &

sleep 5
clisnax wallet create --file wall
clisnax wallet import --private-key 5HvtgZn4wf4vNAe3nRb9vjYfLqvasemsSQckVHxmdAeBRbdPURs
clisnax wallet import --private-key 5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3
clisnax wallet import --private-key 5JD9AGTuTeD5BXZwGQ5AtwBqHK21aHmYnTetHgk1B3pjj7krT8N
clisnax wallet import --private-key 5JLYkoKuNXGGvUtzjRnP8DqUwt7xny3YGVaDpeqFDdCJKBoBkNC

clisnax create account snax test1 SNAX5yd9aufDv7MqMquGcQdD6Bfmv6umqSuh9ru3kheDBqbi6vtJ58 SNAX5yd9aufDv7MqMquGcQdD6Bfmv6umqSuh9ru3kheDBqbi6vtJ58
clisnax create account snax platform SNAX6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV SNAX6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
clisnax create account snax test2 SNAX5btzHW33f9zbhkwjJTYsoyRzXUNstx1Da9X2nTzk8BQztxoP3H SNAX5btzHW33f9zbhkwjJTYsoyRzXUNstx1Da9X2nTzk8BQztxoP3H
clisnax create account snax snax.token SNAX6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV SNAX6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
clisnax create account snax snax.msig SNAX6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV SNAX6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
clisnax create account snax snax.stake SNAX6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV SNAX6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
clisnax create account snax snax.ramfee SNAX6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV SNAX6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
clisnax create account snax snax.ram SNAX6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV SNAX6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
clisnax create account snax snax.team SNAX5zwaVbLo5Jj6Uhota1c9uUNT1nuNBuv5RoGawEMYra2v9iGM3h SNAX5zwaVbLo5Jj6Uhota1c9uUNT1nuNBuv5RoGawEMYra2v9iGM3h
clisnax create account snax snax.airdrop SNAX6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV SNAX6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
clisnax create account snax snax.creator SNAX6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV SNAX6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV
clisnax create account snax snax.bpay SNAX82vokU8bz112PQueGtpG6qkW2tXXrAGPjsVjsNEkZRXaGH4xTe SNAX82vokU8bz112PQueGtpG6qkW2tXXrAGPjsVjsNEkZRXaGH4xTe
clisnax create account snax snax.vpay SNAX82vokU8bz112PQueGtpG6qkW2tXXrAGPjsVjsNEkZRXaGH4xTe SNAX82vokU8bz112PQueGtpG6qkW2tXXrAGPjsVjsNEkZRXaGH4xTe

clisnax set contract snax ../build/contracts/snax.bios -p snax@owner
clisnax set contract snax.msig ../build/contracts/snax.msig -p snax.msig@owner
clisnax push action snax setpriv '["snax.token", 1]' -p snax@active
clisnax push action snax setpriv '["platform", 1]' -p snax@active
clisnax push action snax setpriv '["snax.airdrop", 1]' -p snax@active
clisnax set contract snax.token ../build/contracts/snax.token -p snax.token@owner
clisnax push action snax.token create '["snax", "1000000000000.0000 SNAX"]' -p snax.token@active
clisnax set contract snax ../build/contracts/snax.system -p snax@owner -x 1000
clisnax push action snax.token issue '["snax.airdrop", "100000.0000 SNAX", "airdrop"]' -p snax@active
clisnax push action snax setplatforms '[[{ "account": "platform", "period": 1, "weight": 1, "quotas": {  "cpu_weight": 1500000, "net_weight": 150000, "ram_bytes": 10000000 } }, { "account": "test2", "period": 1, "weight": 0, "quotas": {  "cpu_weight": 1500000, "net_weight": 150000, "ram_bytes": 10000000 } }]]' -p snax@active
clisnax push action snax setparams '[{   "max_block_net_usage": 1048576,"target_block_net_usage_pct": 1000,"max_transaction_net_usage": 524288,"base_per_transaction_net_usage": 12,"net_usage_leeway": 500,"context_free_discount_net_usage_num": 20,"context_free_discount_net_usage_den": 100,"max_block_cpu_usage": 150000000,"target_block_cpu_usage_pct": 1000,"max_transaction_cpu_usage": 15000000,"min_transaction_cpu_usage": 100, "resources_market_open": 1, "privileged_contracts": 0,"contract_owner":1,"platform_lock_duration":14400,"max_transaction_lifetime": 15000,"deferred_trx_expiration_window": 5000,"max_transaction_delay": 3888000,"max_inline_action_size": 4096,"max_inline_action_depth": 4,"max_authority_depth": 6,"max_ram_size": "68719476736", "platform1": {"account": "platform", "weight": 1, "period": 1 } }]' -p snax@owner
clisnax set contract platform ../build/contracts/platform -p platform@owner
clisnax set contract snax.airdrop ../build/contracts/snax.airdrop -p snax.airdrop@owner
