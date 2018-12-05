#include "blocks_table.h"

#include <fc/log/logger.hpp>

namespace eosio {

blocks_table::blocks_table(std::shared_ptr<soci::session> write_session):
        m_write_session(write_session) { }

void blocks_table::drop() {
    try {
        *m_write_session << "DROP TABLE IF EXISTS blocks";
    }
    catch(std::exception& e){
        wlog(e.what());
    }
}

void blocks_table::create() {
    *m_write_session << "CREATE TABLE blocks("
            "id VARCHAR(64) PRIMARY KEY,"
            "block_number INT NOT NULL AUTO_INCREMENT,"
            "prev_block_id VARCHAR(64),"
            "irreversible TINYINT(1) DEFAULT 0,"
            "timestamp DATETIME DEFAULT NOW(),"
            "transaction_merkle_root VARCHAR(64),"
            "action_merkle_root VARCHAR(64),"
            "producer VARCHAR(12),"
            "version INT NOT NULL DEFAULT 0,"
            "new_producers JSON DEFAULT NULL,"
            "num_transactions INT DEFAULT 0,"
            "confirmed INT, UNIQUE KEY block_number (block_number)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE utf8mb4_general_ci;";

    *m_write_session << "CREATE INDEX idx_blocks_number ON blocks (block_number);";
}

void blocks_table::add(chain::signed_block_ptr block) {
    const auto block_id_str = block->id().str();
    const auto previous_block_id_str = block->previous.str();
    const auto transaction_mroot_str = block->transaction_mroot.str();
    const auto action_mroot_str = block->action_mroot.str();
    const auto timestamp = std::chrono::seconds{block->timestamp.operator fc::time_point().sec_since_epoch()}.count();
    const auto num_transactions = (int)block->transactions.size();

    *m_write_session << "INSERT INTO blocks(id, block_number, prev_block_id, timestamp, transaction_merkle_root, action_merkle_root,"
            "producer, version, confirmed, num_transactions) VALUES (:id, :in, :pb, FROM_UNIXTIME(:ti), :tr, :ar, :pa, :ve, :pe, :nt)",
            soci::use(block_id_str),
            soci::use(block->block_num()),
            soci::use(previous_block_id_str),
            soci::use(timestamp),
            soci::use(transaction_mroot_str),
            soci::use(action_mroot_str),
            soci::use(block->producer.to_string()),
            soci::use(block->schedule_version),
            soci::use(block->confirmed),
            soci::use(num_transactions);

    if (block->new_producers) {
        const auto new_producers = fc::json::to_string(block->new_producers->producers);
        *m_write_session << "UPDATE blocks SET new_producers = :np WHERE id = :id",
                soci::use(new_producers),
                soci::use(block_id_str);
    }
}

void blocks_table::add(const fc::variant& block, uint64_t timestamp) {
    const auto block_id_str = block["id"].as_string();
    const auto block_num = block["block_num"].as_uint64();
    const auto previous_block_id_str = block["previous"].as_string();
    const auto transaction_mroot_str = block["transaction_mroot"].as_string();
    const auto action_mroot_str = block["action_mroot"].as_string();
    const auto schedule_version = block["schedule_version"].as_uint64();
    const auto confirmed = block["confirmed"].as_uint64();
    const auto num_transactions = block["transactions"].get_array().size();
    const auto new_producers = block["new_producers"];
    const auto producer = block["producer"].as_string();

    *m_write_session << "INSERT INTO blocks(id, block_number, prev_block_id, timestamp, transaction_merkle_root, action_merkle_root,"
            "producer, version, confirmed, num_transactions) VALUES (:id, :in, :pb, FROM_UNIXTIME(:ti), :tr, :ar, :pa, :ve, :pe, :nt)",
            soci::use(block_id_str),
            soci::use(block_num),
            soci::use(previous_block_id_str),
            soci::use(timestamp),
            soci::use(transaction_mroot_str),
            soci::use(action_mroot_str),
            soci::use(producer),
            soci::use(schedule_version),
            soci::use(confirmed),
            soci::use(num_transactions);

    if (!new_producers.is_null()) {
        const auto new_producers_str = fc::json::to_string(new_producers);
        *m_write_session << "UPDATE blocks SET new_producers = :np WHERE id = :id",
                soci::use(new_producers_str),
                soci::use(block_id_str);
    }
}

} // namespace
