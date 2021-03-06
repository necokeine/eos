#ifndef ACCOUNTS_TABLE_H
#define ACCOUNTS_TABLE_H

#include <memory>
#include <soci/soci.h>

namespace eosio {

using std::string;

class accounts_table {
public:
    accounts_table(std::shared_ptr<soci::session> read_session, std::shared_ptr<soci::session> write_session);

    void drop();
    void create();
    void add(string name);
    bool exist(string name);

private:
    std::shared_ptr<soci::session> m_read_session;
    std::shared_ptr<soci::session> m_write_session;
};

} // namespace

#endif // ACCOUNTS_TABLE_H
