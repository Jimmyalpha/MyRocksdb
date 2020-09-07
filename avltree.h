#include <assert.h>
#include <stdlib.h>
#include <atomic>
#include "port/port.h"

namespace ROCKSDB_NAMESPACE {

template<typename Key, class Comparator>
class AVLTree {
 private:
  struct Node;
  Node*  root;

  // Immutable after construction
  Comparator const compare_;
  Node* NewNode(const Key& key);

  void LL(Node *&t);
  void RR(Node *&t);
  void LR(Node *&t);
  void RL(Node *&t);
  inline int height(Node *x){
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
  bool Equal(const Key& a, const Key& b) const { return (compare_(a, b) == 0); }
  bool LessThan(const Key& a, const Key& b) const {return (compare_(a, b) < 0); }
  // Return true if key is greater than the data stored in "n"
  bool KeyIsAfterNode(const Key& key, Node* n) const{  
    return (n != nullptr) && (compare_(n->key, key) < 0);
  }
  // Returns the earliest node with a key >= key.
  // Return nullptr if there is no such node.
  Node* FindGreaterOrEqual(const Key& key) const{ return FindGreaterOrEqual(key, root);}
  Node* FindGreaterOrEqual(const Key& key,const Node* x) const;
  // Return the latest node with a key < key.
  // Return head_ if there is no such node.
  // Fills prev[level] with pointer to previous node at "level" for every
  // level in [0..max_height_-1], if prev is non-null.
  Node* FindLessThan(const Key& key) const;

  // Return the last node in the list.
  // Return head_ if list is empty.
  Node* FindLast() const;

 public:

  explicit AVLTree(Comparator cmp);
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

    // Change the avltree used for this iterator
    // This enables us not changing the iterator without deallocating
    // an old one and then allocating a new one
    void SetList(const AVLTree* tree);

    // Returns true iff the iterator is positioned at a valid node.
    bool Valid() const;

    // Returns the key at the current position.
    // REQUIRES: Valid()
    const Key& key() const;

    // Advance to the first entry with a key >= target
    void Seek(const Key& target);

    // Advances to the next position.
    // REQUIRES: Valid()
    void Next();

    // Advances to the previous position.
    // REQUIRES: Valid()
    void Prev();

    // Retreat to the last entry with a key <= target
    //void SeekForPrev(const Key& target);

    // Position at the first entry in list.
    // Final state of iterator is Valid() iff list is not empty.
    //void SeekToFirst();

    // Position at the last entry in list.
    // Final state of iterator is Valid() iff list is not empty.
    //void SeekToLast();

   private:
    const AVLTree* tree_;
    Node* node_;
    // Intentionally copyable
  };

  
};

// Implementation details follow
template<typename Key, class Comparator>
struct AVLTree<Key, Comparator>::Node {
  explicit Node(const Key& k) : key(k) {
    left_ = nullptr;
  	right_ = nullptr;
	  parent_  =nullptr;
	  height = -1; 
   }

  Key const key;

  //Find next node in order
  Node* Next(){
    if (right_ == nullptr){
      if (parent_->Left() == this)
        return parent_;
      else 
        return right_;
    }
    else {
      Node* x = right_;
      while (x->Left()!=nullptr){
        x = x->Left();
      }
      return x;
    }
  }


  Node* Prev(){
    if (left_ == nullptr){
      if (parent_->Right() == this)
        return parent_;
      else 
        return left_;
    }
    else{
      Node* x = left_;
      while (x->Right()!=nullptr)
        x = x->Right();
      return x;
    }
    
  }
  
  
  
  int Height(){   return height; };
  void SetHeight(int x){  height = x;}
  Node* Parent() {  return parent_;}
  void SetParent(Node* x) { parent_ = x; }
  Node* Left() { return left_; }
  void SetLeft(Node* x) { left_ = x;}
  Node* Right() { return right_; }
  void SetRight(Node* x) { right_ = x;}
  

