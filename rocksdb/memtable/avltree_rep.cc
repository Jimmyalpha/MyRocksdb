
#ifndef ROCKSDB_LITE
#include "memtable/avltree_rep.h"

#include "db/memtable.h"
#include "memtable/avltree.h"
#include "memory/arena.h"
#include "rocksdb/memtablerep.h"

namespace ROCKSDB_NAMESPACE {
namespace {

class AVLTreeRep : public MemTableRep {
  typedef AVLTree<const char* , const MemTableRep::KeyComparator&> Bucket;
 private:
  Allocator* const allocator_;
  const SliceTransform* transform_;
  const MemTableRep::KeyComparator& compare_;
  AVLTree<const char*, const MemTableRep::KeyComparator&> avltree_;
  mutable port::RWMutex rwlock_;
 public:
  AVLTreeRep(const MemTableRep::KeyComparator& compare,
              Allocator* allocator, const SliceTransform* transform ): 
                  MemTableRep(allocator),
                  allocator_(allocator),
                  transform_(transform),
                  compare_(compare),
                  avltree_(compare) 
            {};
  ~AVLTreeRep() {}
  void Insert(KeyHandle handle) override;

  bool Contains(const char* key) const override;

  void Get(const LookupKey& k, void* callback_args,
           bool (*callback_func)(void* arg, const char* entry)) override;
  size_t ApproximateMemoryUsage() override{
    return sizeof(avltree_) + avltree_.Size();
  }
  Iterator* GetIterator(Arena* arena = nullptr) override{
    return new AVLTreeRep::Iterator(&avltree_);
  }


 private:

  class Iterator : public MemTableRep::Iterator {
    AVLTree<const char*, const MemTableRep::KeyComparator&>::Iterator iter_;
   public:
    explicit Iterator(
           AVLTree<const char*, const MemTableRep::KeyComparator&> *tree )
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
        if (memtable_key != nullptr) {
        iter_.Seek(memtable_key);
      } else {
        iter_.Seek(EncodeKey(&tmp_, internal_key));
      }
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
        iter_.SeekToFirst();
    }

    // Position at the last entry in collection.
    // Final state of iterator is Valid() iff collection is not empty.
    void SeekToLast() override {
        iter_.SeekToLast();
    }
   protected:
    std::string tmp_;       // For passing to EncodeKey

  };

};



void AVLTreeRep::Insert(KeyHandle handle) {
  auto* key = static_cast<char*>(handle);
  assert(!Contains(key));
  WriteLock l(&rwlock_);
  avltree_.Insert(key);
}

bool AVLTreeRep::Contains(const char* key) const {
  ReadLock l(&rwlock_);
  return avltree_.Contains(key);
}


void AVLTreeRep::Get(const LookupKey& k, void* callback_args,
                          bool (*callback_func)(void* arg, const char* entry)) {
   AVLTreeRep::Iterator iter(&avltree_);
   for (iter.Seek(k.user_key(), k.memtable_key().data());
        iter.Valid() && callback_func(callback_args, iter.key()); iter.Next()) {
   }
}




} // anon namespace

MemTableRep* AVLTreeRepFactory::CreateMemTableRep(
    const MemTableRep::KeyComparator& compare, Allocator* allocator,
    const SliceTransform* transform, Logger* /*logger*/) {
  return new AVLTreeRep(compare, allocator, transform);
}

MemTableRepFactory* NewAVLTreeRepFactory() {
  return new AVLTreeRepFactory();
}

}  // namespace ROCKSDB_NAMESPACE
#endif  // ROCKSDB_LITE
