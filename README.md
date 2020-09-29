# concurrent_datastructures
This repository contains my own implementation of thread-safe editions of common data structures.

## concurrent_forward_list
This is a STL-like concurrent singly linked list. Offers the following public interface:  
    `void clear();`      
    `void push_front(const T &val);`   
    `void pop_front();`  
    `bool insert_after(const_iterator pos, const T &val);`  
    `bool erase_after(const_iterator pos);`  
    `bool empty() const noexcept;`  

Notes:    
    - `push_front()` is totally lock-free, and `pop_front()` is partly lock-free as it requires a lock to mark the node as deleted.         Both use CAS on the lock-free part.    
    - `insert_after()` and `erase_after()` require lock on the pos, or both locks on the pos and the deleting node, respectively.  

Current issues:  
    - `clear()` does not mark nodes as deleted but only CAS the pointer to head node as nullptr.
    - Test does not scale beyond 2 threads currently.
