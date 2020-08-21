//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// Thread safety
// -------------
//
// Writes require external synchronization, most likely a mutex.
// Reads require a guarantee that the SkipList will not be destroyed
// while the read is in progress.  Apart from that, reads progress
// without any internal locking or synchronization.
//
// Invariants:
//
// (1) Allocated nodes are never deleted until the SkipList is
// destroyed.  This is trivially guaranteed by the code since we
// never delete any skip list nodes.
//
// (2) The contents of a Node except for the next/prev pointers are
// immutable after the Node has been linked into the SkipList.
// Only Insert() modifies the list, and it is careful to initialize
// a node and use release-stores to publish the nodes in one or
// more lists.
//
// ... prev vs. next pointer ordering ...
//

#pragma once
#include <assert.h>
#include <stdlib.h>
#include <atomic>
#include "memory/allocator.h"
#include "port/port.h"
#include "util/random.h"

namespace ROCKSDB_NAMESPACE {

template<typename Key, class Comparator>
class AVLTree {
 private:
  struct Node;
  Node*  root;

  // Immutable after construction
  Comparator const compare_;
  Allocator* const allocator_;    // Allocator used for allocations of nodes
  Node* NewNode(const Key& key);

  void LL(Node *&t);
  void RR(Node *&t);
  void LR(Node *&t);
  void RL(Node *&t);
  inline int hight(Node *x){
    if (x==nullptr)
      return 0;
    return x->Height();
  }
  inline int getbf(Node *x){
    if (x==nullptr)
      return 0;
    else
      return height(x->Left())-height(x->Right());
  }
  
  inline int max(Node *a, Node *b) const {
    return a->Height()>b->Height()?a->Height():b->Height();
  }
  bool Equal(const Key& a, const Key& b) const { return (compare_(a, b) == 0); }
  bool LessThan(const Key& a, const Key& b) const {return (compare_(a, b) < 0); }
  // Return true if key is greater than the data stored in "n"
  bool KeyIsAfterNode(const Key& key, Node* n) const{  
    return (n != nullptr) && (compare_(n->key, key) < 0);
  }

 public:
  // Create a new SkipList object that will use "cmp" for comparing keys,
  // and will allocate memory using "*allocator".  Objects allocated in the
  // allocator must remain allocated for the lifetime of the skiplist object.
  explicit AVLTree(Comparator cmp, Allocator* allocator);
  // No copying allowed
  AVLTree(const AVLTree&) = delete;
  void operator=(const AVLTree&) = delete;
  
  // Insert key into the list.
  // REQUIRES: nothing that compares equal to key is currently in the list.
  void Insert(const Key& key);
  //bool InsertWithHint(const char* key, void** hint);
  //bool InsertWithHintConcurrently(const char* key, void** hint);
  // Like Insert, but external synchronization is not required.
  //bool InsertConcurrently(const char* key);

  // Returns true iff an entry that compares equal to key is in the list.
  bool Contains(const Key& key) const;

  // Return estimated number of entries smaller than `key`.
  uint64_t EstimateCount(const Key& key) const{ return EstimateCount(key, root);}
  uint64_t EstimateCount(const Key& key,const Node* x) const;

  // Iteration over the contents of a skip list
  class Iterator {
   public:
    // Initialize an iterator over the specified list.
    // The returned iterator is not valid.
    explicit Iterator(const AVLTree* tree);

    // Change the underlying skiplist used for this iterator
    // This enables us not changing the iterator without deallocating
    // an old one and then allocating a new one
    void SetTree(const AVLTree* tree);

    // Returns true iff the iterator is positioned at a valid node.
    bool Valid() const;

    // Returns the key at the current position.
    // REQUIRES: Valid()
    const Key& key() const;

    // Advance to the first entry with a key >= target
    void Seek(const Key& target);

    // Retreat to the last entry with a key <= target
    void SeekForPrev(const Key& target);

    // Position at the first entry in list.
    // Final state of iterator is Valid() iff list is not empty.
    void SeekToFirst();

    // Position at the last entry in list.
    // Final state of iterator is Valid() iff list is not empty.
    void SeekToLast();

