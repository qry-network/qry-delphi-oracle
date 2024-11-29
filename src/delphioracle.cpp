/*

  delphioracle

  Authors: Guillaume "Gnome" Babin-Tremblay - EOS Titan, Andrew "netuoso" Chaney - EOS Titan
  
  Website: https://eostitan.com
  Email: guillaume@eostitan.com

  Github: https://github.com/eostitan/delphioracle/
  
  Published under MIT License

*/
#include <delphioracle.hpp>

// Write datapoint
ACTION delphioracle::write(const name owner, const std::vector<quote> &quotes) {
    require_auth(owner);

    size_t length = quotes.size();

    print("length ", length);

    check(length > 0, "must supply non-empty array of quotes");
    check(check_oracle(owner), "account is not a qualified oracle");

    const pairstable pairs(_self, _self.value);

    for (int i = 0; i < length; i++) {
        print("quote ", i, " ", quotes[i].value, " ", quotes[i].pair, "\n");
        auto itr = pairs.find(quotes[i].pair.value);
        check(itr != pairs.end() && itr->active == true, "pair not allowed");
        check_last_push(owner, quotes[i].pair);
        update_datapoints(owner, quotes[i].value, itr);
    }
}

// Configuration
ACTION delphioracle::configure(globalinput g) {
    require_auth(_self);

    globaltable global_table(_self, _self.value);
    pairstable pairs(_self, _self.value);

    const auto global_itr = global_table.begin();

    constexpr auto default_pair = name("qryusd");

    if (global_itr == global_table.end()) {
        global_table.emplace(_self, [&](auto &o) {
            o.id = 1;
            o.total_datapoints_count = 0;
            o.total_claimed = asset(0, symbol(SYSTEM_SYMBOL, SYSTEM_PRECISION));
            o.datapoints_per_instrument = g.datapoints_per_instrument;
            o.bars_per_instrument = g.bars_per_instrument;
            o.vote_interval = g.vote_interval;
            o.write_cooldown = g.write_cooldown;
            o.approver_threshold = g.approver_threshold;
            o.approving_oracles_threshold = g.approving_oracles_threshold;
            o.approving_custodians_threshold = g.approving_custodians_threshold;
            o.minimum_rank = g.minimum_rank;
            o.paid = g.paid;
            o.min_bounty_delay = g.min_bounty_delay;
            o.new_bounty_delay = g.new_bounty_delay;
        });
    } else {
        global_table.modify(*global_itr, _self, [&](auto &o) {
            o.datapoints_per_instrument = g.datapoints_per_instrument;
            o.bars_per_instrument = g.bars_per_instrument;
            o.vote_interval = g.vote_interval;
            o.write_cooldown = g.write_cooldown;
            o.approver_threshold = g.approver_threshold;
            o.approving_oracles_threshold = g.approving_oracles_threshold;
            o.approving_custodians_threshold = g.approving_custodians_threshold;
            o.minimum_rank = g.minimum_rank;
            o.paid = g.paid;
            o.min_bounty_delay = g.min_bounty_delay;
            o.new_bounty_delay = g.new_bounty_delay;
        });
    }

    // check if default pair already exists using
    if (pairs.find(default_pair.value) == pairs.end()) {
        // Add default pair
        pairs.emplace(_self, [&](auto &o) {
            o.active = true;
            o.name = "qryusd"_n;
            o.base_symbol = symbol("QRY", 4);
            o.base_type = e_asset_type::eosio_token;
            o.base_contract = "eosio.token"_n;
            o.quote_symbol = symbol("USD", 2);
            o.quote_type = e_asset_type::fiat;
            o.quote_contract = ""_n;
            o.quoted_precision = 4;
        });
    }

    datapointstable dstore(_self, name("eosusd"_n).value);

    //First data point starts at uint64 max
    uint64_t primary_key = 0;

    for (uint16_t i = 0; i < 21; i++) {
        dstore.emplace(_self, [&](auto &s) {
            s.id = primary_key;
            s.value = 0;
            s.timestamp = NULL_TIME_POINT;
        });
        primary_key++;
    }
}

// create a new pair
ACTION delphioracle::newpair(pairinput pair) {
    require_auth(_self);

    pairstable pairs(_self, _self.value);
    datapointstable dstore(_self, pair.name.value);

    const auto itr = pairs.find(pair.name.value);

    check(pair.name != "system"_n, "Cannot create a pair named system");
    check(itr == pairs.end(), "A pair with this name already exists.");

    pairs.emplace(_self, [&](auto &s) {
        s.name = pair.name;
        s.base_symbol = pair.base_symbol;
        s.base_type = pair.base_type;
        s.base_contract = pair.base_contract;
        s.quote_symbol = pair.quote_symbol;
        s.quote_type = pair.quote_type;
        s.quote_contract = pair.quote_contract;
        s.quoted_precision = pair.quoted_precision;
    });

    // First data point starts at uint64 max
    uint64_t primary_key = 0;

    for (uint16_t i = 0; i < 21; i++) {
        // Insert next datapoint
        dstore.emplace(_self, [&](auto &s) {
            s.id = primary_key;
            s.value = 0;
            s.timestamp = NULL_TIME_POINT;
        });
        primary_key++;
    }
}

