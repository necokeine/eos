#include "database.h"

namespace eosio {

database::database(const std::string &uri) {
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

void database::process_block(const chain::block_state_ptr & block) {
    try {
        const static int max_error_count = 3;
        int error_count = 0;
        while (error_count < max_error_count) {
            error_count ++;
            try {
                m_blocks_table->add(block->block);
                for (const auto &transaction : block->trxs) {
                    m_transactions_table->add(block->block_num, transaction->trx);
                }
                error_count = 0;
                break;
            } catch (const std::exception &ex) {
                elog("Standard exception in exposing block " + std::to_string(block->block_num) + " : ${e}", ("e", ex.what()));
            } catch (const fc::assert_exception &ex) { // malformed actions
                wlog("Fc exception in assert exception in block " + std::to_string(block->block_num) + " : ${e}", ("e", ex.what()));
            } catch (...) {
                elog("Unknown expection during adding block: " + std::to_string(block->block_num));
            }
        }

        error_count = 0;
        while (error_count < max_error_count) {
            error_count ++;
            try {
                bool has_error = false;
                for (const auto &transaction : block->trxs) {
                    uint8_t seq = 0;
                    for (const auto& action : transaction->trx.actions) {
                        try {
                            m_actions_table->add(action, transaction->trx.id(), transaction->trx.expiration, seq);
                            seq++;
                        } catch (const fc::assert_exception &ex) { // malformed actions
                            wlog("${e}", ("e", ex.what()));
                            has_error = true;
                            continue;
                        } catch (const std::exception& ex) {
                            wlog("${e}", ("e", ex.what()));
                            has_error = true;
                            continue;
                        } catch (...) {
                            wlog("Unknown error in exposing Action");
                            has_error = true;
                            continue;
                        }
                    }
                }
                if (!has_error) {
                    error_count = 0;
                    break;
                }
            } catch (const std::exception& ex) {
                elog("Standard exception in exposing actions of block " + std::to_string(block->block_num) + " : ${e}", ("e", ex.what()));
            } catch (const fc::assert_exception &ex) { // malformed actions
                wlog("Fc exception in assert exception in actions of block " + std::to_string(block->block_num) + " : ${e}", ("e", ex.what()));
            } catch (...) {
                elog("Unknown expection during adding actions of block: " + std::to_string(block->block_num));
            }
        }
    } catch (...) {
        elog("Unknown exception during consuming block: " + std::to_string(block->block_num));
    }
}

void database::consume(const std::deque<chain::block_state_ptr>& blocks) {
    try {
        ilog("consuming " + std::to_string(blocks.front()->block_num) + "; and consume "  + std::to_string(blocks.size()) + " blocks.");
        check_session(m_read_session);
        check_session(m_write_session);
        while (!blocks.empty()) {
            process_block(blocks.front());
            blocks.pop_front();
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
