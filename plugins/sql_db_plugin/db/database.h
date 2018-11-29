#ifndef DATABASE_H
#define DATABASE_H

#include "consumer_core.h"

#include <memory>
#include <mutex>
#include <atomic>
#include <deque>

#include <soci/soci.h>

#include <eosio/chain/config.hpp>
#include <eosio/chain/block_state.hpp>
#include <eosio/chain/types.hpp>

#include "accounts_table.h"
#include "transactions_table.h"
#include "blocks_table.h"
#include "actions_table.h"

namespace eosio {

class database : public consumer_core<chain::block_state_ptr> {
public:
    database(const std::string& uri, uint32_t block_num_start);

    void consume(const std::deque<chain::block_state_ptr>& blocks) override;

    void wipe();
    bool is_started();

private:
    void check_session(soci::session* session);
    void process_block(const chain::block_state_ptr & block);

    soci::session* m_read_session;
    soci::session* m_write_session;
    std::unique_ptr<accounts_table> m_accounts_table;
    std::unique_ptr<actions_table> m_actions_table;
    std::unique_ptr<blocks_table> m_blocks_table;
    std::unique_ptr<transactions_table> m_transactions_table;
    std::string system_account;
    std::atomic_bool m_stoped;
    std::string m_database_uri;
    uint32_t m_block_num_start;
};

} // namespace

#endif // DATABASE_H
