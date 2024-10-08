//
//  basic_db.cc
//  YCSB-C
//
//  Created by Jinglei Ren on 12/17/14.
//  Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>.
//

#include "db/db_factory.h"

#include <string>
#include "db/basic_db.h"
#include "db/lock_stl_db.h"
#include "sqlite_lib_db.h"
#include "sqlite_ipc_db.h"
#include "sqlite_shm_db.h"

using namespace std;
using ycsbc::DB;
using ycsbc::DBFactory;

DB* DBFactory::CreateDB(utils::Properties &props) {
  if (props["dbname"] == "basic") {
    return new BasicDB;
  }
  else if (props["dbname"] == "lock_stl") {
    return new LockStlDB;
  }
  else if (props["dbname"] == "sqlite_lib") {
    return new SqliteLibDB;
  }
  else if (props["dbname"] == "sqlite_ipc") {
    return new SqliteIpcDB;
  }
  else if (props["dbname"] == "sqlite_shm") {
    return new SqliteShmDB;
  }
  else 
    return NULL;
}