   private:
    const AVLTree* tree_;
    Node* node_;
    // Intentionally copyable
  };

  
};

// Implementation details follow
template<typename Key, class Comparator>
struct AVLTree<Key, Comparator>::Node {
  explicit Node(const Key& k) : key(k) { }

  Key const key;

  int Height(){
    return height.load(std::memory_order_acquire);
  };
  void SetHeight(int x){
      height.store(x,std::memory_order_release);
  }
  Node* Parent() {
    assert(parent_ != nullptr);
    // Use an 'acquire load' so that we observe a fully initialized
    // version of the returned Node.
    return (parent_.load(std::memory_order_acquire));
  }
  void SetParent(Node* x) {
    // Use a 'release store' so that anybody who reads through this
    // pointer observes a fully initialized version of the inserted node.
    parent_.store(x, std::memory_order_release);
  }
  // Accessors/mutators for links.  Wrapped in methods so we can
  // add the appropriate barriers as necessary.
  Node* Left() {
    assert(left_ != nullptr);
    // Use an 'acquire load' so that we observe a fully initialized
    // version of the returned Node.
    return (left_.load(std::memory_order_acquire));
  }

  void SetLeft(Node* x) {
    // Use a 'release store' so that anybody who reads through this
    // pointer observes a fully initialized version of the inserted node.
    left_.store(x, std::memory_order_release);
  }
  Node* Right() {
    assert(right_ != nullptr);
    // Use an 'acquire load' so that we observe a fully initialized
    // version of the returned Node.
    return (right_.load(std::memory_order_acquire));
  }
  void SetRight(Node* x) {
    // Use a 'release store' so that anybody who reads through this
    // pointer observes a fully initialized version of the inserted node.
    right_.store(x, std::memory_order_release);
  }
  // No-barrier variants that can be safely used in a few locations.
  Node* NoBarrier_Left() {
    assert(left_ != nullptr);
    return left_.load(std::memory_order_relaxed);
  }
  void NoBarrier_SetLeft(Node* x) {
    left_.store(x, std::memory_order_relaxed);
  }
  Node* NoBarrier_Right() {
    assert(right_ != nullptr);
    return right_.load(std::memory_order_relaxed);
  }
  void NoBarrier_SetRight(Node* x) {
    right_.store(x, std::memory_order_relaxed);
  }
  int NoBarrier_Height() {
    return right_.load(std::memory_order_relaxed);
  }
  void NoBarrier_SetHeight(int x) {
    height.store(x, std::memory_order_relaxed);
  }
  Node* NoBarrier_Parent() {
    assert(parent_ != nullptr);
    return parent_.load(std::memory_order_relaxed);
  }
  void NoBarrier_SetParent(Node* x) {
    parent_.store(x, std::memory_order_relaxed);
  }

