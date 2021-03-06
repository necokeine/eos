#include "actions_table.h"

namespace eosio {

actions_table::actions_table(
    std::shared_ptr<soci::session> read_session,
    std::shared_ptr<soci::session> write_session):
    m_read_session(read_session), m_write_session(write_session) { }

void actions_table::drop() {
    try {
        *m_write_session << "drop table IF EXISTS actions_accounts";
        *m_write_session << "drop table IF EXISTS stakes";
        *m_write_session << "drop table IF EXISTS votes";
        *m_write_session << "drop table IF EXISTS tokens";
        *m_write_session << "drop table IF EXISTS actions";
        *m_write_session << "drop table IF EXISTS bids";
    } catch(std::exception& e){
        wlog(e.what());
    }
}

void actions_table::create() {
    *m_write_session << "CREATE TABLE actions("
            "id INT NOT NULL AUTO_INCREMENT PRIMARY KEY,"
            "account VARCHAR(12),"
            "transaction_id VARCHAR(64),"
            "seq SMALLINT,"
            "parent INT DEFAULT NULL,"
            "name VARCHAR(12),"
            "created_at DATETIME DEFAULT NOW(),"
            "`eosto` varchar(12) GENERATED ALWAYS AS (`data` ->> '$.to'),"
            "`eosfrom` varchar(12) GENERATED ALWAYS AS (`data` ->> '$.from'),"
            "`quantity` varchar(64) GENERATED ALWAYS AS (`data` ->> '$.quantity'),"
            "`receiver` varchar(12) GENERATED ALWAYS AS (`data` ->> '$.receiver'),"
            "`payer` varchar(12) GENERATED ALWAYS AS (`data` ->> '$.payer'),"
            "data JSON) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE utf8mb4_general_ci;";

    *m_write_session << "CREATE INDEX idx_actions_account ON actions (account);";
    *m_write_session << "CREATE INDEX idx_actions_tx_id ON actions (transaction_id);";
    *m_write_session << "CREATE INDEX idx_actions_created ON actions (created_at);";
    *m_write_session << "CREATE INDEX `idx_actions_eosto` ON `actions`(`eosto`);";
    *m_write_session << "CREATE INDEX `idx_actions_eosfrom` ON `actions`(`eosfrom`);";
    *m_write_session << "CREATE INDEX `idx_actions_receiver` ON `actions`(`receiver`);";
    *m_write_session << "CREATE INDEX `idx_actions_payer` ON `actions`(`payer`);";

    *m_write_session << "CREATE TABLE actions_accounts("
            "actor VARCHAR(12),"
            "permission VARCHAR(12),"
            "action_id INT NOT NULL,"
            "PRIMARY KEY(actor, action_id))" 
            "ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE utf8mb4_general_ci;";

    *m_write_session << "CREATE INDEX idx_actions_actor ON actions_accounts (actor);";
    *m_write_session << "CREATE INDEX idx_actions_action_id ON actions_accounts (action_id);";

    *m_write_session << "CREATE TABLE tokens("
            "account VARCHAR(13),"
            "contract VARCHAR(13),"
            "symbol VARCHAR(10),"
            "amount double(64,4) DEFAULT NULL,"
            "PRIMARY KEY(account, contract, symbol))"
            "ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE utf8mb4_general_ci;"; // TODO: other tokens could have diff format.

    *m_write_session << "CREATE INDEX idx_tokens_account ON tokens (account);";

    *m_write_session << "CREATE TABLE votes("
            "account VARCHAR(13) PRIMARY KEY,"
            "votes JSON,"
            "timestamp DATETIME DEFAULT NOW())"
            "ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE utf8mb4_general_ci;";

    *m_write_session << "CREATE TABLE stakes("
            "account VARCHAR(13) PRIMARY KEY,"
            "cpu REAL(14,4),"
            "net REAL(14,4)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE utf8mb4_general_ci;";

    *m_write_session << "CREATE TABLE bids("
            "newname VARCHAR(13) PRIMARY KEY,"
            "bidder VARCHAR(13),"
            "quantity REAL(14,4),"
            "timestamp DATETIME DEFAULT NOW())"
            "ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE utf8mb4_general_ci;";
}

void actions_table::add(chain::action action, chain::transaction_id_type transaction_id, fc::time_point_sec transaction_time, uint8_t seq) {
    const auto transaction_id_str = transaction_id.str();
    const auto expiration = boost::chrono::seconds{transaction_time.sec_since_epoch()}.count();
    auto abi_itr = m_abi_map.find(action.account);

    if (action.name != N(transfer) && action.name != N(issue) && action.account != chain::name(chain::config::system_account_name)) {
        *m_write_session << "INSERT INTO actions(account, seq, created_at, name, transaction_id) VALUES (:ac, :se, FROM_UNIXTIME(:ca), :na, :ti) ",
            soci::use(action.account.to_string()),
            soci::use(seq),
            soci::use(expiration),
            soci::use(action.name.to_string()),
            soci::use(transaction_id_str);
        return;
    } else {
        // Important action here.
        if (abi_itr == m_abi_map.end()) {
            std::string abi_def_account;
            chain::abi_def abi;
            soci::indicator ind;
            *m_read_session << "SELECT abi FROM accounts WHERE name = :name", soci::into(abi_def_account, ind), soci::use(action.account.to_string());
            if (!abi_def_account.empty()) {
                abi = fc::json::from_string(abi_def_account).as<chain::abi_def>();
            } else if (action.account == chain::config::system_account_name) {
                abi = chain::eosio_contract_abi(abi);
            } else {
                *m_write_session << "INSERT INTO actions(account, seq, created_at, name, transaction_id) VALUES (:ac, :se, FROM_UNIXTIME(:ca), :na, :ti) ",
                    soci::use(action.account.to_string()),
                    soci::use(seq),
                    soci::use(expiration),
                    soci::use(action.name.to_string()),
                    soci::use(transaction_id_str);
                return;
            }
            m_abi_map[action.account].set_abi(abi, fc::seconds(10));
            abi_itr = m_abi_map.find(action.account);
        }
    }

    auto abi_data = abi_itr->second.binary_to_variant(abi_itr->second.get_action_type(action.name), action.data, fc::seconds(10));
    string json = fc::json::to_string(abi_data);

//    boost::uuids::random_generator gen;
//    boost::uuids::uuid id = gen();
//    std::string action_id = boost::uuids::to_string(id);

    soci::transaction tran(*m_write_session);
    *m_write_session << "INSERT INTO actions(account, seq, created_at, name, data, transaction_id) VALUES (:ac, :se, FROM_UNIXTIME(:ca), :na, :da, :ti) ",
            soci::use(action.account.to_string()),
            soci::use(seq),
            soci::use(expiration),
            soci::use(action.name.to_string()),
            soci::use(json),
            soci::use(transaction_id_str);

//    for (const auto& auth : action.authorization) {
//        *m_write_session << "INSERT INTO actions_accounts(action_id, actor, permission) VALUES (LAST_INSERT_ID(), :ac, :pe) ",
//                soci::use(auth.actor.to_string()),
//                soci::use(auth.permission.to_string());
//    }
    try {
        parse_actions(action, abi_data, expiration);
        tran.commit();
    } catch(std::exception& e){
        wlog(e.what());
    } catch(fc::exception& e) {
        wlog(e.what());
    } catch(...) {
        elog("Unknown error during parsing action data: " + transaction_id_str);
    }
}

void actions_table::add(const fc::variant& action, const std::string& transaction_id_str, uint64_t timestamp, uint8_t seq) {
    const string& action_account = action["account"].as_string();
    const string& action_name = action["name"].as_string();
    const string& action_data = action["data"].as_string();
    const string& json = "";

    bool error_during_parse_actions = false;
    soci::transaction tran(*m_write_session);
    *m_write_session << "INSERT INTO actions(account, seq, created_at, name, data, transaction_id) VALUES (:ac, :se, FROM_UNIXTIME(:ca), :na, :da, :ti) ",
            soci::use(action_account),
            soci::use(seq),
            soci::use(timestamp),
            soci::use(action_name),
            soci::use(json),
            soci::use(transaction_id_str);

//    for (const auto& auth : action.authorization) {
//        *m_write_session << "INSERT INTO actions_accounts(action_id, actor, permission) VALUES (LAST_INSERT_ID(), :ac, :pe) ",
//                soci::use(auth.actor.to_string()),
//                soci::use(auth.permission.to_string());
//    }
    try {
        //parse_actions(action, abi_data, expiration);
        tran.commit();
    } catch(std::exception& e){
        wlog(e.what());
        error_during_parse_actions = true;
    } catch(fc::exception& e) {
        wlog(e.what());
    } catch(...) {
        error_during_parse_actions = true;
        elog("Unknown error during parsing action data: " + transaction_id_str);
    }
    if (error_during_parse_actions) {
        tran.rollback();
        // Insert action without data.
    }
}

void actions_table::remove(uint64_t action_id) {
    soci::transaction tran(*m_write_session);
    try {
        soci::indicator ind_to, ind_from, ind_quantity;
        std::string eosfrom, eosto, quantity;
        std::string account, name;
        *m_read_session << "SELECT account, name, eosfrom, eosto, quantity FROM actions WHERE id = :id",
            soci::into(account), soci::into(name), soci::into(eosfrom, ind_from), soci::into(eosto, ind_to), soci::into(quantity, ind_quantity),
            soci::use(action_id);
        if (ind_quantity != soci::i_null) {
            auto ast = eosio::chain::asset::from_string(quantity);
            if (ind_to != soci::i_null) {
                *m_write_session << "UPDATE tokens SET amount = amount - :am WHERE account = :ac, contract = :co, symbol = :sy",
                    soci::use(ast.to_real()), soci::use(eosto), soci::use(account), soci::use(ast.get_symbol().name());
            }
            if (ind_from != soci::i_null) {
                *m_write_session << "UPDATE tokens SET amount = amount + :am WHERE account = :ac, contract = :co, symbol = :sy",
                    soci::use(ast.to_real()), soci::use(eosfrom), soci::use(account), soci::use(ast.get_symbol().name());
            }
        }
        *m_write_session << "DELETE FROM actions WHERE id = :id",
            soci::use(action_id);
        tran.commit();
    } catch(std::exception& e) {
        elog(string("STD error ") + e.what() + " during delete action : " + std::to_string(action_id));
    } catch(fc::exception& e) {
        elog(string("FC error ") + e.what() + " during delete action : " + std::to_string(action_id));
    } catch(...) {
        elog("Unknown error during delete action : " + std::to_string(action_id));
    }
}


void parse_actions(const fc::variant& action, const string& account_name, const string& action_name, uint64_t timestamp) {
    if (action_name == "issue") {
        auto to_name = action["data"]["to"].as<chain::name>().to_string();

    } else if (action_name == "transfer") {

    }
}

void actions_table::parse_actions(chain::action action, fc::variant abi_data, uint64_t timestamp) {
    // TODO: move all  + catch // public keys update // stake / voting
    auto contract = action.account.to_string();
    if (action.name == N(issue)) {
        auto to_name = abi_data["to"].as<chain::name>().to_string();
        auto asset_quantity = abi_data["quantity"].as<chain::asset>();

        *m_write_session << "INSERT INTO tokens(account, contract, amount, symbol) VALUES (:ac, :co, :am, :as) "
            "ON DUPLICATE KEY UPDATE amount = amount + :am",
                    soci::use(to_name),
                    soci::use(contract),
                    soci::use(asset_quantity.to_real()),
                    soci::use(asset_quantity.get_symbol().name()),
                    soci::use(asset_quantity.to_real());
    }

    if (action.name == N(transfer)) {
        auto from_name = abi_data["from"].as<chain::name>().to_string();
        auto to_name = abi_data["to"].as<chain::name>().to_string();
        auto asset_quantity = abi_data["quantity"].as<chain::asset>();

        *m_write_session << "INSERT INTO tokens(account, contract, amount, symbol) VALUES (:ac, :co, :am, :as) "
            "ON DUPLICATE KEY UPDATE amount = amount + :am",
                    soci::use(to_name),
                    soci::use(contract),
                    soci::use(asset_quantity.to_real()),
                    soci::use(asset_quantity.get_symbol().name()),
                    soci::use(asset_quantity.to_real());

        *m_write_session << "INSERT INTO tokens(account, contract, amount, symbol) VALUES (:ac, :co, :am, :as) "
            " ON DUPLICATE KEY UPDATE amount = amount - :am",
                soci::use(from_name),
                soci::use(contract),
                soci::use(-asset_quantity.to_real()),
                soci::use(asset_quantity.get_symbol().name()),
                soci::use(asset_quantity.to_real());
    }

    if (action.account != chain::name(chain::config::system_account_name)) {
        return;
    }

    if (action.name == N(bidname)) {
        auto bidder = abi_data["bidder"].as<chain::name>().to_string();
        auto newname = abi_data["newname"].as<chain::name>().to_string();
        auto quantity = abi_data["bid"].as<chain::asset>().to_real();
        *m_write_session << "INSERT INTO bids(bidder, newname, quantity, timestamp) VALUES (:bd, :ne, :qu, FROM_UNIXTIME(:ts)) "
            "ON DUPLICATE KEY UPDATE bidder = :db, quantity = :qu, timestamp = FROM_UNIXTIME(:ts)",
            soci::use(bidder),
            soci::use(newname),
            soci::use(quantity),
            soci::use(timestamp),
            soci::use(bidder),
            soci::use(quantity),
            soci::use(timestamp);
    }

  //if (action.name == N(voteproducer)) {
  //    auto voter = abi_data["voter"].as<chain::name>().to_string();
  //    string votes = fc::json::to_string(abi_data["producers"]);

  //    *m_write_session << "INSERT INTO votes(account, votes, timestamp) VALUES (:ac, :vo, FROM_UNIXTIME(:ts)) "
  //        "ON DUPLICATE KEY UPDATE votes = :vo, timestamp = FROM_UNIXTIME(:ts)",
  //            soci::use(voter),
  //            soci::use(votes),
  //            soci::use(timestamp),
  //            soci::use(votes),
  //            soci::use(timestamp);
  //}

  //if (action.name == N(delegatebw)) {
  //    auto account = abi_data["receiver"].as<chain::name>().to_string();
  //    auto cpu = abi_data["stake_cpu_quantity"].as<chain::asset>().to_real();
  //    auto net = abi_data["stake_net_quantity"].as<chain::asset>().to_real();

  //    *m_write_session << "INSERT INTO stakes(account, cpu, net) VALUES (:ac, :cp, :ne) "
  //        "ON DUPLICATE KEY UPDATE cpu = cpu + :cp, net = net + :ne",
  //            soci::use(account),
  //            soci::use(cpu),
  //            soci::use(net),
  //            soci::use(cpu),
  //            soci::use(net);
  //} else if (action.name == N(undelegatebw)) {
  //    auto account = abi_data["receiver"].as<chain::name>().to_string();
  //    auto cpu = -abi_data["unstake_cpu_quantity"].as<chain::asset>().to_real();
  //    auto net = -abi_data["unstake_net_quantity"].as<chain::asset>().to_real();

  //    *m_write_session << "INSERT INTO stakes(account, cpu, net) VALUES (:ac, :cp, :ne) "
  //        "ON DUPLICATE KEY UPDATE cpu = cpu + :cp, net = net + :ne",
  //            soci::use(account),
  //            soci::use(cpu),
  //            soci::use(net),
  //            soci::use(cpu),
  //            soci::use(net);
  //}

    //if (action.name == chain::buyram::get_name()) {
    //}

    if (action.name == chain::setabi::get_name()) {
        chain::abi_def abi_setabi;
        chain::setabi action_data = action.data_as<chain::setabi>();
        chain::abi_serializer::to_abi(action_data.abi, abi_setabi);
        string abi_string = fc::json::to_string(abi_setabi);

        // Clear local abi cache.
        m_abi_map.erase(action.account);
        *m_write_session << "INSERT INTO accounts (name, abi, updated_at) VALUES (:na, :abi, :ua) "
            "ON DUPLICATE KEY UPDATE abi = :abi, updated_at = FROM_UNIXTIME(:ua) ",
                soci::use(action_data.account.to_string()),
                soci::use(abi_string),
                soci::use(timestamp),
                soci::use(abi_string),
                soci::use(timestamp);

    } else if (action.name == chain::newaccount::get_name()) {
        auto action_data = action.data_as<chain::newaccount>();
        *m_write_session << "INSERT INTO accounts (name, created_at, updated_at) VALUES (:name, FROM_UNIXTIME(:ca), FROM_UNIXTIME(:ua)) "
            "ON DUPLICATE KEY UPDATE created_at = FROM_UNIXTIME(:ca)",
                soci::use(action_data.name.to_string()),
                soci::use(timestamp),
                soci::use(timestamp),
                soci::use(timestamp);

        for (const auto& key_owner : action_data.owner.keys) {
            string permission_owner = "owner";
            string public_key_owner = static_cast<string>(key_owner.key);
            *m_write_session << "INSERT INTO accounts_keys(account, public_key, permission) VALUES (:ac, :ke, :pe) ",
                    soci::use(action_data.name.to_string()),
                    soci::use(public_key_owner),
                    soci::use(permission_owner);
        }

        for (const auto& key_active : action_data.active.keys) {
            string permission_active = "active";
            string public_key_active = static_cast<string>(key_active.key);
            *m_write_session << "INSERT INTO accounts_keys(account, public_key, permission) VALUES (:ac, :ke, :pe) ",
                    soci::use(action_data.name.to_string()),
                    soci::use(public_key_active),
                    soci::use(permission_active);
        }
    }
}

} // namespace
