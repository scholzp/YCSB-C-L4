/******************************************************************************
 *                                                                            *
 * sqlite_shm_db.h - A database backend using the sqlite shared mem server.   *
 *                                                                            *
 * Author: Till Miemietz <till.miemietz@barkhauseninstitut.org>               *
 * Author: Viktor Reusch                                                      *
 *                                                                            *
 ******************************************************************************/

#pragma once

#include "db.h"                     // YCSBC interface for databases
#include "sqlite_shm_server.h"      // Interfaces for the Sqlite server.

#include <sqlite3.h>                // Definitions for Sqlite

#include <string>
#include <vector>

#include <l4/re/env>                // L4 environment

namespace ycsbc {

struct Ctx;

class SqliteShmDB : public DB {
public:
    SqliteShmDB(const std::string &filename = std::string(":memory:"));
    // FIXME: Add destructor.

    // Meta operations for database and/or connection management
    void CreateSchema(DB::Tables tables) override;
    void *Init() override;
    void Close(void *ctx) override;

    // Database (benchmark) operations
    int Read(void *ctx, const std::string &table, const std::string &key,
             const std::vector<std::string> *fields,
             std::vector<KVPair> &result) override;

    int Scan(void *ctx, const std::string &table, const std::string &key, int len,
             const std::vector<std::string> *fields,
             std::vector<std::vector<KVPair>> &result) override;

    int Update(void *ctx, const std::string &table, const std::string &key,
               std::vector<KVPair> &values) override;

    int Insert(void *ctx, const std::string &table, const std::string &key,
               std::vector<KVPair> &values) override;

    int Delete(void *ctx, const std::string &table,
               const std::string &key) override;

private:
    // Filename of the DB, transmitted to server
    const std::string filename;

    // Capability to the sqlite shared memory server
    L4::Cap<sqlite::shm::DbI> server;

    // Dataspace for transmitting database layout information during setup
    L4::Cap<L4Re::Dataspace> db_infopage;
    char *db_infopage_addr = 0;
};

} // namespace ycsbc
