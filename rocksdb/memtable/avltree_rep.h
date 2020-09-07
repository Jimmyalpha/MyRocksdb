#pragma once
#ifndef ROCKSDB_LITE
#include "rocksdb/slice_transform.h"
#include "rocksdb/memtablerep.h"

namespace ROCKSDB_NAMESPACE {

class AVLTreeRepFactory : public MemTableRepFactory {
 public:
  explicit AVLTreeRepFactory(){ }

  virtual ~AVLTreeRepFactory() {}

  using MemTableRepFactory::CreateMemTableRep;
  virtual MemTableRep* CreateMemTableRep(
      const MemTableRep::KeyComparator& compare, Allocator* allocator,
      const SliceTransform* transform, Logger* logger) override;

  virtual const char* Name() const override {
    return "AVLTreeRepFactory";
  }

};

}  // namespace ROCKSDB_NAMESPACE
#endif  // ROCKSDB_LITE
