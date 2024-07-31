//
//  lock_stl_hashtable.h
//
//  Created by Jinglei Ren on 12/22/14.
//  Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>
//

#ifndef YCSB_C_LIB_LOCK_STL_HASHTABLE_FUTEX_H_
#define YCSB_C_LIB_LOCK_STL_HASHTABLE_FUTEX_H_

#include "lib/stl_hashtable.h"
#include "lib/mutex_wrapper.h"

#include <vector>

namespace vmp {

template<class V>
class LockStlHashtableFutex : public StlHashtable<V> {
 public:
  typedef typename StringHashtable<V>::KVPair KVPair;

  V Get(const char *key) const; ///< Returns NULL if the key is not found
  bool Insert(const char *key, V value);
  V Update(const char *key, V value);
  V Remove(const char *key);
  std::vector<KVPair> Entries(const char *key = NULL, size_t n = -1) const;
  std::size_t Size() const;

 private:
  mutable wrapped_mutex mutex_;
};

template<class V>
inline V LockStlHashtableFutex<V>::Get(const char *key) const {
  std::lock_guard<wrapped_mutex> lock(mutex_);
  return StlHashtable<V>::Get(key);
}

template<class V>
inline bool LockStlHashtableFutex<V>::Insert(const char *key, V value) {
  std::lock_guard<wrapped_mutex> lock(mutex_);
  return StlHashtable<V>::Insert(key, value);
}

template<class V>
inline V LockStlHashtableFutex<V>::Update(const char *key, V value) {
  std::lock_guard<wrapped_mutex> lock(mutex_);
  return StlHashtable<V>::Update(key, value);
}

template<class V>
inline V LockStlHashtableFutex<V>::Remove(const char *key) {
  std::lock_guard<wrapped_mutex> lock(mutex_);
  return StlHashtable<V>::Remove(key);
}

template<class V>
inline std::size_t LockStlHashtableFutex<V>::Size() const {
  std::lock_guard<wrapped_mutex> lock(mutex_);
  return StlHashtable<V>::Size();
}

template<class V>
inline std::vector<typename LockStlHashtableFutex<V>::KVPair>
LockStlHashtableFutex<V>::Entries(const char *key, size_t n) const {
  std::lock_guard<wrapped_mutex> lock(mutex_);
  return StlHashtable<V>::Entries(key, n);
}

} // vmp

#endif // YCSB_C_LIB_LOCK_STL_HASHTABLE_H_

