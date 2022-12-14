/*
 * LRUCache11 - a templated C++11 based LRU cache class that allows
 * specification of
 * key, value and optionally the map container type (defaults to
 * std::unordered_map)
 * By using the std::unordered_map and a linked list of keys it allows O(1) insert, delete
 * and
 * refresh operations.
 *
 * This is a header-only library and all you need is the LRUCache11.hpp file
 *
 * Github: https://github.com/mohaps/lrucache11
 *
 * This is a follow-up to the LRUCache project -
 * https://github.com/mohaps/lrucache
 *
 * Copyright (c) 2012-22 SAURAV MOHAPATRA <mohaps@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#pragma once
#include <algorithm>
#include <cstdint>
#include <list>
#include <mutex>
#include <stdexcept>
#include <functional>
#include <thread>
#include <unordered_map>

namespace lru11 {

// A noop lockable concept that can be used in place of std::shared_mutex
class NullLock {
 public:
  void lock() {}
  void unlock() {}
  bool try_lock() { return true; }
};

template <typename K, typename V>
struct KeyValuePair {
 public:
  K key;
  V value;

  KeyValuePair(const K& k, const V& v) : key(k), value(v) {}
};

/**
 *	The LRU Cache class templated by
 *		Key - key type
 *		Value - value type
 *		MapType - an associative container like std::unordered_map
 *		LockType - a lock type derived from the Lock class (default:
 *NullLock = no synchronization)
 *
 *	The default NullLock based template is not thread-safe, however passing
 *Lock=std::mutex will make it
 *	thread-safe
 */
template <class Key, class Value, class Lock = NullLock,
          class Map = std::unordered_map<
              Key, typename std::list<KeyValuePair<Key, Value>>::iterator>>
class Cache {
 public:
  typedef KeyValuePair<Key, Value> node_type;
  typedef std::list<KeyValuePair<Key, Value>> list_type;
  typedef Map map_type;
  typedef Lock lock_type;
  using Guard = std::lock_guard<lock_type>;
  /**
   * the maxSize is the soft limit of keys and (maxSize + elasticity) is the
   * hard limit
   * the cache is allowed to grow till (maxSize + elasticity) and is pruned back
   * to maxSize keys
   * set maxSize = 0 for an unbounded cache (but in that case, you're better off
   * using a std::unordered_map
   * directly anyway! :)
   */
  explicit Cache(size_t maxSize = 64, size_t elasticity = 10,
                 std::function<void(list_type)> delcb = {})
      : maxSize_(maxSize), elasticity_(elasticity), delcb_(delcb) {}
  virtual ~Cache() {
    prune(0);
  }
  size_t size() const {
    Guard g(lock_);
    return cache_.size();
  }
  bool empty() const {
    Guard g(lock_);
    return cache_.empty();
  }
  void clear() {
    Guard g(lock_);
    prune(0);
  }
  void insert(const Key& k, const Value& v) {
    Guard g(lock_);
    const auto iter = cache_.find(k);
    if (iter != cache_.end()) {
      iter->second->value = v;
      keys_.splice(keys_.begin(), keys_, iter->second);
      return;
    }

    keys_.emplace_front(k, v);
    cache_[k] = keys_.begin();

    if (cache_.size() > getMaxAllowedSize())
      prune(getMaxSize());
  }
  bool tryGet(const Key& kIn, Value& vOut) {
    Guard g(lock_);
    const auto iter = cache_.find(kIn);
    if (iter == cache_.end()) {
      return false;
    }
    keys_.splice(keys_.begin(), keys_, iter->second);
    vOut = iter->second->value;
    return true;
  }

  bool remove(const Key& k) {
    Guard g(lock_);
    auto iter = cache_.find(k);
    if (iter == cache_.end()) {
      return false;
    }
    keys_.erase(iter->second);
    cache_.erase(iter);
    return true;
  }
  bool contains(const Key& k) const {
    Guard g(lock_);
    return cache_.find(k) != cache_.end();
  }

  size_t getMaxSize() const { return maxSize_; }
  size_t getElasticity() const { return elasticity_; }
  size_t getMaxAllowedSize() const { return maxSize_ + elasticity_; }

 protected:
  size_t prune(size_t tosize) {
    list_type delelems;
    if (cache_.size() > tosize) {
      // Remove exceeding elements
      while (keys_.size() > tosize) {
        delelems.push_back(keys_.back());
        keys_.pop_back();
      }
      if (delcb_)
        delcb_(delelems);
      for (const auto & iter : delelems)
        cache_.erase(iter.key);
      return cache_.size() - tosize;
    }
    return 0;
  }

 private:
  // Disallow copying.
  Cache(const Cache&) = delete;
  Cache& operator=(const Cache&) = delete;

  mutable Lock lock_;
  Map cache_;
  list_type keys_;
  size_t maxSize_;
  size_t elasticity_;
  std::function<void(list_type)> delcb_;
};

}  // namespace LRUCache11
