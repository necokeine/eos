#ifndef DUMPER_H
#define DUMPER_H

#include "consumer_core.h"

#include <memory>
#include <mutex>
#include <atomic>
#include <deque>
#include <fstream>

#include <eosio/chain/abi_def.hpp>
#include <eosio/chain/abi_serializer.hpp>
#include <eosio/chain/asset.hpp>
#include <eosio/chain/block_state.hpp>
#include <eosio/chain/config.hpp>
#include <eosio/chain/eosio_contract.hpp>
#include <eosio/chain/types.hpp>
#include <eosio/chain/block_state.hpp>

#include <fc/io/json.hpp>
#include <fc/variant.hpp>

namespace eosio {

class dumper : public consumer_core<chain::block_state_ptr> {
public:
    dumper(const std::string& file_name, uint32_t block_num_start);
    ~dumper();

    void consume(const std::deque<chain::block_state_ptr>& blocks) override;

    void wipe();
    bool is_started();

private:
    std::string m_file_name;
    uint32_t m_block_num_start;
    std::unordered_map<chain::name, chain::abi_serializer> m_abi_map;
    std::ofstream m_file;
};

} // namespace

#endif // DUMPER_H
