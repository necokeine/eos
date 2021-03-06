#include "transactions_table.h"

#include <chrono>
#include <fc/log/logger.hpp>

namespace eosio {

transactions_table::transactions_table(std::shared_ptr<soci::session> write_session):
    m_write_session(write_session) {
}

void transactions_table::drop() {
    try {
        *m_write_session << "DROP TABLE IF EXISTS transactions";
    } catch(std::exception& e){
        wlog(e.what());
    }
}

void transactions_table::create() {
    *m_write_session << "CREATE TABLE transactions("
            "id VARCHAR(64) PRIMARY KEY,"
            "block_id INT NOT NULL,"
            "ref_block_num INT NOT NULL,"
            "ref_block_prefix INT,"
            "expiration DATETIME DEFAULT NOW(),"
            "pending TINYINT(1),"
            "created_at DATETIME DEFAULT NOW(),"
            "num_actions INT DEFAULT 0,"
            "updated_at DATETIME DEFAULT NOW()) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE utf8mb4_general_ci;";

    *m_write_session << "CREATE INDEX transactions_block_id ON transactions (block_id);";

}

void transactions_table::add(uint32_t block_id, chain::transaction transaction) {
    const auto transaction_id_str = transaction.id().str();
    const auto expiration = std::chrono::seconds{transaction.expiration.sec_since_epoch()}.count();
    *m_write_session << "INSERT INTO transactions(id, block_id, ref_block_num, ref_block_prefix,"
            "expiration, pending, created_at, updated_at, num_actions) VALUES (:id, :bi, :rbi, :rb, FROM_UNIXTIME(:ex), :pe, FROM_UNIXTIME(:ca), FROM_UNIXTIME(:ua), :na)",
            soci::use(transaction_id_str),
            soci::use(block_id),
            soci::use(transaction.ref_block_num),
            soci::use(transaction.ref_block_prefix),
            soci::use(expiration),
            soci::use(0),
            soci::use(expiration),
            soci::use(expiration),
            soci::use(transaction.total_actions());
}

void transactions_table::add(uint32_t block_id, uint64_t ref_block_prefix, uint64_t timestamp, const fc::variant& transaction) {
    const auto transaction_id_str = transaction["trx"]["id"].as_string();
    const auto ref_block_num = transaction["trx"]["ref_block_num"].as_uint64();
    const auto total_actions = transaction["trx"]["transaction"]["actions"].get_array().size() + transaction["trx"]["transaction"]["context_free_actions"].get_array().size();
    *m_write_session << "INSERT INTO transactions(id, block_id, ref_block_num, ref_block_prefix,"
        "expiration, pending, created_at, updated_at, num_actions) VALUES (:id, :bi, :rbi, :rb, FROM_UNIXTIME(:ex), :pe, FROM_UNIXTIME(:ca), FROM_UNIXTIME(:ua), :na)",
        soci::use(transaction_id_str),
        soci::use(block_id),
        soci::use(ref_block_num),
        soci::use(ref_block_prefix),
        soci::use(timestamp),
        soci::use(0),
        soci::use(timestamp),
        soci::use(timestamp),
        soci::use(total_actions);

}

} // namespace
