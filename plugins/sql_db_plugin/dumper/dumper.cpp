#include "dumper.h"

#include <cstdio>

namespace eosio {

dumper::dumper(const std::string &file_name, uint32_t block_num_start) {
    m_block_num_start = block_num_start;
    m_file_name = file_name;

    m_abi_map.clear();
    chain::abi_def abi;
    abi = chain::eosio_contract_abi(abi);
    m_abi_map[chain::config::system_account_name].set_abi(abi, fc::seconds(10));

    m_file.open(file_name.c_str(), std::ofstream::out | std::ofstream::app);
}

dumper::~dumper() {
    m_file.close();
}

void dumper::consume(const std::deque<chain::block_state_ptr>& blocks) {
    try {
        if (m_block_num_start > 0 && blocks[0]->block_num + blocks.size() < m_block_num_start) {
            return;
        }
        dlog("consuming " + std::to_string(blocks[0]->block_num) + "; and consume "  + std::to_string(blocks.size()) + " blocks.");
        for (const auto &block : blocks) {
            if (m_block_num_start > 0 && block->block_num < m_block_num_start) {
                continue;
            }
            m_file << "Block: " << block->block_num << std::endl;
            for (const auto &transaction : block->trxs) {
                uint8_t seq = 0;
                m_file << "  Transaction: " << transaction->trx.id().str() << " " << boost::chrono::seconds{transaction->trx.expiration.sec_since_epoch()}.count() << std::endl;
                for (const auto &action : transaction->trx.actions) {
                    m_file << "    Action: " << seq << " ";
                    seq++;
                    m_file << action.account.to_string() << " " << action.name.to_string();

                    auto abi_itr = m_abi_map.find(action.account);
                    if (abi_itr != m_abi_map.end()) {
                        std::string json_str;
                        try {
                            auto abi_data = abi_itr->second.binary_to_variant(abi_itr->second.get_action_type(action.name), action.data, fc::seconds(10));
                            json_str = fc::json::to_string(abi_data);
                        } catch (...) {
                            json_str.clear();
                        }
                        if (!json_str.empty()) {
                            m_file << " " << json_str;
                        }

                        if (action.name == chain::config::system_account_name) {
                            if (action.name == chain::setabi::get_name()) {
                                chain::abi_def abi_setabi;
                                chain::setabi action_data = action.data_as<chain::setabi>();
                                chain::abi_serializer::to_abi(action_data.abi, abi_setabi);
                                m_abi_map[action.name].set_abi(abi_setabi, fc::seconds(10));
                            }
                        }
                    }
                    m_file << std::endl;
                }
            }
        }
    } catch (const std::exception &ex) {
        elog("${e}", ("e", ex.what())); // prevent crash
    } catch (...) {
        elog("Unknown exception during consuming.");// Do no thing.
    }
}

void
dumper::wipe() {
    //std::remove(m_file_name.c_str());
}

bool dumper::is_started() {
    return true;
}

} // namespace
