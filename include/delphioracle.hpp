/*

  delphioracle

  Authors: 
    Guillaume "Gnome" Babin-Tremblay - EOS Titan 
    Andrew "netuoso" Chaney - EOS Titan

  Website: https://eostitan.com
  Email: guillaume@eostitan.com

  Github: https://github.com/eostitan/delphioracle/

  Published under MIT License

*/
#ifndef DELPHIORACLE_HPP
#define DELPHIORACLE_HPP

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/crypto.hpp>
#include <eosio/system.hpp>
#include <eosio/singleton.hpp>
#include <cmath>

using namespace eosio;
using namespace std;

const auto NULL_TIME_POINT = time_point(microseconds(0));

CONTRACT delphioracle : public contract {
public:
    using contract::contract;

    delphioracle(const name receiver, const name code, const datastream<const char *> &ds) : contract(receiver, code,
            ds),
        global_stats(receiver, receiver.value) {
    }

    // Types
    enum e_asset_type: uint16_t {
        fiat = 1,
        cryptocurrency = 2,
        erc20_token = 3,
        eosio_token = 4,
        equity = 5,
        derivative = 6,
        other = 7
    };

    typedef uint16_t asset_type;

    struct globalinput {
        uint64_t datapoints_per_instrument;
        uint64_t bars_per_instrument;
        uint64_t vote_interval;
        uint64_t write_cooldown;
        uint64_t approver_threshold;
        uint64_t approving_oracles_threshold;
        uint64_t minimum_rank;
        uint64_t paid;
    };

    struct pairinput {
        name name;
        symbol base_symbol;
        asset_type base_type;
        eosio::name base_contract;
        symbol quote_symbol;
        asset_type quote_type;
        eosio::name quote_contract;
        uint64_t quoted_precision;
    };


    //Quote
    struct quote {
        uint64_t value;
        name pair;
    };

    // eosmechanics::cpu
    struct event {
        uint64_t value;
        name instrument;
    };

    TABLE producer_info {
        name owner;
        double total_votes = 0;
        public_key producer_key;
        bool is_active = true;
        std::string url;
        uint32_t unpaid_blocks = 0;
        time_point last_claim_time;
        uint16_t location = 0;
        [[nodiscard]] uint64_t primary_key() const { return owner.value; }
        [[nodiscard]] double by_votes() const { return is_active ? -total_votes : total_votes; }
        [[nodiscard]] bool active() const { return is_active; }
    };

    struct st_transfer {
        name from;
        name to;
        asset quantity;
        std::string memo;
    };

    // Global config
    TABLE GlobalConfigModel {
        uint64_t datapoints_per_instrument = 21;
        uint64_t bars_per_instrument = 30;
        uint64_t vote_interval = 10000;
        uint64_t write_cooldown = 1000000 * 55;
        uint64_t approver_threshold = 1;
        uint64_t approving_oracles_threshold = 1;
        uint64_t minimum_rank = 105;
        uint64_t paid = 21;
    };

    // Holds the global configuration
    using global_table = singleton<"global"_n, GlobalConfigModel>;

    TABLE GlobalStatsModel {
        uint64_t total_datapoints = 0;
    };

    // Holds the total number of datapoints
    using global_stats_type = singleton<"globalstats"_n, GlobalStatsModel>;
    global_stats_type global_stats;

    // Holds the last datapoints_count datapoints from qualified oracles
    TABLE datapoints {
        uint64_t id{};
        name owner;
        uint64_t value{};
        uint64_t median{};
        time_point timestamp;

        [[nodiscard]] uint64_t primary_key() const { return id; }
        [[nodiscard]] uint64_t by_timestamp() const { return timestamp.elapsed.to_seconds(); }
        [[nodiscard]] uint64_t by_value() const { return value; }
    };

    // Holds the count and time of last writes for qualified oracles
    TABLE stats {
        name owner;
        time_point timestamp;
        uint64_t count{};

        [[nodiscard]] uint64_t primary_key() const { return owner.value; }
        [[nodiscard]] uint64_t by_count() const { return -count; }
    };

    // Holds aggregated datapoints
    TABLE bars {
        uint64_t id;

        uint64_t high;
        uint64_t low;
        uint64_t median;
        time_point timestamp;

        [[nodiscard]] uint64_t primary_key() const { return id; }
        uint64_t by_timestamp() const { return timestamp.elapsed.to_seconds(); }
    };

    // Holds users information
    TABLE users {
        name name;
        asset contribution;
        uint64_t score;
        time_point creation_timestamp;

        [[nodiscard]] uint64_t primary_key() const { return name.value; }
        uint64_t by_score() const { return score; }
    };

    TABLE abusers {
        name name;
        uint64_t votes;

        [[nodiscard]] uint64_t primary_key() const { return name.value; }
        uint64_t by_votes() const { return votes; }
    };

    //Holds custodians information
    TABLE custodians {
        name name;

        [[nodiscard]] uint64_t primary_key() const { return name.value; }
    };

    //Holds the list of pairs
    TABLE pairs {
        bool active = false;
        name name;

        symbol base_symbol;
        asset_type base_type;
        eosio::name base_contract;

        symbol quote_symbol;
        asset_type quote_type;
        eosio::name quote_contract;

        uint64_t quoted_precision;

        [[nodiscard]] uint64_t primary_key() const { return name.value; }
    };

    // Multi index types definition

    typedef eosio::multi_index<"stats"_n, stats,
        indexed_by<"count"_n, const_mem_fun<stats, uint64_t, &stats::by_count> >
    > stats_table;

    typedef eosio::multi_index<"pairs"_n, pairs> pairstable;
    typedef eosio::multi_index<"npairs"_n, pairs> npairstable;

    typedef eosio::multi_index<"datapoints"_n, datapoints,
        indexed_by<"value"_n, const_mem_fun<datapoints, uint64_t, &datapoints::by_value> >,
        indexed_by<"timestamp"_n, const_mem_fun<datapoints, uint64_t, &datapoints::by_timestamp> >
    > data_points_table;

    typedef eosio::multi_index<"producers"_n, producer_info,
        indexed_by<"prototalvote"_n, const_mem_fun<producer_info, double, &producer_info::by_votes> >
    > producers_table;

    ACTION configure(const globalinput &g, name default_pair);

    ACTION write(name owner, const std::vector<quote> &quotes);

    ACTION newpair(pairinput pair);

    ACTION deletepair(name pair_name, const std::string &reason) const;

    ACTION clear(name pair_name) const;

    using write_action = action_wrapper<"write"_n, &delphioracle::write>;
    using configure_action = action_wrapper<"configure"_n, &delphioracle::configure>;
    using newpair_action = action_wrapper<"newpair"_n, &delphioracle::newpair>;
    using deletepair_action = action_wrapper<"deletepair"_n, &delphioracle::deletepair>;
    using clear_action = action_wrapper<"clear"_n, &delphioracle::clear>;

private:
    // Check if calling account is a qualified oracle
    bool check_oracle(const name owner) {
        global_table global(_self, _self.value);

        producers_table prod_table("eosio"_n, name("eosio").value);

        const auto min_rank = global.get().minimum_rank;

        auto p_idx = prod_table.get_index<"prototalvote"_n>();
        auto p_itr = p_idx.begin();
        uint64_t count = 0;
        while (p_itr != p_idx.end()) {
            if (p_itr->owner == owner) return true;
            p_itr++;
            count++;
            if (count > min_rank) break;
        }
        return false;
    }

    // Ensure account cannot push data more often than every 60 seconds
    void check_last_push(const name owner, const name pair) {
        global_table global(_self, _self.value);

        stats_table gstore(_self, _self.value);
        stats_table store(_self, pair.value);

        auto itr = store.find(owner.value);

        if (itr != store.end()) {
            time_point ctime = current_time_point();
            auto last = store.get(owner.value);

            time_point next_push =
                    eosio::time_point(last.timestamp.elapsed + eosio::microseconds(global.get().write_cooldown));

            check(ctime >= next_push, "can only call every 60 seconds");
            store.modify(itr, _self, [&](auto &s) {
                s.timestamp = ctime;
                ++s.count;
            });
        } else {
            store.emplace(_self, [&](auto &s) {
                s.owner = owner;
                s.timestamp = current_time_point();
                s.count = 1;
            });
        }

        auto gsitr = gstore.find(owner.value);
        if (gsitr != gstore.end()) {
            time_point ctime = current_time_point();

            gstore.modify(gsitr, _self, [&](auto &s) {
                s.timestamp = ctime;
                ++s.count;
            });
        } else {
            gstore.emplace(_self, [&](auto &s) {
                s.owner = owner;
                s.timestamp = current_time_point();
                s.count = 1;
            });
        }
    }

    //Push oracle message on top of queue, pop oldest element if queue size is larger than datapoints_count
    void update_datapoints(const name owner, const uint64_t value, pairstable::const_iterator pair_itr) {
        global_table global(get_self(), get_self().value);

        data_points_table dstore(_self, pair_itr->name.value);

        uint64_t median = 0;

        auto t_idx = dstore.get_index<"timestamp"_n>();
        auto oldest = t_idx.begin();

        t_idx.modify(oldest, _self, [&](auto &s) {
            s.owner = owner;
            s.value = value;
            s.timestamp = current_time_point();
        });

        // Get index sorted by value
        auto value_sorted = dstore.get_index<"value"_n>();

        // skip first 10 values
        auto itr = value_sorted.begin();
        itr++;
        itr++;
        itr++;
        itr++;
        itr++;
        itr++;
        itr++;
        itr++;
        itr++;

        median = itr->value;

        //set median
        t_idx.modify(oldest, _self, [&](auto &s) {
            s.median = median;
        });

        // update total datapoints
        auto [total_datapoints] = global_stats.get_or_default(GlobalStatsModel{
            .total_datapoints = 0
        });

        global_stats.set(GlobalStatsModel{
                             .total_datapoints = total_datapoints + 1
                         }, _self);
    }
};
#endif // DELPHIORACLE_HPP
