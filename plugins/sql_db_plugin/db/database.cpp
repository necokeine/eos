#include "database.h"

namespace eosio {

database::database(const std::string &uri, chain_plugin* chain_plug) :
  m_chain_plug(chain_plug),
  m_read_only_api(chain_plug->get_read_only_api()) {
    m_write_session = std::make_shared<soci::session>(uri);
    m_read_session = std::make_shared<soci::session>(uri);
    m_accounts_table = std::make_unique<accounts_table>(m_read_session, m_write_session);
    m_blocks_table = std::make_unique<blocks_table>(m_write_session);
    m_transactions_table = std::make_unique<transactions_table>(m_write_session);
    m_actions_table = std::make_unique<actions_table>(m_read_session, m_write_session);
    system_account = chain::name(chain::config::system_account_name).to_string();
}

void database::check_session(std::shared_ptr<soci::session> session) {
    //mysql_session_backend* mysql_backend = static_cast<mysql_session_backend*>(session->get_backend());
    //if (mysql_ping(mysql_backend->conn_) == 1) {
    session->reconnect();
    //}
}

void database::process_block(const unsigned int block_num) {
    try {
        dlog("djkfljdsfklaj");
        chain_apis::read_only::get_block_params request;
        request.block_num_or_id = std::to_string(block_num);
        auto block = m_read_only_api.get_block(request);
        dlog(fc::json::to_string(block));
        return;
        
        for (const auto& transaction : block["transactions"].get_array()) {
            if (transaction["status"].as<chain::transaction_receipt_header::status_enum>() !=
                chain::transaction_receipt_header::executed) continue;

            chain::time_point_sec expiration;
            fc::from_variant(transaction["trx"]["expiration"], expiration);
            const auto timestamp = std::chrono::seconds{expiration.sec_since_epoch()}.count();

            uint16_t seq = 0;
            for (const auto& action : transaction["trx"]["transaction"]["actions"].get_array()) {
                //m_actions_table->add(action, transaction_id);
                seq ++;
            }
            for (const auto& inline_action : transaction["trx"]["transaction"]["actions"].get_array()) {
                seq ++;
            }
            m_transactions_table->add(block_num, block["ref_block_prefix"].as_uint64(), timestamp, transaction);
        }

        chain::time_point_sec time;
        fc::from_variant(block["timestamp"], time);
        const auto timestamp = std::chrono::seconds{time.sec_since_epoch()}.count();
        // Block table.
        m_blocks_table->add(block, timestamp);
    } catch (...) {

    }
}

void database::consume(const unsigned int left, const unsigned int right) {
    try {
        ilog("consuming " + std::to_string(left) + " and consume "  + std::to_string(right - left + 1) + " blocks.");
        check_session(m_read_session);
        check_session(m_write_session);
        for (auto i = left; i <= right; i++) {
            process_block(i);
        }
    } catch (const std::exception &ex) {
        elog("${e}", ("e", ex.what())); // prevent crash
    } catch (...) {
        elog("Unknown exception during consuming.");// Do no thing.
    }
}

void
database::wipe() {
    *m_write_session << "SET foreign_key_checks = 0;";

    m_actions_table->drop();
    m_transactions_table->drop();
    m_blocks_table->drop();
    m_accounts_table->drop();

    //*m_write_session << "SET foreign_key_checks = 1;";

    m_accounts_table->create();
    m_blocks_table->create();
    m_transactions_table->create();
    m_actions_table->create();

    m_accounts_table->add(system_account);
}

bool database::is_started() {
    return m_accounts_table->exist(system_account);
}

} // namespace