 private:
  // Array of length equal to the node height.  next_[0] is lowest level link.
  Node* left_;
  Node* right_; 
  Node* parent_;
  int height;
};

template<typename Key, class Comparator>
typename AVLTree<Key, Comparator>::Node*
AVLTree<Key, Comparator>::NewNode(const Key& key) {
  return new Node(key);
}

template<typename Key, class Comparator>
inline AVLTree<Key, Comparator>::Iterator::Iterator(const AVLTree* tree) {
  SetList(tree);
}

template<typename Key, class Comparator>
inline void AVLTree<Key, Comparator>::Iterator::SetList(const AVLTree* tree) {
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
inline void AVLTree<Key, Comparator>::Iterator::Next() {
  assert(Valid());
  node_ = node_->Next();
}

template<typename Key, class Comparator>
inline void AVLTree<Key, Comparator>::Iterator::Prev() {
  // Instead of using explicit "prev" links, we just search for the
  // last node that falls before key.
  assert(Valid());
  node_ = node_->Prev();
  
}

template<typename Key, class Comparator>
inline void AVLTree<Key, Comparator>::Iterator::Seek(const Key& target) {
  node_ = tree_->FindGreaterOrEqual(target);
}


template<typename Key, class Comparator>
typename AVLTree<Key, Comparator>::Node* 
AVLTree<Key, Comparator>::FindGreaterOrEqual(const Key& key,const Node* x) const{
    if (x==nullptr)
      return x;

    if (Equal(key,x->key))
      return x;  
    else if (KeyIsAfterNode(key, x))
      return FindGreaterOrEqual(key, x->Right());

    else{
      Node* prev = x->Prev();
      if (KeyIsAfterNode(key, prev)){
        return x;
      }
      else 
        return FindGreaterOrEqual(key,x->Prev);
      
    } 
}


template<typename Key, class Comparator>
typename AVLTree<Key, Comparator>::Node* 
AVLTree<Key, Comparator>::FindLessThan(const Key& key) const{

}

template<typename Key, class Comparator>
typename AVLTree<Key, Comparator>::Node* 
AVLTree<Key, Comparator>::FindLast() const{
    Node * x = root;
    while (x->Right()!= nullptr){
      x = x->Right();
    }
    return x;
}
/*---------------------------------------Tree Operations-------------------------------------*/
template <typename Key, class Comparator>
void AVLTree<Key, Comparator>::LL(Node* &t){
    //LL rotate
    Node* tmp = t->Left();
    t->SetLeft(tmp->Right());
    tmp->Right()->SetParent(t);

    //change parent after rotate
    tmp->SetRight(t);
    t->SetParent(tmp);

    //Set height of changed nodes
    t->SetHeight(max(height(t->Left()), height(t->Right()))+1);
    tmp->SetHeight(max(height(tmp->Left()), height(tmp->Right()))+1);
    t = tmp;
}

template <typename Key, class Comparator>
void AVLTree<Key,Comparator>::RR(Node* &t){
    //RR rotate
    Node* tmp = t->Right();
    t->SetRight(tmp->Left());
    tmp->Left()->SetParent(t);

    //change parent after rotate
    tmp->SetLeft(t);
    t->SetParent(tmp);

    //Set height of changed nodes
    t->SetHeight(max(height(t->Left()), height(t->Right()))+1);
    tmp->SetHeight(max(height(tmp->Left()), height(tmp->Right()))+1);
    t = tmp;
}

template <typename Key, class Comparator>
void AVLTree<Key,Comparator>::LR(Node* &t){
	  Node* x = t->Left();
    RR(x);
    LL(t);
}

template <typename Key, class Comparator>
void AVLTree<Key,Comparator>::RL(Node* &t){
	  Node* x = t->Right();
    LL(x);
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
AVLTree<Key, Comparator>::AVLTree(const Comparator cmp)
    : compare_(cmp){
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
    if (LessThan(key, cur_node->key))
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

  int hl = height(cur_node->Left());
  int hr = height(cur_node->Right());
  cur_node->SetHeight((hl > hr? hl : hr)+1);
 // cur_node->SetHeight(max(cur_node->Left(), cur_node->Right())+1);
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
    if (LessThan(key, x->key))
      x = x->Left();
    else 
      x = x->Right();
  }
  return false;

}

}  // namespace ROCKSDB_NAMESPACE


