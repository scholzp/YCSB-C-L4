//
//  basic_db.h
//  YCSB-C
//
//  Created by Jinglei Ren on 12/17/14.
//  Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>.
//  Copyright (c) 2022 Viktor Reusch.
//

#ifndef YCSB_C_BASIC_DB_H_
#define YCSB_C_BASIC_DB_H_

#include "db.h"

#include <iostream>
#include <string>
#include <mutex>
#include "core/properties.h"

using std::cout;
using std::endl;

namespace ycsbc {

class BasicDB : public DB {
 public:
  void *Init() {
    std::lock_guard<std::mutex> lock(mutex_);
    cout << "A new thread begins working." << endl;
    return nullptr;
  }

  int Read(void *, const std::string &table, const std::string &key,
           const std::vector<std::string> *fields,
           std::vector<KVPair> &result) override {
    (void) result;
    std::lock_guard<std::mutex> lock(mutex_);
    cout << "READ " << table << ' ' << key;
    if (fields) {
      cout << " [ ";
      for (auto f : *fields) {
        cout << f << ' ';
      }
      cout << ']' << endl;
    } else {
      cout  << " < all fields >" << endl;
    }
    return 0;
  }

  int Scan(void *, const std::string &table, const std::string &key,
           int len, const std::vector<std::string> *fields,
           std::vector<std::vector<KVPair>> &result) override {
    (void) result;
    std::lock_guard<std::mutex> lock(mutex_);
    cout << "SCAN " << table << ' ' << key << " " << len;
    if (fields) {
      cout << " [ ";
      for (auto f : *fields) {
        cout << f << ' ';
      }
      cout << ']' << endl;
    } else {
      cout  << " < all fields >" << endl;
    }
    return 0;
  }

  int Update(void *, const std::string &table, const std::string &key,
             std::vector<KVPair> &values) override {
    std::lock_guard<std::mutex> lock(mutex_);
    cout << "UPDATE " << table << ' ' << key << " [ ";
    for (auto v : values) {
      cout << v.first << '=' << v.second << ' ';
    }
    cout << ']' << endl;
    return 0;
  }

  int Insert(void *, const std::string &table, const std::string &key,
             std::vector<KVPair> &values) override {
    std::lock_guard<std::mutex> lock(mutex_);
    cout << "INSERT " << table << ' ' << key << " [ ";
    for (auto v : values) {
      cout << v.first << '=' << v.second << ' ';
    }
    cout << ']' << endl;
    return 0;
  }

  int Delete(void *, const std::string &table,
             const std::string &key) override {
    std::lock_guard<std::mutex> lock(mutex_);
    cout << "DELETE " << table << ' ' << key << endl;
    return 0;
  }

 private:
  std::mutex mutex_;
};

} // ycsbc

#endif // YCSB_C_BASIC_DB_H_

