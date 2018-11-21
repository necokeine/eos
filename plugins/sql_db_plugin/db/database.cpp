#include "database.h"

namespace eosio {

database::database(const std::string &uri, uint32_t block_num_start) {
    m_write_session = std::make_shared<soci::session>(uri);
    m_read_session = std::make_shared<soci::session>(uri);

    m_accounts_table = std::make_unique<accounts_table>(m_read_session, m_write_session);
    m_blocks_table = std::make_unique<blocks_table>(m_write_session);
    m_transactions_table = std::make_unique<transactions_table>(m_write_session);
    m_actions_table = std::make_unique<actions_table>(m_read_session, m_write_session);
    m_block_num_start = block_num_start;
    system_account = chain::name(chain::config::system_account_name).to_string();
    m_stoped = false;
}

void database::consume(const std::vector<chain::block_state_ptr> &blocks) {
    if (m_stoped) return; // Already a unhandled error happen.
    try {
        dlog("consuming " + std::to_string(blocks[0]->block_num) + "; and consume "  + std::to_string(blocks.size()) + " blocks.");
        for (const auto &block : blocks) {
            try {
                if (m_block_num_start > 0 && block->block_num < m_block_num_start) {
                    continue;
                }
                int error_count = 0;
                while (error_count < 10) {
                    try {
                        error_count ++;
                        m_blocks_table->add(block->block);
                        for (const auto &transaction : block->trxs) {
                            m_transactions_table->add(block->block_num, transaction->trx);
                            uint8_t seq = 0;
                            for (const auto &action : transaction->trx.actions) {
                                try {
                                    m_actions_table->add(action, transaction->trx.id(), transaction->trx.expiration, seq);
                                    seq++;
                                } catch (const fc::assert_exception &ex) { // malformed actions
                                    wlog("${e}", ("e", ex.what()));
                                    continue;
                                }
                            }
                        }
                        error_count = 0; // Commited successfully.
                        break;
                    } catch (const std::exception & ex) {
                        elog("${e}", ("e", ex.what())); // prevent crash
                    } catch (const fc::assert_exception &ex) { // malformed actions
                        wlog("${e}", ("e", ex.what()));
                    } catch (...) {
                        elog(boost::current_exception_diagnostic_information());
                        elog("Unknown expection during consuming block: " + std::to_string(block->block_num));
                    }
                }
                if (error_count) {
                    m_stoped = true;
                }
            } catch (const std::exception & ex) {
                elog("${e}", ("e", ex.what())); // prevent crash
            } catch (const fc::assert_exception &ex) { // malformed actions
                wlog("${e}", ("e", ex.what()));
            } catch (...) {
                elog(boost::current_exception_diagnostic_information());
                elog("Unknown expection during consuming block: " + std::to_string(block->block_num));
            }
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
