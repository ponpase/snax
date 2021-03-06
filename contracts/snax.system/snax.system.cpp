#include "snax.system.hpp"
#include <snaxlib/dispatcher.hpp>

#include "producer_pay.cpp"
#include "delegate_bandwidth.cpp"
#include "voting.cpp"
#include "exchange_state.cpp"
#include <math.h>


namespace snaxsystem {

   system_contract::system_contract( account_name s )
   :native(s),
    _platforms(_self, _self),
    _voters(_self,_self),
    _producers(_self,_self),
    _global(_self,_self),
    _rammarket(_self,_self)
   {
      //print( "construct system\n" );
      _gstate = _global.exists() ? _global.get() : get_default_parameters();

      auto itr = _rammarket.find(S(4,RAMCORE));

      if( itr == _rammarket.end() ) {
         const int64_t system_token_soft_supply_limit = snax::token(N(snax.token)).get_max_supply(snax::symbol_type(system_token_symbol).name()).amount / 10;
         if( system_token_soft_supply_limit > 0 ) {
            itr = _rammarket.emplace( _self, [&]( auto& m ) {
               m.supply.amount = 100000000000000ll;
               m.supply.symbol = S(4,RAMCORE);
               m.base.balance.amount = int64_t(_gstate.free_ram());
               m.base.balance.symbol = S(0,RAM);
               m.quote.balance.amount = system_token_soft_supply_limit / 7500;
               m.quote.balance.symbol = CORE_SYMBOL;
            });
         }
      } else {
         //print( "ram market already created" );
      }

      if ( !_gstate.initialized ) {
        const asset amount_to_issue = asset(
            staked_by_team_initial
            + team_memory_initial
            + team_balance_initial
            + account_creator_initial
            + airdrop_initial
        );

        INLINE_ACTION_SENDER(snax::token, issue)(
            N(snax.token), {_self,N(active)},
            {
                _self,
                amount_to_issue,
                "premine"
            }
        );

        INLINE_ACTION_SENDER(snax::token, transfer)(
            N(snax.token), {_self,N(active)},
            {
                _self,
                N(snax.team),
                asset(team_balance_initial),
                "initial team balance"
            }
        );

        INLINE_ACTION_SENDER(system_contract, buyram)(
            _self, {_self, N(active)},
            {
                _self,
                N(snax.team),
                asset(team_memory_initial)
            }
        );

        INLINE_ACTION_SENDER(system_contract, escrowbw)(
            _self, {_self, N(active)},
            {
                _self,
                N(snax.team),
                asset(staked_by_team_initial / 5),
                asset(staked_by_team_initial / 5 * 4),
                true,
                10
            }
        );

        INLINE_ACTION_SENDER(snax::token, transfer)(
            N(snax.token), {_self,N(active)},
            {
                _self,
                N(snax.airdrop),
                asset(airdrop_initial),
                "airdrop"
            }
        );

        INLINE_ACTION_SENDER(snax::token, transfer)(
            N(snax.token), {_self,N(active)},
            {
                _self,
                N(snax.creator),
                asset(account_creator_initial),
                "account creation"
            }
        );

        _gstate.initialized = true;
        _global.set(_gstate, _self);
      }

   }

   snax_global_state system_contract::get_default_parameters() {
      snax_global_state dp;
      get_blockchain_parameters(dp);
      return dp;
   }

   system_contract::~system_contract() {
      //print( "destruct system\n" );
      _global.set( _gstate, _self );
      //snax_exit(0);
   }

   void system_contract::lockplatform( account_name& platform ) {
        require_auth(platform);

        const auto current_time = snax::time_point_sec(now());

        _platform_locks platform_locks(_self, platform);

        platform_locks.emplace(platform, [&](auto& record) {
            record.time = block_timestamp(current_time);
        });
   }

