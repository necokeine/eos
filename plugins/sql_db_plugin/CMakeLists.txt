message(STATUS "Database SQL plugin: enabled")

include_directories(${CMAKE_CURRENT_SOURCE_DIR} include db)

add_library(sql_db_plugin
    db/database.cpp
    db/accounts_table.cpp
    db/transactions_table.cpp
    db/blocks_table.cpp
    db/actions_table.cpp
    sql_db_plugin.cpp
    )

target_link_libraries(sql_db_plugin
    chain_plugin
    eosio_chain
    libsoci_mysql.so
    libsoci_core.so
    )

#add_subdirectory(test)

