//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//

#ifndef ROCKSDB_LITE
#include "memtable/avltree_rep.h"

#include <atomic>

#include "db/memtable.h"
#include "memory/arena.h"
#include "memtable/avltree.h"
#include "port/port.h"
#include "rocksdb/memtablerep.h"
#include "rocksdb/slice.h"
#include "rocksdb/slice_transform.h"
#include "util/murmurhash.h"

namespace ROCKSDB_NAMESPACE {
namespace {

class AVLtreeRep : public MemTableRep {
private:
  typedef AVLTree<const char*, const MemTableRep::KeyComparator&> Bucket;
  Bucket avltree_;
  const MemTableRep::KeyComparator& compare_;
  Allocator* const allocator_;
  const SliceTransform* transform_;
  // Maps slices (which are transformed user keys) to buckets of keys sharing

 public:
  AVLTreeRep(const MemTableRep::KeyComparator& compare,
              Allocator* allocator, const SliceTransform* transform ): 
                  MemTableRep(allocator),
                  transform_(transform),
                  compare_(compare),
                  allocator_(allocator) 
            {
                      avltree_ = new Bucket(compare, allocator);
            };
  ~AVLTreeRep() {}
  void Insert(KeyHandle handle) override;

  bool Contains(const char* key) const override;

  void Get(const LookupKey& k, void* callback_args,
           bool (*callback_func)(void* arg, const char* entry)) override;

 private:

  class Iterator : public MemTableRep::Iterator {
    AVLTree<const MemTableRep::KeyComparator&>::Iterator iter_;
   public:
    explicit Iterator(
      AVLTree<const MemTableRep::KeyComparator&>::Iterator* tree
    )
        : iter_(tree){}

    ~Iterator() override {}

    // Returns true iff the iterator is positioned at a valid node.
    bool Valid() const override { return iter_.Valid(); }

    // Returns the key at the current position.
    // REQUIRES: Valid()
    const char* key() const override {
      assert(Valid());
      return iter_.key();
    }

    // Advances to the next position.
    // REQUIRES: Valid()
    void Next() override {
      assert(Valid());
      iter_.Next();
    }

    // Advances to the previous position.
    // REQUIRES: Valid()
    void Prev() override {
      assert(Valid());
      iter_.Prev();
    }

    // Advance to the first entry with a key >= target
    void Seek(const Slice& internal_key, const char* memtable_key) override {
        const char* encoded_key =
            (memtable_key != nullptr) ?
                memtable_key : EncodeKey(&tmp_, internal_key);
        iter_.Seek(encoded_key);
    }

    // Retreat to the last entry with a key <= target
    void SeekForPrev(const Slice& /*internal_key*/,
                     const char* /*memtable_key*/) override {
      // not supported
      assert(false);
    }

    // Position at the first entry in collection.
    // Final state of iterator is Valid() iff collection is not empty.
    void SeekToFirst() override {
      if (iter_ != nullptr) {
        iter_.SeekToFirst();
      }
    }

    // Position at the last entry in collection.
    // Final state of iterator is Valid() iff collection is not empty.
    void SeekToLast() override {
      if (iter_ != nullptr) {
        iter_.SeekToLast();
      }
    }


  };
};



void AVLTreeRep::Insert(KeyHandle handle) {
  auto* key = static_cast<char*>(handle);
  assert(!Contains(key));
  auto transformed = transform_->Transform(UserKey(key));
  avltree_->Insert(key);
}

bool AVLTreeRep::Contains(const char* key) const {
  auto transformed = transform_->Transform(UserKey(key));
  return avltree_->Contains(key);
}


void AVLTreeRep::Get(const LookupKey& k, void* callback_args,
                          bool (*callback_func)(void* arg, const char* entry)) {
   SkipListRep::Iterator iter(&avltree_);
   for (iter.Seek(k.memtable_key().data());
        iter.Valid() && callback_func(callback_args, iter.key()); iter.Next()) {
   }
}




} // anon namespace

MemTableRep* AVLTreeRepFactory::CreateMemTableRep(
    const MemTableRep::KeyComparator& compare, Allocator* allocator,
    const SliceTransform* transform, Logger* /*logger*/) {
  return new AVLTreeRep(compare, allocator, transform);
}

MemTableRepFactory* AVLTreeRepFactory() {
  return new AVLTreeRepFactory;
}

}  // namespace ROCKSDB_NAMESPACE
#endif  // ROCKSDB_LITE
