#include "accounts_table.h"

#include <fc/log/logger.hpp>

namespace eosio {

accounts_table::accounts_table(std::shared_ptr<soci::session> read_session, std::shared_ptr<soci::session> write_session):
    m_read_session(read_session), m_write_session(write_session) { }

void accounts_table::drop() {
    try {
        *m_write_session << "DROP TABLE IF EXISTS accounts_keys";
        *m_write_session << "DROP TABLE IF EXISTS accounts";
    } catch(std::exception& e){
        wlog(e.what());
    }
}

void accounts_table::create() {
    *m_write_session << "CREATE TABLE accounts("
            "name VARCHAR(12) PRIMARY KEY,"
            "abi JSON DEFAULT NULL,"
            "created_at DATETIME DEFAULT NOW(),"
            "updated_at DATETIME DEFAULT NOW()) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE utf8mb4_general_ci;";

    *m_write_session << "CREATE TABLE accounts_keys("
            "account VARCHAR(12),"
            "permission VARCHAR(12),"
            "public_key varchar(64) DEFAULT NULL,"
            "FOREIGN KEY (account) REFERENCES accounts(name)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE utf8mb4_general_ci;";
}

void accounts_table::add(string name) {
    *m_write_session << "INSERT INTO accounts (name) VALUES (:name)",
            soci::use(name);
}

bool accounts_table::exist(string name) {
    int amount;
    try {
        *m_read_session << "SELECT COUNT(*) FROM accounts WHERE name = :name", soci::into(amount), soci::use(name);
    }
    catch (std::exception const & e)
    {
        amount = 0;
    }
    return amount > 0;
}

} // namespace