   void system_contract::emitplatform( account_name& platform ) {
        require_auth(platform);

        _platform_requests platform_requests(_self, platform);
        _platform_locks platform_locks(_self, platform);

        auto found_config = _platforms.end();

        asset platform_full_balance = get_platform_full_balance();
        int64_t offset_of_round = 1;

        for ( auto platform_conf = _platforms.begin(); platform_conf != _platforms.end(); platform_conf++ ) {
            if (offset_of_round < platform_conf->period) {
                offset_of_round = platform_conf->period;
            }
            if (platform_conf->account == platform) {
                found_config = platform_conf;
            }
        }

        snax_assert(found_config != _platforms.end(), "platform not found in platforms config");

        const auto current_time = snax::time_point_sec(now());

        const asset system_supply_soft_limit = asset(snax::token(N(snax.token)).get_max_supply(snax::symbol_type(system_token_symbol).name()).amount / 10);

        if (platform_requests.cbegin() != platform_requests.cend()) {
           auto last_request = platform_requests.end();
           snax_assert(
               (--last_request)->request
                    .to_time_point()
                    .time_since_epoch()
                    .to_seconds()
                    + 3600 * 24 * found_config -> period
               <=
               block_timestamp(current_time)
                    .to_time_point()
                    .time_since_epoch()
                    .to_seconds(),
               "platform can't request new amount of tokens because of period"
           );
        }

        if (platform_locks.cbegin() != platform_locks.cend()) {
           auto last_lock = platform_locks.end();
           snax_assert(
               (--last_lock)->time
                    .to_time_point()
                    .time_since_epoch()
                    .to_seconds()
                    + _gstate.platform_lock_duration
               <=
               block_timestamp(current_time)
                    .to_time_point()
                    .time_since_epoch()
                    .to_seconds(),
               "platform can't request new amount of tokens because of lock"
           );
        }

        const asset issued_supply =
            snax::token(N(snax.token))
                .get_supply(snax::symbol_type(system_token_symbol).name());

        snax::print("Issued supply: \t", issued_supply, "\n");

        const asset system_balance = get_balance(_self);

        int64_t circulating_supply = issued_supply.amount - system_balance.amount - platform_full_balance.amount;

        if (circulating_supply < 0) {
            circulating_supply = 0;
        }

        snax::print("Circulating supply: \t", circulating_supply, "\n");

        double _, current_offset;

        int64_t supply_difference = system_supply_soft_limit.amount - circulating_supply;

        if (supply_difference < 1'000'000'000'0000) {
            supply_difference = 1'000'000'000'0000;
        }

        std::tie(_, current_offset) = solve_quadratic_equation(
            static_cast<double>(_gstate.system_parabola_a),
            static_cast<double>(_gstate.system_parabola_b),
            static_cast<double>(circulating_supply / 1'0000)
        );

        snax::print("Supply difference: \t", supply_difference, "\n");

        snax::print("Offset: \t", current_offset, "\n");

        const int64_t round_supply = (
            supply_difference / 1'0000
            -
            static_cast<int64_t>(
                calculate_parabola(
                    static_cast<double>(_gstate.system_parabola_a),
                    static_cast<double>(_gstate.system_parabola_b),
                    convert_asset_to_double(system_supply_soft_limit / 1'0000),
                    static_cast<double>(current_offset + offset_of_round)
                )
            )
        ) * 1'0000;

        snax::print("Round supply: \t", round_supply, "\n");

        const asset amount_to_transfer =
            asset(
                static_cast<int64_t>(
                    static_cast<double>(round_supply)
                    * found_config->weight
                    * static_cast<double>(found_config->period)
                    / static_cast<double>(offset_of_round)
            )
        );

        snax::print("Amount to transfer: \t", amount_to_transfer, "\n");

        const asset amount_to_issue = amount_to_transfer > system_balance ? amount_to_transfer - system_balance: asset(0);

        if (amount_to_issue > asset(0)) {
            INLINE_ACTION_SENDER(snax::token, issue)(
                N(snax.token), {N(snax),N(active)},
                {
                    N(snax),
                    amount_to_issue,
                    "amount to issue to pay platform users"
                }
            );
        }

        if (amount_to_transfer > asset(0)) {
            INLINE_ACTION_SENDER(snax::token, transfer)(
                N(snax.token), {N(snax),N(active)},
                {
                    N(snax),
                    platform,
                    amount_to_transfer,
                    "platform round supply"
                }
            );
        }

        platform_requests.emplace(platform, [&](auto& record) {
            record.token_amount = amount_to_transfer;
            record.request = block_timestamp(current_time);
        });

        _global.set( _gstate, _self );

   }

   void system_contract::setram( uint64_t max_ram_size ) {
      require_auth( _self );

      snax_assert( _gstate.max_ram_size < max_ram_size, "ram may only be increased" ); /// decreasing ram might result market maker issues
      snax_assert( max_ram_size < 1024ll*1024*1024*1024*1024, "ram size is unrealistic" );
      snax_assert( max_ram_size > _gstate.total_ram_bytes_reserved, "attempt to set max below reserved" );

      auto delta = int64_t(max_ram_size) - int64_t(_gstate.max_ram_size);
      auto itr = _rammarket.find(S(4,RAMCORE));

      /**
       *  Increase or decrease the amount of ram for sale based upon the change in max
       *  ram size.
       */
      _rammarket.modify( itr, 0, [&]( auto& m ) {
         m.base.balance.amount += delta;
      });

      _gstate.max_ram_size = max_ram_size;
      _global.set( _gstate, _self );
   }

   void system_contract::setparams( const snax::blockchain_parameters& params ) {
      require_auth( N(snax) );
      (snax::blockchain_parameters&)(_gstate) = params;
      snax_assert( 3 <= _gstate.max_authority_depth, "max_authority_depth should be at least 3" );
      set_blockchain_parameters( params );
   }

   void system_contract::setpriv( account_name account, uint8_t ispriv ) {
      require_auth( _self );
      set_privileged( account, ispriv );
   }

   void system_contract::rmvproducer( account_name producer ) {
      require_auth( _self );
      auto prod = _producers.find( producer );
      snax_assert( prod != _producers.end(), "producer not found" );
      _producers.modify( prod, 0, [&](auto& p) {
            p.deactivate();
         });
   }

   void system_contract::setplatforms( const std::vector<snax::platform_config>& platforms ) {
       require_auth( _self );

       for ( auto snax_platform = _platforms.begin(); snax_platform != _platforms.end(); ) {
           bool found_platform = false;
           for (auto& platform: platforms) {
               if (platform.account == snax_platform->account) {
                   found_platform = true;
               }
           }
           // Set default resourse limits to excluded platforms
           if (!found_platform) {
               user_resources_table  userres( _self, snax_platform->account );
               auto res_itr = userres.find( snax_platform->account );
               if (res_itr != userres.end()) {
                  set_resource_limits( res_itr->owner, res_itr->ram_bytes, 0, 0 );
                  userres.modify( res_itr, _self, [&]( auto& res ) {
                    res.owner = snax_platform->account;
                    res.ram_bytes = res_itr->ram_bytes;
                    res.net_weight = asset(0);
                    res.cpu_weight = asset(0);
                  });
               }
               _platforms.erase(snax_platform++);
           } else {
               snax_platform++;
           }
        }

        double total_weight = 0;

        for (auto& platform: platforms) {

            snax_assert(platform.weight >= 0, "platform weight must be greater than 0 or equal to 0");

            total_weight += platform.weight;

            snax_assert(platform.period > 0, "platform period must be greater than 0");
            snax_assert(is_account(platform.account), "platform account doesnt exist");

            snax::print("Platform: \t", platform.account, "\n");

            snax::print("Quotas: \n");

            snax::print("RAM: \t", platform.quotas.ram_bytes, "\n");
            snax::print("NET: \t", platform.quotas.net_weight, "\n");
            snax::print("CPU: \t", platform.quotas.cpu_weight, "\n");

            // Set platform memory limits to quotas specified in configuration
            user_resources_table  userres( _self, platform.account );
            auto res_itr = userres.find( platform.account );
            if (res_itr != userres.end()) {
                set_resource_limits(
                    res_itr->owner,
                    platform.quotas.ram_bytes,
                    static_cast<int64_t>(platform.quotas.net_weight),
                    static_cast<int64_t>(platform.quotas.cpu_weight)
                );
                userres.modify( res_itr, _self, [&]( auto& res ) {
                    res.owner = platform.account;
                    res.ram_bytes = platform.quotas.ram_bytes;
                    res.net_weight = asset(platform.quotas.net_weight);
                    res.cpu_weight = asset(platform.quotas.cpu_weight);
                });
            } else {
                userres.emplace( _self, [&]( auto& res ) {
                    res.owner = platform.account;
                    res.ram_bytes = platform.quotas.ram_bytes;
                    res.net_weight = asset(platform.quotas.net_weight);
                    res.cpu_weight = asset(platform.quotas.cpu_weight);
                });
            }

            const auto found_platform = _platforms.find(platform.account);

            if (found_platform != _platforms.end()) {
                _platforms.modify( found_platform, 0, [&](auto& record) {
                    record.period = platform.period;
                    record.weight = platform.weight;
                    record.account = platform.account;
                    record.quotas.ram_bytes = platform.quotas.ram_bytes;
                    record.quotas.net_weight = platform.quotas.net_weight;
                    record.quotas.cpu_weight = platform.quotas.cpu_weight;
                });
            } else {
                _platforms.emplace( _self, [&](auto& record) {
                    record.period = platform.period;
                    record.weight = platform.weight;
                    record.account = platform.account;
                    record.quotas.ram_bytes = platform.quotas.ram_bytes;
                    record.quotas.net_weight = platform.quotas.net_weight;
                    record.quotas.cpu_weight = platform.quotas.cpu_weight;
                });
            }
        }

        snax_assert(total_weight <= 1  && total_weight >= 0, "Summary weight of all platforms must be equal from 1 to 0");
   }

   void system_contract::bidname( account_name bidder, account_name newname, asset bid ) {
      require_auth( bidder );
      snax_assert( snax::name_suffix(newname) == newname, "you can only bid on top-level suffix" );
      snax_assert( newname != 0, "the empty name is not a valid account name to bid on" );
      snax_assert( (newname & 0xFull) == 0, "13 character names are not valid account names to bid on" );
      snax_assert( (newname & 0x1F0ull) == 0, "accounts with 12 character names and no dots can be created without bidding required" );
      snax_assert( !is_account( newname ), "account already exists" );
      snax_assert( bid.symbol == asset().symbol, "asset must be system token" );
      snax_assert( bid.amount > 0, "insufficient bid" );

      INLINE_ACTION_SENDER(snax::token, transfer)( N(snax.token), {bidder,N(active)},
                                                    { bidder, N(snax.names), bid, std::string("bid name ")+(name{newname}).to_string()  } );

      name_bid_table bids(_self,_self);
      print( name{bidder}, " bid ", bid, " on ", name{newname}, "\n" );
      auto current = bids.find( newname );
      if( current == bids.end() ) {
         bids.emplace( bidder, [&]( auto& b ) {
            b.newname = newname;
            b.high_bidder = bidder;
            b.high_bid = bid.amount;
            b.last_bid_time = current_time();
         });
      } else {
         snax_assert( current->high_bid > 0, "this auction has already closed" );
         snax_assert( bid.amount - current->high_bid > (current->high_bid / 10), "must increase bid by 10%" );
         snax_assert( current->high_bidder != bidder, "account is already highest bidder" );

         INLINE_ACTION_SENDER(snax::token, transfer)( N(snax.token), {N(snax.names),N(active)},
                                                       { N(snax.names), current->high_bidder, asset(current->high_bid),
                                                       std::string("refund bid on name ")+(name{newname}).to_string()  } );

         bids.modify( current, bidder, [&]( auto& b ) {
            b.high_bidder = bidder;
            b.high_bid = bid.amount;
            b.last_bid_time = current_time();
         });
      }
   }

   /**
    *  Called after a new account is created. This code enforces resource-limits rules
    *  for new accounts as well as new account naming conventions.
    *
    *  Account names containing '.' symbols must have a suffix equal to the name of the creator.
    *  This allows users who buy a premium name (shorter than 12 characters with no dots) to be the only ones
    *  who can create accounts with the creator's name as a suffix.
    *
    */
   void native::newaccount( account_name     creator,
                            account_name     newact
                            /*  no need to parse authorities
                            const authority& owner,
                            const authority& active*/ ) {

      if( creator != _self ) {
         uint16_t length = 0;
         auto tmp = newact >> 4;
         bool has_dot = false;
         bool begin = false;
         int8_t previous_empty = 0;
         for( uint32_t i = 0; i < 12; ++i ) {
            const bool current = !!(tmp & 0x1f);
            if (current && previous_empty == 1) {
                has_dot = true;
                length += 2;
                previous_empty = -1;
            } else if (current) {
                if (!begin) {
                    begin = true;
                }
                length++;
            } else if (begin && previous_empty == 0) {
                previous_empty = 1;
            } else if (begin && (previous_empty == 1 || previous_empty == -1)) {
                break;
            }
            tmp >>= 5;
         }

         if( has_dot || length < 6 ) { // has dot or is less than 6 characters
            auto suffix = snax::name_suffix(newact);
            if( suffix == newact ) {
               name_bid_table bids(_self,_self);
               auto current = bids.find( newact );
               snax_assert( current != bids.end(), "no active bid for name" );
               snax_assert( current->high_bidder == creator, "only highest bidder can claim" );
               snax_assert( current->high_bid < 0, "auction for name is not closed yet" );
               bids.erase( current );
            } else {
               snax_assert( creator == suffix, "only suffix may create this account" );
            }
         }
      }

      user_resources_table  userres( _self, newact);

      userres.emplace( newact, [&]( auto& res ) {
        res.owner = newact;
      });

      set_resource_limits( newact, 0, 0, 0 );
   }

    std::tuple<double, double>
    system_contract::solve_quadratic_equation(
        const double a,
        const double b,
        const double c
    ) const {
        const double d = b * b - 4 * a * c;
        snax_assert(d >= 0, "cant calculate parabola");
        return std::make_tuple((-b + sqrt(d)) / 2 / a, (-b - sqrt(d)) / 2 / a);
    }

    double system_contract::calculate_parabola(
        const double a,
        const double b,
        const double c,
        const double x
    ) const {
        return a * x * x + b * x + c;
    }

    double system_contract::convert_asset_to_double(const asset value) const {
        return static_cast<double>(value.amount);
    }

    asset system_contract::get_balance(const account_name account) {
        _accounts_balances balances(N(snax.token), account);
        const auto& found = balances.find(snax::symbol_type(system_token_symbol).name());
        return found == balances.cend() ? asset(0): found->balance;
    }

    double system_contract::get_block_reward_multiplier(double x) const {
        const double target_point = 40'000'000'000;
        const double x0 = exp(0.15);
        const double x1 = (exp(1) - x0) / target_point;
        return log(x0 + x * x1);
    }

    asset system_contract::get_platform_full_balance() {
        asset balance = asset(0);

        for ( auto platform = _platforms.begin(); platform != _platforms.end(); platform++ ) {
            balance += get_balance(platform->account);
        }

        return balance;
    }


} /// snax.system


SNAX_ABI( snaxsystem::system_contract,
     // native.hpp (newaccount definition is actually in snax.system.cpp)
     (newaccount)(updateauth)(deleteauth)(linkauth)(unlinkauth)(canceldelay)(onerror)
     // snax.system.cpp
     (lockplatform)(emitplatform)(setram)(setplatforms)(setparams)(setpriv)(rmvproducer)(bidname)
     // delegate_bandwidth.cpp
     (buyrambytes)(buyram)(sellram)(escrowbw)(delegatebw)(undelegatebw)(refund)
     // voting.cpp
     (regproducer)(unregprod)(voteproducer)(regproxy)
     // producer_pay.cpp
     (onblock)(claimrewards)
)
