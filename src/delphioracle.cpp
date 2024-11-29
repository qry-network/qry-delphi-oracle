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
ACTION delphioracle::configure(const globalinput &g, const name default_pair) {
    require_auth(_self);

    // // set the datapoints counter
    if (!global_stats.exists()) {
        global_stats.set(GlobalStatsModel{.total_datapoints = 0}, _self);
    }

    global_table global(_self, _self.value);
    global.set(GlobalConfigModel{
                   .datapoints_per_instrument = g.datapoints_per_instrument,
                   .bars_per_instrument = g.bars_per_instrument,
                   .vote_interval = g.vote_interval,
                   .write_cooldown = g.write_cooldown,
                   .approver_threshold = g.approver_threshold,
                   .approving_oracles_threshold = g.approving_oracles_threshold,
                   .minimum_rank = g.minimum_rank,
                   .paid = g.paid
               }, _self);


    // check if default pair already exists
    pairstable pairs(_self, _self.value);
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

    // Create default pair data points
    data_points_table dstore(_self, default_pair.value);
    // First data point starts at uint64 max
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
    data_points_table dstore(_self, pair.name.value);

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
    data_points_table datapoints(_self, pair_name.value);
    while (datapoints.begin() != datapoints.end()) {
        auto itr = datapoints.end();
        --itr;
        datapoints.erase(itr);
    }
    pairs.erase(pair_itr);
}

// Clear pair_name data
ACTION delphioracle::clear(name pair_name) const {
    require_auth(_self);

    // global config singleton
    global_table global(get_self(), get_self().value);
    global.remove();

    stats_table gstore(_self, _self.value);
    while (gstore.begin() != gstore.end()) {
        auto itr = gstore.end();
        --itr;
        gstore.erase(itr);
    }

    stats_table lstore(_self, pair_name.value);
    while (lstore.begin() != lstore.end()) {
        auto itr = lstore.end();
        --itr;
        lstore.erase(itr);
    }

    data_points_table estore(_self, pair_name.value);
    while (estore.begin() != estore.end()) {
        auto itr = estore.end();
        --itr;
        estore.erase(itr);
    }

    pairstable pairs(_self, _self.value);
    while (pairs.begin() != pairs.end()) {
        auto itr = pairs.end();
        --itr;
        pairs.erase(itr);
    }
}