 private:
  // Array of length equal to the node height.  next_[0] is lowest level link.
  std::atomic<Node*> left_;
  std::atomic<Node*> right_; 
  std::atomic<Node*> parent_;
  std::atomic<int> height;
};

template<typename Key, class Comparator>
typename AVLTree<Key, Comparator>::Node*
AVLTree<Key, Comparator>::NewNode(const Key& key) {
  char* mem = allocator_->AllocateAligned(
      sizeof(Node) /*+ sizeof(std::atomic<Node*>)*/);
  return new (mem) Node(key);
}

template<typename Key, class Comparator>
inline AVLTree<Key, Comparator>::Iterator::Iterator(const AVLTree* tree) {
  SetTree(tree);
}

template<typename Key, class Comparator>
inline void AVLTree<Key, Comparator>::Iterator::SetTree(const AVLTree* tree) {
  tree_ = tree;
  node_ = nullptr;
}

template<typename Key, class Comparator>
inline bool AVLTree<Key, Comparator>::Iterator::Valid() const {
  return node_ != nullptr;
}

template<typename Key, class Comparator>
inline const Key& AVLTree<Key, Comparator>::Iterator::key() const {
  assert(Valid());
  return node_->key;
}

template<typename Key, class Comparator>
inline void AVLTree<Key, Comparator>::Iterator::Left() {
  assert(Valid());
  node_ = node_->Left();
}

template<typename Key, class Comparator>
inline void AVLTree<Key, Comparator>::Iterator::Right() {
  // Instead of using explicit "prev" links, we just search for the
  // last node that falls before key.
  assert(Valid());
  node_ = node_->Right();
}

/*---------------------------------------Tree Operations-------------------------------------*/
template <typename Key, class Comparator>
void AVLTree<Key,Comparator>::LL(Node* &t){
    Node* tmp = t->Left();
    t->SetLeft(tmp->Right());
    tmp->Right()->SetParent(t);

    tmp->SetRight(t);
    t->SetParent(tmp);

    t->SetHeight(max(t->Left(), t->Right())+1);
    tmp->SetHeight(max(tmp->Left(), tmp->Right())+1);
    t = tmp;
}

template <typename Key, class Comparator>
void AVLTree<Key,Comparator>::RR(Node* &t){
    Node* tmp = t->Right();
    t->SetRight(tmp->Left());
    tmp->Left()->SetParent(t);


    tmp->SetLeft(t);
    t->SetParent(tmp);

    t->SetHeight(max(t->Left(), t->Right())+1);
    tmp->SetHeight(max(tmp->Left(), tmp->Right())+1);
    t = tmp;
}

template <typename Key, class Comparator>
void AVLTree<Key,Comparator>::LR(Node* &t){
    RR(t->Left());
    LL(t);
}

template <typename Key, class Comparator>
void AVLTree<Key,Comparator>::RL(Node* &t){
    LL(t->Right());
    RR(t);
}

template <typename Key, class Comparator>
uint64_t AVLTree<Key, Comparator>::EstimateCount(const Key& key,const Node* x) const {
  uint64_t count = 0;
  if (x==nullptr)
    return 0;

  if (LessThan(x->key,key)){
    count = count + 1 + EstimateCount(key,x->Left()) + EstimateCount(key,x->Right());
  }
  else{
    count = count + EstimateCount(key,x->Left());
  }
    
  return count;
  
}

template <typename Key, class Comparator>
AVLTree<Key, Comparator>::AVLTree(const Comparator cmp, Allocator* allocator)
    : compare_(cmp),
      allocator_(allocator){
  // Allocate the prev_ Node* array, directly from the passed-in allocator.
  // prev_ does not need to be freed, as its life cycle is tied up with
  // the allocator as a whole.
  root = nullptr;
}

template<typename Key, class Comparator>
void AVLTree<Key, Comparator>::Insert(const Key& key) {
  if (root == nullptr){
     root = NewNode(key);
     return;
  }
  /*Find position to insert*/
  Node* cur_node = root;
  Node* cur_parent = nullptr;
  while (cur_node){
    cur_parent = cur_node;
    if (LessThan(key < cur_node->key))
      cur_node = cur_node->Left();
    else 
      cur_node = cur_node->Right();
  }
  /*Create new node and insert*/
  cur_node = NewNode(key);
  if (LessThan(key,cur_parent->key))
    cur_parent->SetLeft(cur_node);
  else
    cur_parent->SetRight(cur_node);
  cur_node->SetParent(cur_parent);
  cur_node->SetHeight(max(cur_node->Left(), cur_node->Right())+1);
  int bf;
  /*balance*/  
  while (cur_parent!=nullptr){
    bf = getbf(cur_parent);
    if (bf==0)
      return ;
    else if (bf==-1 || bf ==1){
      cur_node = cur_parent;
      cur_parent = cur_node->Parent();
    }
    else {//Not balance
      if (bf==2){// Right subtree is higher
        if (getbf(cur_node)==1)
          LL(cur_parent);
        else
          RL(cur_parent);
      }
      else{
        if (getbf(cur_node)==-1)
          RR(cur_parent);
        else
          LR(cur_parent);
      }
    }
    break;
  }

}

template<typename Key, class Comparator>
bool AVLTree<Key, Comparator>::Contains(const Key& key) const {
  Node *x= root;
  while (x){
    if (Equal(x->key,key))
      return true;
    if (LessThan(key < x->key))
      x = cur_node->Left();
    else 
      x = x->Right();
  }
  return false;

}

}  // namespace ROCKSDB_NAMESPACE