// delete pair
ACTION delphioracle::deletepair(const name pair_name, const std::string &reason) const {
    require_auth(_self);
    pairstable pairs(_self, _self.value);
    const auto pair_itr = pairs.find(pair_name.value);
    check(pair_itr != pairs.end(), "Unable to find pair");
    check(!reason.empty(), "Must supply a reason when deleting a pair");
    datapointstable datapoints(_self, pair_name.value);
    while (datapoints.begin() != datapoints.end()) {
        auto itr = datapoints.end();
        --itr;
        datapoints.erase(itr);
    }
    pairs.erase(pair_itr);
}

// Clear all data
ACTION delphioracle::clear(name pair_name) const {
    require_auth(_self);

    globaltable global_table(_self, _self.value);
    statstable gstore(_self, _self.value);
    statstable lstore(_self, pair_name.value);
    datapointstable estore(_self, pair_name.value);
    pairstable pairs(_self, _self.value);
    custodianstable ctable(_self, _self.value);
    hashestable htable(_self, _self.value);

    while (ctable.begin() != ctable.end()) {
        auto itr = ctable.end();
        --itr;
        ctable.erase(itr);
    }

    while (global_table.begin() != global_table.end()) {
        auto itr = global_table.end();
        --itr;
        global_table.erase(itr);
    }

    while (gstore.begin() != gstore.end()) {
        auto itr = gstore.end();
        --itr;
        gstore.erase(itr);
    }

    while (lstore.begin() != lstore.end()) {
        auto itr = lstore.end();
        --itr;
        lstore.erase(itr);
    }

    while (estore.begin() != estore.end()) {
        auto itr = estore.end();
        --itr;
        estore.erase(itr);
    }

    while (pairs.begin() != pairs.end()) {
        auto itr = pairs.end();
        --itr;
        pairs.erase(itr);
    }

    while (htable.begin() != htable.end()) {
        auto itr = htable.end();
        --itr;
        htable.erase(itr);
    }
}

// ACTION delphioracle::migratedata() {
//     require_auth(_self);
//     statstable stats(name("delphibackup"), name("delphibackup").value);
//     statstable _stats(_self, _self.value);
//     check(_stats.begin() == _stats.end(), "stats info already exists; call clear first");
//     oglobaltable global(name("delphibackup"), name("delphibackup").value);
//     globaltable _global(_self, _self.value);
//     auto glitr = global.begin();
//     _global.emplace(_self, [&](auto &g) {
//         g.id = glitr->id;
//         g.total_datapoints_count = glitr->total_datapoints_count;
//         g.datapoints_per_instrument = 21;
//         g.bars_per_instrument = 30;
//         g.vote_interval = 10000;
//         g.write_cooldown = 55000000;
//         g.approver_threshold = 1;
//         g.approving_oracles_threshold = 2;
//         g.approving_custodians_threshold = 1;
//         g.minimum_rank = 105;
//         g.paid = 21;
//         g.min_bounty_delay = 604800;
//         g.new_bounty_delay = 259200;
//     });
//     auto gitr = stats.begin();
//     while (gitr != stats.end()) {
//         _stats.emplace(_self, [&](auto &s) {
//             s.owner = gitr->owner;
//             s.timestamp = gitr->timestamp;
//             s.count = gitr->count;
//             s.last_claim = gitr->last_claim;
//             s.balance = gitr->balance;
//         });
//         gitr++;
//     }
//     npairstable pairs(name("delphibackup"), name("delphibackup").value);
//     pairstable _pairs(_self, _self.value);
//     auto pitr = pairs.begin();
//     while (pitr != pairs.end()) {
//         _pairs.emplace(_self, [&](auto &p) {
//             p.active = pitr->active;
//             p.name = pitr->name;
//             p.base_symbol = pitr->base_symbol;
//             p.base_type = pitr->base_type;
//             p.base_contract = pitr->base_contract;
//             p.quote_symbol = pitr->quote_symbol;
//             p.quote_type = pitr->quote_type;
//             p.quote_contract = pitr->quote_contract;
//             p.quoted_precision = pitr->quoted_precision;
//         });
//         statstable pstats(name("delphibackup"), pitr->name.value);
//         statstable _pstats(_self, pitr->name.value);
//         auto sitr = pstats.begin();
//         while (sitr != pstats.end()) {
//             _pstats.emplace(_self, [&](auto &s) {
//                 s.owner = sitr->owner;
//                 s.timestamp = sitr->timestamp;
//                 s.count = sitr->count;
//                 s.last_claim = sitr->last_claim;
//                 s.balance = sitr->balance;
//             });
//             sitr++;
//         }
//         datapointstable datapoints(name("delphibackup"), pitr->name.value);
//         datapointstable _datapoints(_self, pitr->name.value);
//         auto ditr = datapoints.begin();
//         while (ditr != datapoints.end()) {
//             _datapoints.emplace(_self, [&](auto &d) {
//                 d.id = ditr->id;
//                 d.owner = ditr->owner;
//                 d.value = ditr->value;
//                 d.median = ditr->median;
//                 d.timestamp = ditr->timestamp;
//             });
//             ditr++;
//         }
//         pitr++;
//     }
// }
