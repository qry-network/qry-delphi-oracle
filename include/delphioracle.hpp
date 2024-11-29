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
#include <cmath>

using namespace eosio;
using namespace std;

const std::string SYSTEM_SYMBOL = "QRY";
const uint64_t SYSTEM_PRECISION = 4;
static const asset one_larimer = asset(1, symbol(SYSTEM_SYMBOL, SYSTEM_PRECISION));

static const std::string system_str("system");

const checksum256 NULL_HASH;

const eosio::time_point NULL_TIME_POINT = eosio::time_point(eosio::microseconds(0));
CONTRACT delphioracle : public eosio::contract {
public:
    using contract::contract;

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
        uint64_t approving_custodians_threshold;
        uint64_t minimum_rank;
        uint64_t paid;
        uint64_t min_bounty_delay;
        uint64_t new_bounty_delay;
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

    /*   struct blockchain_parameters {
          uint64_t free_ram()const { return max_ram_size - total_ram_bytes_reserved; }

          uint64_t             max_ram_size = 64ll*1024 * 1024 * 1024;
          uint64_t             total_ram_bytes_reserved = 0;
          int64_t              total_ram_stake = 0;

          block_timestamp      last_producer_schedule_update;
          time_point           last_pervote_bucket_fill;
          int64_t              pervote_bucket = 0;
          int64_t              perblock_bucket = 0;
          uint32_t             total_unpaid_blocks = 0; /// all blocks which have been produced but not paid
          int64_t              total_activated_stake = 0;
          time_point           thresh_activated_stake_time;
          uint16_t             last_producer_schedule_size = 0;
          double               total_producer_vote_weight = 0; /// the sum of all producer votes
          block_timestamp      last_name_close;

          uint64_t primary_key()const { return 1;      }
       };
    */

    struct producer_info {
        name owner;
        double total_votes = 0;
        public_key producer_key; /// a packed public key object
        bool is_active = true;
        std::string url;
        uint32_t unpaid_blocks = 0;
        time_point last_claim_time;
        uint16_t location = 0;

        uint64_t primary_key() const { return owner.value; }
        double by_votes() const { return is_active ? -total_votes : total_votes; }
        bool active() const { return is_active; }
    };

    struct st_transfer {
        name from;
        name to;
        asset quantity;
        std::string memo;
    };

    //Global config
    TABLE global {
        //variables
        uint64_t id;
        uint64_t total_datapoints_count;
        asset total_claimed = asset(0, symbol(SYSTEM_SYMBOL, SYSTEM_PRECISION));

        //constants
        uint64_t datapoints_per_instrument = 21;
        uint64_t bars_per_instrument = 30;
        uint64_t vote_interval = 10000;
        uint64_t write_cooldown = 1000000 * 55;
        uint64_t approver_threshold = 1;
        uint64_t approving_oracles_threshold = 1;
        uint64_t approving_custodians_threshold = 1;
        uint64_t minimum_rank = 105;
        uint64_t paid = 21;
        uint64_t min_bounty_delay = 604800;
        uint64_t new_bounty_delay = 259200;

        uint64_t primary_key() const { return id; }
    };

    TABLE oglobal {
        uint64_t id;
        uint64_t total_datapoints_count;
        //asset total_claimed;

        uint64_t primary_key() const { return id; }
    };

    // Holds the last datapoints_count datapoints from qualified oracles
    TABLE datapoints {
        uint64_t id;
        name owner;
        uint64_t value;
        uint64_t median;
        time_point timestamp;

        uint64_t primary_key() const { return id; }
        uint64_t by_timestamp() const { return timestamp.elapsed.to_seconds(); }
        uint64_t by_value() const { return value; }
    };

    //Holds the last hashes from qualified oracles
    TABLE hashes {
        uint64_t id;
        name owner;
        checksum256 multiparty;
        checksum256 hash;
        std::string reveal;
        time_point timestamp;

        uint64_t primary_key() const { return id; }
        uint64_t by_timestamp() const { return timestamp.elapsed.to_seconds(); }
        uint64_t by_owner() const { return owner.value; }
        checksum256 by_hash() const { return hash; }
    };

    //Holds the count and time of last writes for qualified oracles
    TABLE stats {
        name owner;
        time_point timestamp;
        uint64_t count;
        time_point last_claim;
        asset balance;

        uint64_t primary_key() const { return owner.value; }
        uint64_t by_count() const { return -count; }
    };

    //Holds aggregated datapoints
    TABLE bars {
        uint64_t id;

        uint64_t high;
        uint64_t low;
        uint64_t median;
        time_point timestamp;

        uint64_t primary_key() const { return id; }
        uint64_t by_timestamp() const { return timestamp.elapsed.to_seconds(); }
    };

    // Holds users information
    TABLE users {
        name name;
        asset contribution;
        uint64_t score;
        time_point creation_timestamp;

        uint64_t primary_key() const { return name.value; }
        uint64_t by_score() const { return score; }
    };

    TABLE abusers {
        name name;
        uint64_t votes;

        uint64_t primary_key() const { return name.value; }
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

    TABLE voter_info {
        name owner;
        name proxy;
        std::vector<name> producers;
        int64_t staked = 0;
        double last_vote_weight = 0;
        double proxied_vote_weight = 0;
        bool is_proxy = 0;
        uint32_t flags1 = 0;
        uint32_t reserved2 = 0;
        eosio::asset reserved3;

        uint64_t primary_key() const { return owner.value; }

        enum class flags1_fields : uint32_t {
            ram_managed = 1,
            net_managed = 2,
            cpu_managed = 4
        };

        // explicit serialization macro is not necessary, used here only to improve compilation time
        EOSLIB_SERIALIZE(
            voter_info,
            (owner)(proxy)(producers)(staked)(last_vote_weight)(proxied_vote_weight)(is_proxy)(flags1)(reserved2)(
                reserved3))
    };

    //Multi index types definition
    typedef eosio::multi_index<"global"_n, global> globaltable;
    typedef eosio::multi_index<"global"_n, oglobal> oglobaltable;

    typedef eosio::multi_index<"custodians"_n, custodians> custodianstable;

    typedef eosio::multi_index<"stats"_n, stats,
        indexed_by<"count"_n, const_mem_fun<stats, uint64_t, &stats::by_count> > > statstable;

    typedef eosio::multi_index<"pairs"_n, pairs> pairstable;
    typedef eosio::multi_index<"npairs"_n, pairs> npairstable;

    typedef eosio::multi_index<"datapoints"_n, datapoints,
        indexed_by<"value"_n, const_mem_fun<datapoints, uint64_t, &datapoints::by_value> >,
        indexed_by<"timestamp"_n, const_mem_fun<datapoints, uint64_t, &datapoints::by_timestamp> > > datapointstable;

    typedef eosio::multi_index<"hashes"_n, hashes,
        indexed_by<"timestamp"_n, const_mem_fun<hashes, uint64_t, &hashes::by_timestamp> >,
        indexed_by<"owner"_n, const_mem_fun<hashes, uint64_t, &hashes::by_owner> >,
        indexed_by<"hash"_n, const_mem_fun<hashes, checksum256, &hashes::by_hash> > > hashestable;

    typedef eosio::multi_index<"voters"_n, voter_info,
        indexed_by<"voter"_n, const_mem_fun<voter_info, uint64_t, &voter_info::primary_key> > > voters_table;

    typedef eosio::multi_index<"producers"_n, producer_info,
        indexed_by<"prototalvote"_n, const_mem_fun<producer_info, double, &producer_info::by_votes> > > producers_table;

    typedef eosio::multi_index<"users"_n, users,
        indexed_by<"score"_n, const_mem_fun<users, uint64_t, &users::by_score> > > userstable;

    typedef eosio::multi_index<"abusers"_n, abusers,
        indexed_by<"votes"_n, const_mem_fun<abusers, uint64_t, &abusers::by_votes> > > abuserstable;

    std::string to_hex(const char *d, uint32_t s) {
        std::string r;
        const char *to_hex = "0123456789abcdef";
        uint8_t *c = (uint8_t *) d;
        for (uint32_t i = 0; i < s; ++i)
            (r += to_hex[(c[i] >> 4)]) += to_hex[(c[i] & 0x0f)];
        return r;
    }

    // Write datapoint
    ACTION write(const name owner, const std::vector<quote> &quotes);

    using write_action = action_wrapper<"write"_n, &delphioracle::write>;

    // Configure contract global1
    ACTION configure(globalinput g);

    using configure_action = action_wrapper<"configure"_n, &delphioracle::configure>;

    // Register new pair
    ACTION newpair(pairinput pair);

    using newpair_action = action_wrapper<"newpair"_n, &delphioracle::newpair>;

    // Delete pair
    ACTION deletepair(name pair_name, const std::string &reason) const;

    using deletepair_action = action_wrapper<"deletepair"_n, &delphioracle::deletepair>;

    // Clear contract data
    ACTION clear(name pair_name) const;

    using clear_action = action_wrapper<"clear"_n, &delphioracle::clear>;

    // Migrate data example
    // ACTION migratedata();

    // using migratedata_action = action_wrapper<"migratedata"_n, &delphioracle::migratedata>;

private:
    // Check if calling account is a qualified oracle
    bool check_oracle(const name owner) {
        const globaltable global_table(_self, _self.value);
        producers_table prod_table("eosio"_n, name("eosio").value);
        const auto min_rank = global_table.begin()->minimum_rank;
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
        globaltable gtable(_self, _self.value);
        statstable gstore(_self, _self.value);
        statstable store(_self, pair.value);

        auto gitr = gtable.begin();
        auto itr = store.find(owner.value);

        if (itr != store.end()) {
            time_point ctime = current_time_point();
            auto last = store.get(owner.value);

            time_point next_push =
                    eosio::time_point(last.timestamp.elapsed + eosio::microseconds(gitr->write_cooldown));

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
                s.balance = asset(0, symbol(SYSTEM_SYMBOL, SYSTEM_PRECISION));
                s.last_claim = NULL_TIME_POINT;
            });
        }

        auto gsitr = gstore.find(owner.value);
        if (gsitr != gstore.end()) {
            time_point ctime = current_time_point();

            gstore.modify(gsitr, _self, [&](auto &s) {
                s.timestamp = ctime;
                s.count++;
            });
        } else {
            gstore.emplace(_self, [&](auto &s) {
                s.owner = owner;
                s.timestamp = current_time_point();
                s.count = 1;
                s.balance = asset(0, symbol(SYSTEM_SYMBOL, SYSTEM_PRECISION));
                s.last_claim = NULL_TIME_POINT;
            });
        }
    }

    //Push oracle message on top of queue, pop oldest element if queue size is larger than datapoints_count
    void update_datapoints(const name owner, const uint64_t value, pairstable::const_iterator pair_itr) {
        globaltable gtable(_self, _self.value);
        datapointstable dstore(_self, pair_itr->name.value);

        uint64_t median = 0;
        //uint64_t primary_key ;

        //Calculate new primary key by substracting one from the previous one
        /*   auto latest = dstore.begin();
           primary_key = latest->id - 1;*/

        auto t_idx = dstore.get_index<"timestamp"_n>();

        auto oldest = t_idx.begin();

        /*

          auto idx = hashtable.get_index<"producer"_n>();
            auto itr = idx.find(producer);
            if (itr != idx.end()) {
              idx.modify(itr, producer, [&](auto& prod_hash) {
                prod_hash.hash = hash;
              });
            } else {*/


        //Pop oldest point
        //t_idx.erase(oldest);

        //Insert next datapoint
        /*    auto c_itr = dstore.emplace(_self, [&](auto& s) {
              s.id = primary_key;
              s.owner = owner;
              s.value = value;
              s.timestamp = current_time_point().sec_since_epoch();
            });*/


        t_idx.modify(oldest, _self, [&](auto &s) {
            // s.id = primary_key;
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

        gtable.modify(gtable.begin(), _self, [&](auto &s) {
            ++s.total_datapoints_count;
        });
    }
};
#endif // DELPHIORACLE_HPP
