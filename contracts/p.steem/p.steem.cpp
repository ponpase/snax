#include "p.steem.hpp"

using namespace std;

namespace snax {

/// @abi action initialize
void steem::initialize(const string name, const account_name token_dealer,
                       const string token_symbol_str, const uint8_t precision,
                       const account_name airdrop) {
  require_auth(_self);
  require_uninitialized();

  snax_assert(name.size() > 0, "platform name can't be empty");

  const symbol_type token_symbol =
      string_to_symbol(precision, token_symbol_str.c_str());

  _state.round_supply = asset(0, token_symbol);
  _state.step_number = 1;
  _state.token_dealer = token_dealer;
  _state.total_attention_rate = 0.0;
  _state.registered_attention_rate = 0.0;
  _state.round_sent_account_count = 0;
  _state.round_updated_account_count = 0;
  _state.total_user_count = 0;
  _state.registered_user_count = 0;
  _state.updating = 0;
  _state.account = _self;
  _state.airdrop = airdrop;
  _state.platform_name = name;
  _state.sent_amount = asset(0, token_symbol);
  _state.token_symbols = {token_symbol};

  _platform_state.set(_state, _self);
}

/// @abi action addarticle
void steem::addarticle(const uint64_t author, const string permlink,
                       const string title, const block_timestamp created) {
  require_auth(_self);
  require_initialized();
  snax_assert(_users.find(author) != _users.end(), "author not found");
  checksum256 hash;
  sha256(permlink.c_str(), permlink.size() * sizeof(char), &hash);
  const auto permlink_key = checksum256_to_sha256(hash);

  const auto permlink_index = _bounty_articles.get_index<N(permlink)>();

  const auto author_index = _bounty_articles.get_index<N(author)>();

  snax_assert(permlink_index.find(permlink_key) == permlink_index.end(),
              "article already added");

  snax_assert(author_index.find(author) == author_index.end(),
              "author already has an article");

  auto end = _bounty_articles.end();

  _bounty_articles.emplace(_self, [&](auto &record) {
    record.seq = _bounty_articles.begin() != _bounty_articles.end()
                     ? (--end)->seq + 1
                     : 0;
    record.author = author;
    record.permlink = permlink;
    record.title = title;
    record.created = created;
    record.paid = asset(0, _platform_state.get().round_supply.symbol);
  });

  if (_bounty_state.exists()) {
    _bounty = _bounty_state.get();
  } else {
    _bounty.total_paid = asset(0, _platform_state.get().round_supply.symbol);
  }
  _bounty.last_update = block_timestamp(snax::time_point_sec(now()));
  _bounty_state.set(_bounty, _self);
}

/// @abi action rmarticle
void steem::rmarticle(const string permlink) {
  require_auth(_self);
  require_initialized();
  checksum256 hash;
  sha256(permlink.c_str(), permlink.size() * sizeof(char), &hash);
  const auto permlink_key = checksum256_to_sha256(hash);

  const auto permlink_index = _bounty_articles.get_index<N(permlink)>();
  const auto &article = permlink_index.find(permlink_key);

  snax_assert(article != permlink_index.end(), "article not found");
  _bounty_articles.erase(_bounty_articles.find(article->seq));
}

/// @abi action paybounty
void steem::paybounty(const account_name payer, const string permlink,
                      const asset amount) {
  require_auth(payer);
  require_initialized();
  checksum256 hash;
  sha256(permlink.c_str(), permlink.size() * sizeof(char), &hash);
  const auto permlink_key = checksum256_to_sha256(hash);

  const auto permlink_index = _bounty_articles.get_index<N(permlink)>();

  const auto &article = permlink_index.find(permlink_key);

  snax_assert(article != permlink_index.end(), "article not found");
  action(permission_level{payer, N(active)}, _self, N(transfersoc),
         make_tuple(payer, article->author, amount, string("bounty program")))
      .send();

  _bounty_articles.modify(_bounty_articles.find(article->seq), _self,
                          [&](auto &record) { record.paid += amount; });
  _bounty = _bounty_state.get();
  _bounty.total_paid += amount;
  _bounty_state.set(_bounty, _self);
}

/// @abi action lockarupdate
void steem::lockarupdate() {
  require_auth(_self);
  require_initialized();
  _state = _platform_state.get();

  snax_assert(!_state.updating, "platform is already updating");

  _state.updating = 1;
  _state.total_attention_rate = 0.0;
  _state.registered_attention_rate = 0.0;

  _platform_state.set(_state, _self);
}

/// @abi action lockupdate
void steem::lockupdate() {
  require_auth(_self);
  require_initialized();
  _state = _platform_state.get();

  snax_assert(_state.updating == 1, "platform must be in ar updating state");

  action(permission_level{_self, N(active)}, _state.token_dealer,
         N(lockplatform), make_tuple(_self))
      .send();

  _state.updating = 2;
  _platform_state.set(_state, _self);
}

/// @abi action addcreator
void steem::addcreator(const account_name name) {
  require_auth(_self);
  require_initialized();
  snax_assert(is_account(name), "account isnt registered");

  const auto found = _creators.find(name);

  snax_assert(found == _creators.end(), "creator already registered");

  _creators.emplace(_self, [&](auto &record) { record.account = name; });
}

/// @abi action rmcreator
void steem::rmcreator(const account_name name) {
  require_auth(_self);
  require_initialized();
  snax_assert(is_account(name), "account isnt registered");

  const auto found = _creators.find(name);

  snax_assert(found != _creators.end(), "creator isnt registered");

  _creators.erase(found);
}

/// @abi action nextround
void steem::nextround() {
  require_auth(_self);
  require_initialized();

  _state = _platform_state.get();

  snax_assert(
      _state.updating == 2,
      "platform must be in updating state when nextround action is called");

  action(permission_level{_self, N(active)}, _state.token_dealer,
         N(emitplatform), make_tuple(_self))
      .send();

  _state.round_sent_account_count = 0;
  _state.updating = 3;

  _platform_state.set(_state, _self);
}

/// @abi action sendpayments
void steem::sendpayments(const account_name lower_account_name,
                         uint64_t account_count) {
  require_auth(_self);
  require_initialized();
  _state = _platform_state.get();

  snax_assert(_state.updating == 3, "platform must be in updating state and "
                                    "nextround must be called before sending "
                                    "payments");

  const auto &_accounts_account_index = _accounts.get_index<N(name)>();
  auto iter = _accounts_account_index.lower_bound(
      lower_account_name ? lower_account_name : 1);

  const auto &end_iter = _accounts_account_index.cend();
  uint32_t updated_account_count = 0;

  asset current_balance = get_balance(_self, _state.round_supply.symbol);
  asset sent_amount = _state.round_supply - _state.round_supply;
  asset total_balance = _state.round_supply;

  if (_state.round_supply.amount == 0 && current_balance.amount > 0) {
    _state.round_supply = current_balance;
    total_balance = current_balance;
    _platform_state.set(_state, _self);
  }

  while (iter != end_iter && account_count--) {
    const auto &account = *iter;
    const auto &user = *_users.find(account.id);
    if (account.name && account.active) {
      snax_assert(account.last_paid_step_number < _state.step_number + 1,
                  "account already updated");
      updated_account_count++;
      if (user.attention_rate > 0.1 &&
          user.last_attention_rate_updated_step_number == _state.step_number) {
        asset token_amount;
        const int64_t portion = static_cast<int64_t>(
            _state.total_attention_rate / user.attention_rate);
        if (portion < total_balance.amount) {
          token_amount = total_balance / portion;
          if (token_amount.amount > current_balance.amount) {
            token_amount = current_balance;
          }
        } else {
          token_amount = asset(0);
        }
        if (token_amount.amount > 0) {
          current_balance -= token_amount;
          action(permission_level{_self, N(active)}, N(snax.token), N(transfer),
                 make_tuple(_self, account.name, token_amount,
                            string("payment for activity")))
              .send();
          sent_amount += token_amount;
        }
      }
      _accounts.modify(_accounts.find(account.id), _self, [&](auto &account) {
        account.last_paid_step_number = _state.step_number;
      });
    }
    iter++;
  }

  if (_state.round_sent_account_count + updated_account_count >=
      _state.registered_user_count) {
    unlock_update(current_balance, sent_amount, updated_account_count);
  } else {
    _state.round_sent_account_count += updated_account_count;
    _state.sent_amount += sent_amount;
    _platform_state.set(_state, _self);
  }
}

/// @abi action activate
void steem::activate(const uint64_t id) {
  require_auth(_self);
  require_initialized();
  set_account_active(id, true);
}

/// @abi action deactivate
void steem::deactivate(const uint64_t id) {
  require_auth(_self);
  require_initialized();
  set_account_active(id, false);
}

/// @abi action addsymbol
void steem::addsymbol(const string token_symbol_str, const uint8_t precision) {
  require_auth(_self);
  require_initialized();

  const symbol_type symbol =
      string_to_symbol(precision, token_symbol_str.c_str());

  _state.token_symbols.push_back(symbol);
  _platform_state.set(_state, _self);
}

/// @abi action updatear
void steem::updatear(const uint64_t id, const string account_name,
                     const double attention_rate,
                     const uint32_t attention_rate_rating_position,
                     const vector<uint32_t> stat_diff,
                     const uint8_t posts_ranked_in_period,
                     const bool add_account_if_not_exist) {
  require_auth(_self);
  require_initialized();
  _state = _platform_state.get();

  snax_assert(
      _state.updating == 1,
      "platform must be in updating state 1 when updatear action is called");

  const auto &found = _users.find(id);

  snax_assert(found != _users.end() || add_account_if_not_exist,
              "user doesnt exist");

  double registered_attention_rate = 0;
  double total_attention_rate = 0;
  uint32_t round_updated_account_count = 0;
  uint32_t total_user_count = 0;

  if (found != _users.end()) {
    snax_assert(attention_rate >= 0, "incorrect attention rate");

    const auto already_updated =
        found->last_attention_rate_updated_step_number == _state.step_number;

    const double attention_rate_inc =
        already_updated ? attention_rate - found->attention_rate
                        : attention_rate;

    total_attention_rate += attention_rate_inc;
    round_updated_account_count += !already_updated;

    _users.modify(found, _self, [&](auto &record) {
      record.attention_rate = attention_rate;
      record.attention_rate_rating_position = attention_rate_rating_position;
      record.last_attention_rate_updated_step_number = _state.step_number;
      record.posts_ranked_in_last_period = posts_ranked_in_period;
    });

    const auto &found_account = _accounts.find(id);

    if (found_account != _accounts.end()) {
      _accounts.modify(found_account, _self,
                       [&](auto &record) { record.stat_diff = stat_diff; });
      registered_attention_rate += attention_rate_inc;
    }
  } else {
    total_user_count++;
    _users.emplace(_self, [&](auto &record) {
      record.attention_rate = attention_rate;
      record.attention_rate_rating_position = attention_rate_rating_position;
      record.last_attention_rate_updated_step_number = _state.step_number;
      record.posts_ranked_in_last_period = posts_ranked_in_period;
      record.id = id;
      record.account_name = account_name;
    });
    addaccount(_self, 0, id, account_name, 0, string(""), stat_diff);

    total_attention_rate += attention_rate;
    round_updated_account_count++;
  }

  _state = _platform_state.get();
  _state.registered_attention_rate += registered_attention_rate;
  _state.total_attention_rate += total_attention_rate;
  _state.round_updated_account_count += round_updated_account_count;
  _state.total_user_count += total_user_count;
  _platform_state.set(_state, _self);
}

/// @abi action updatearmult
void steem::updatearmult(vector<account_with_attention_rate> &updates,
                         const bool add_account_if_not_exist) {
  require_auth(_self);
  require_initialized();
  _state = _platform_state.get();

  snax_assert(_state.updating == 1, "platform must be in updating state 1 when "
                                    "updatearmult action is called");

  double total_attention_rate = 0;
  uint32_t updated_account_count = 0;
  double registered_attention_rate = 0;
  uint32_t total_user_count = 0;

  for (auto &update : updates) {
    const auto &user = _users.find(update.id);
    snax_assert(user != _users.end() || add_account_if_not_exist,
                "user doesnt exist");
    if (user != _users.end()) {
      const double attention_rate = update.attention_rate;
      const uint32_t attention_rate_rating_position =
          update.attention_rate_rating_position;

      snax_assert(attention_rate >= 0, "incorrect attention rate");

      const auto already_updated =
          user->last_attention_rate_updated_step_number == _state.step_number;

      const auto attention_rate_inc =
          already_updated ? attention_rate - user->attention_rate
                          : attention_rate;

      _users.modify(user, _self, [&](auto &record) {
        record.attention_rate = attention_rate;
        record.attention_rate_rating_position = attention_rate_rating_position;
        record.last_attention_rate_updated_step_number = _state.step_number;
        record.posts_ranked_in_last_period = update.posts_ranked_in_period;
      });

      const auto &found_account = _accounts.find(update.id);

      if (found_account != _accounts.end()) {
        _accounts.modify(found_account, _self, [&](auto &record) {
          record.stat_diff = update.stat_diff;
        });
        registered_attention_rate += attention_rate_inc;
      }

      total_attention_rate += attention_rate_inc;
      updated_account_count += !already_updated;

    } else {
      total_user_count++;

      _users.emplace(_self, [&](auto &record) {
        record.attention_rate = update.attention_rate;
        record.attention_rate_rating_position =
            update.attention_rate_rating_position;
        record.last_attention_rate_updated_step_number = _state.step_number;
        record.posts_ranked_in_last_period = update.posts_ranked_in_period;
        record.id = update.id;
        record.account_name = update.account_name;
      });

      addaccount(_self, 0, update.id, update.account_name, 0, string(""),
                 update.stat_diff);

      total_attention_rate += update.attention_rate;
      updated_account_count++;
    }
  }

  _state = _platform_state.get();
  _state.registered_attention_rate += registered_attention_rate;
  _state.total_attention_rate += total_attention_rate;
  _state.round_updated_account_count += updated_account_count;
  _state.total_user_count += total_user_count;
  _platform_state.set(_state, _self);
}

/// @abi action dropuser
void steem::dropuser(const uint64_t id) {
  require_auth(_self);
  require_initialized();

  const auto user = _users.find(id);

  snax_assert(user != _users.end(), "user not found");

  const auto account = _accounts.find(id);

  if (account != _accounts.end()) {
    dropaccount(_self, id);
  }

  _state = _platform_state.get();
  _state.total_attention_rate -= user->attention_rate;
  _state.total_user_count--;

  _users.erase(user);
  _platform_state.set(_state, _self);
}

/// @abi action bindaccount
void steem::bindaccount(const account_name account, const string salt) {
  require_auth(account);
  require_initialized();
  _account_bindings account_bindings(_self, account);
  snax_assert(account_bindings.begin() == account_bindings.end(),
              "account is already bound");
  account_bindings.emplace(account, [&](auto &record) {
    record.account = account;
    record.salt = salt;
  });
}

/// @abi action dropaccount
void steem::dropaccount(const account_name initiator, const uint64_t id) {
  require_auth(initiator);
  require_initialized();
  _state = _platform_state.get();

  const auto account = _accounts.find(id);

  snax_assert(account != _accounts.end(), "no such account");

  snax_assert(_self == initiator || account->name == initiator,
              "cant drop account");

  const auto user = _users.find(id);

  _state = _platform_state.get();
  _state.registered_user_count--;
  if (user != _users.end()) {
    _state.registered_attention_rate -= user->attention_rate;
  }
  _accounts.erase(account);
  _platform_state.set(_state, _self);
}

/// @abi action addaccount
void steem::addaccount(const account_name creator, const account_name account,
                       const uint64_t id, const string account_name,
                       const uint64_t verification_post,
                       const string verification_salt,
                       const vector<uint32_t> stat_diff) {
  require_creator_or_platform(creator);
  require_initialized();
  _state = _platform_state.get();

  const auto &found_user = _users.find(id);
  const auto &found_account = _accounts.find(id);

  double registered_attention_rate = 0;

  snax_assert(found_user == _users.end() || found_account == _accounts.end(),
              "user already exists");

  snax_assert(account_name.size() > 0, "account_name cannot be empty");

  claim_transfered(id, account);

  const auto is_user_new = found_user == _users.end();

  if (is_user_new) {
    _users.emplace(_self, [&](auto &record) {
      record.account_name = account_name;
      record.id = id;
      record.last_attention_rate_updated_step_number = 0;
    });
  }

  if (account) {
    snax_assert(found_account == _accounts.end(), "account already exists");
    snax_assert(verification_post > 0,
                "verification post status id can't be empty");
    snax_assert(verification_salt.size() > 0,
                "verification salt can't be empty");

    _accounts.emplace(_self, [&](auto &record) {
      record.active = true;
      record.name = account;
      record.id = id;
      record.last_paid_step_number = 0;
      record.created = block_timestamp(snax::time_point_sec(now()));
      record.verification_post = verification_post;
      record.verification_salt = verification_salt;
      record.stat_diff = stat_diff;
    });

    if (found_user != _users.end()) {
      registered_attention_rate += found_user->attention_rate;
    }
  }

  _state = _platform_state.get();
  _state.total_user_count += is_user_new;
  _state.registered_attention_rate += registered_attention_rate;
  _state.registered_user_count += account != 0;
  _platform_state.set(_state, _self);
};

/// @abi action addaccounts
void steem::addaccounts(const account_name creator,
                        vector<account_to_add> &accounts_to_add) {
  require_creator_or_platform(creator);
  require_initialized();

  for (auto &account_to_add : accounts_to_add) {
    addaccount(creator, account_to_add.name, account_to_add.id,
               account_to_add.account_name, account_to_add.verification_post,
               account_to_add.verification_salt, account_to_add.stat_diff);
  }
}

/// @abi action transfersoca
void steem::transfersoca(const account_name from, const string to,
                         const asset quantity, const string memo) {
  checksum256 hash;
  sha256(to.c_str(), to.size() * sizeof(char), &hash);
  const auto to_key = checksum256_to_sha256(hash);

  const auto users_account_name_index = _users.get_index<N(account_name)>();

  const auto &to_user = users_account_name_index.find(to_key);

  snax_assert(to_user != users_account_name_index.end(),
              "to account not found");

  transfersoc(from, to_user->id, quantity, memo);
}

/// @abi action transfersoc
void steem::transfersoc(const account_name from, const uint64_t to,
                        const asset quantity, const string memo) {
  require_auth(from);
  require_initialized();

  const asset balance = get_balance(from, quantity.symbol);

  snax_assert(balance >= quantity, "from account doesnt have enough tokens");

  const auto &to_account = _accounts.find(to);

  if (to_account != _accounts.end() && to_account->name) {
    action(permission_level{from, N(active)}, N(snax.token), N(transfer),
           make_tuple(from, to_account->name, quantity, memo))
        .send();
  } else {
    transfers_table _transfers(_self, quantity.symbol.name());

    action(permission_level{from, N(active)}, N(snax.token), N(transfer),
           make_tuple(from, N(snax.transf), quantity, memo))
        .send();
    const auto &found_transfer = _transfers.find(to);

    if (found_transfer != _transfers.end()) {
      _transfers.modify(found_transfer, from,
                        [&](auto &transfer) { transfer.amount += quantity; });
    } else {
      _transfers.emplace(from, [&](auto &transfer) {
        transfer.amount = quantity;
        transfer.id = to;
      });
    }
  }
}

asset steem::get_balance(const account_name account, const symbol_type symbol) {
  require_initialized();

  _users_balances balances(N(snax.token), account);
  const auto &found_balance = balances.find(symbol.name());
  const auto result = found_balance != balances.end() ? found_balance->balance
                                                      : asset(0, symbol);
  return result;
}

void steem::claim_transfered(const uint64_t id, const account_name account) {
  require_initialized();

  if (!account)
    return;

  for (symbol_type symbol : _state.token_symbols) {
    transfers_table _transfers(_self, symbol.name());

    auto found = _transfers.find(id);

    if (found != _transfers.end()) {
      const asset amount = found->amount;

      action(permission_level{N(snax.transf), N(active)}, N(snax.token),
             N(transfer),
             make_tuple(N(snax.transf), account, amount, string("social")))
          .send();

      _transfers.erase(found);
    }
  }
}

// Only contract itself is allowed to unlock update
void steem::unlock_update(const asset current_amount,
                          const asset last_sent_amount,
                          const uint32_t last_updated_account_count) {
  require_initialized();
  _state = _platform_state.get();

  _state.sent_amount += last_sent_amount;

  _states_history.emplace(_self, [&](auto &record) {
    record.step_number = _state.step_number;
    record.registered_user_count = _state.registered_user_count;
    record.total_user_count = _state.total_user_count;
    record.total_attention_rate = _state.total_attention_rate;
    record.registered_attention_rate = _state.registered_attention_rate;
    record.round_supply = _state.round_supply;
    record.sent_amount = _state.sent_amount;
    record.round_sent_account_count =
        _state.round_sent_account_count + last_updated_account_count;
    record.round_updated_account_count = _state.round_updated_account_count;
  });

  _state.updating = 0;
  _state.round_sent_account_count = 0;
  _state.round_updated_account_count = 0;
  _state.round_supply -= _state.round_supply;
  _state.step_number++;

  _platform_state.set(_state, _self);

  if (current_amount.amount > 0) {
    action(permission_level{_self, N(active)}, N(snax.token), N(transfer),
           make_tuple(_self, N(snax), current_amount,
                      string("rest of round supply")))
        .send();
  }
}

void steem::require_initialized() {
  snax_assert(_platform_state.exists(), "platform must be initialized");
}

void steem::require_uninitialized() {
  snax_assert(!_platform_state.exists(), "platform is already initialized");
}

void steem::require_creator_or_platform(const account_name account) {
  require_auth(account);
  snax_assert(account == _self || _creators.find(account) != _creators.end(),
              "platform or creator authority needed");
}

void steem::set_account_active(const uint64_t id, const bool active) {
  const auto found_account = _accounts.find(id);

  snax_assert(found_account != _accounts.end(), "account doesnt exist");

  _accounts.modify(found_account, 0,
                   [&](auto &record) { record.active = active; });
}
}

SNAX_ABI(
    snax::steem,
    (initialize)(addarticle)(rmarticle)(paybounty)(lockarupdate)(lockupdate)(
        addcreator)(rmcreator)(nextround)(activate)(deactivate)(addsymbol)(
        sendpayments)(addaccount)(dropuser)(bindaccount)(dropaccount)(
        addaccounts)(updatear)(transfersoc)(transfersoca)(updatearmult))
