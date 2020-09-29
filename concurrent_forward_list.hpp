#pragma once
#include <type_traits> // to be finished
#include <memory>
#include <atomic>
#include <mutex>
#include <stdexcept>

namespace hungbiu {

template<typename T>
class concurrent_forward_list
{
private:
    struct list_node
    {
        typedef std::shared_ptr<list_node>    pointer;
        typedef std::mutex                    mutex_t;
        typedef std::unique_lock<mutex_t>     unique_lock_t;
        typedef std::atomic<bool>             flag_t;

        // Data members 
        T               m_val;
        mutable mutex_t m_mtx;
        mutable flag_t  m_deleted;
        pointer         m_next;

        // Constructor
        list_node() :                   // Default construct. Use case: end() and cend()
            m_deleted(true) {}               
        list_node(const T &val) :       // Specify val. Use case: insert_after()
            m_val(val), m_deleted(false) {}
        list_node(const pointer &p) :   // Specify pointer. Use case: before_begin()
            m_next(p), m_deleted(true) {}
        list_node(const T &val, const pointer &p) : // Specify val and pointer. Use case: push()
            m_val(val), m_deleted(false), m_next(p) {}
        list_node(const list_node &) = delete;
        list_node &operator= (const list_node &) = delete;
        ~list_node() = default;

        // Operations
        // Acquire a RAII lock on the node
        unique_lock_t lock() const noexcept
        {
            return unique_lock_t{ m_mtx };
        }        
        // Test if the node is deleted
        bool is_deleted() const noexcept
        {
            return m_deleted.load(std::memory_order_acquire);
        }
        // Try to mark the node as deleted. Return true only if m_deleted was false.
        bool mark_as_deleted() noexcept
        {
            auto b = false;
            return m_deleted.compare_exchange_strong(b, true);
        }


    };   
private:
    typedef list_node                   node_type;
    typedef typename list_node::pointer pointer;
    typedef typename list_node::mutex_t mutex_t;

public:    
    // Modifying the value of a node using an iterator
    // is NOT thread-safe. Users should not use the same 
    // iterator across different threads.
template<typename Type>
class concurrent_forward_list_iterator 
{
    public:    
        typedef Type                          value_type;
        typedef std::shared_ptr<list_node>    pointer;
        typedef value_type&                   reference;
        typedef const value_type&             const_reference;
        typedef value_type*                   raw_pointer;
        typedef const value_type*             const_raw_pointer;
    private:
        pointer m_node_ptr;
    public:
    // Constructor
    concurrent_forward_list_iterator() noexcept {}    
    concurrent_forward_list_iterator(const pointer &node_ptr) :
        m_node_ptr(node_ptr) {}
    concurrent_forward_list_iterator(pointer &&node_ptr) :
        m_node_ptr(std::move(node_ptr)) {}
    concurrent_forward_list_iterator(const concurrent_forward_list_iterator &oth) :
        m_node_ptr(oth.m_node_ptr) {}    
    template<typename U, 
             typename = std::enable_if_t<std::is_same_v< Type, 
                                                         std::add_const_t<U>> > 
                                        >    
    concurrent_forward_list_iterator(const concurrent_forward_list_iterator<U> &oth) :
        m_node_ptr(oth.m_node_ptr) {}    
    concurrent_forward_list_iterator(concurrent_forward_list_iterator &&oth) noexcept :
        m_node_ptr(std::move(oth.m_node_ptr)) {}
    template<typename U, 
             typename = std::enable_if_t<std::is_same_v< Type, 
                                                         std::add_const_t<U>> > 
                                        >    
    concurrent_forward_list_iterator(concurrent_forward_list_iterator<U> &&oth) :
        m_node_ptr(std::move(oth.m_node_ptr)) {}    
    ~concurrent_forward_list_iterator() = default;

    // Assignment operator
    concurrent_forward_list_iterator& 
    operator= (const concurrent_forward_list_iterator &rhs) {
        if (this != &rhs) {
            m_node_ptr = rhs.m_node_ptr;
        }
        return *this;
    }        
    concurrent_forward_list_iterator& 
    operator= (concurrent_forward_list_iterator &&rhs) noexcept {
        if (this != &rhs) {
            m_node_ptr = move(rhs.m_node_ptr);
        }
        return *this;
    }

    // Relationship operator
    friend bool 
    operator==( const concurrent_forward_list_iterator &lhs, 
                const concurrent_forward_list_iterator &rhs) noexcept {
        return lhs.m_node_ptr == rhs.m_node_ptr;
    }
    friend bool 
    operator!=( const concurrent_forward_list_iterator &lhs, 
                const concurrent_forward_list_iterator &rhs) noexcept {
        return lhs.m_node_ptr != rhs.m_node_ptr;
    }

    // Test if the iterator points to a position that is present in the list
    bool is_valid() const noexcept{
        auto p = std::atomic_load_explicit(&m_node_ptr, std::memory_order_acquire);
        return m_node_ptr && 
               !m_node_ptr->is_deleted();
    }
    explicit operator bool() const noexcept{
        return is_valid();
    }

    // Dereference
    reference operator* () noexcept {
        return m_node_ptr->m_val;
    }
    const_reference operator* () const noexcept {
        return m_node_ptr->m_val;
    }
    raw_pointer operator-> () noexcept {
        return &(m_node_ptr->m_val);
    }    
    const_raw_pointer operator-> () const noexcept {
        return &(m_node_ptr->m_val);
    }
        
    // Advance forward
    // Pre-increment
    concurrent_forward_list_iterator &operator++ () 
    {     
        m_node_ptr = m_node_ptr.get()->m_next;
        return *this;
    }
    // Post-increment    
    concurrent_forward_list_iterator operator++ (int) {
        auto tmp_iter = *this;
        m_node_ptr = m_node_ptr->m_next;
        return tmp_iter;
    }

    friend class concurrent_forward_list;
    friend class concurrent_forward_list<std::remove_cv_t<Type>>;
};
    typedef T                                         value_type;    
    typedef concurrent_forward_list_iterator<T>       iterator;
    typedef concurrent_forward_list_iterator<const T> const_iterator;
    
private:
    pointer m_head;
public:
    // Constructor
    concurrent_forward_list() = default;
    concurrent_forward_list(const concurrent_forward_list &) = delete;
    ~concurrent_forward_list() = default;
    concurrent_forward_list &operator= (const concurrent_forward_list &) = delete;
    concurrent_forward_list &operator= (concurrent_forward_list &&) = delete;

    // Iterators
    iterator begin() noexcept {
        return iterator { 
            std::atomic_load_explicit(&m_head, std::memory_order_acquire) 
        };
    }
    const_iterator cbegin() const noexcept {
        return const_iterator { 
            std::atomic_load_explicit(&m_head, std::memory_order_acquire) 
        };
    }
    iterator end() noexcept {
        return iterator{};
    }
    const_iterator cend() const noexcept {
        return const_iterator{};
    }

    // Modifiers
    // --------------------------------------------------   

    // Release all nodes in the list
    void clear() {
	    // m_head's maintains a chain of nodes
	    // whose lifetimes are control by shared_ptr
	    // releasing the head also release all successors
	    auto tmp = std::atomic_load_explicit(&m_head, std::memory_order_acquire);
	    auto clr = pointer{};
        while (!atomic_compare_exchange_weak( &m_head, 
                                              &tmp,
                                              clr)) ;
        // Empty loop body
    }    
    void push_front(const T &val) {
        auto new_node 
		    = std::make_shared<node_type>( val, 
                                           std::atomic_load_explicit(&m_head,std::memory_order_acquire));        
        while (!atomic_compare_exchange_weak( &m_head, 
                                              &new_node->m_next, 
                                              new_node)) ;
        // Empty loop body
    }
    // Release the first node of the list
    void pop_front() {
        auto old_head = std::atomic_load_explicit( &m_head, 
                                                   std::memory_order_acquire);
        while ( old_head 
                && !atomic_compare_exchange_weak( &m_head, 
                                                  &old_head, 
                                                  old_head->m_next)) ;
        // Empty loop body        

        // Acquire lock on the popped node, mark as deleted
        auto lock = std::move(old_head->lock());
        if (!old_head->mark_as_deleted()) {
            throw std::runtime_error{ "node is already marked as deleted!" };
        }
    }
    // Insert an element after the specified position 
    // Returns a bool indicates if the insertion actually take place
    bool insert_after(const_iterator pos, const T &val) {
        // Allocate a new node
        auto new_node = std::make_shared<node_type>(val);      

        // Acquire lock on position
        auto &p = pos.m_node_ptr; // Does NOT bump the ref count    
        auto lock = std::move(p->lock());

        // Check if the position is still valid
        if (p->is_deleted()) {
            return false;
        }

        // Perform actual insertion
        new_node->m_next = p->m_next;
        p->m_next = new_node;      

        return true;     
    }
    // Erase the element after the specified position
    // Returns a bool indicates if the erasure actually take place
    bool erase_after(const_iterator pos) {   
        auto &pre = pos.m_node_ptr; // Does NOT bump the ref count
         
        // Return false if the pos is not valid or the end of the list
        if ( !pre ||  
             !std::atomic_load_explicit( &(pre->m_next), 
                                         std::memory_order_acquire))
            return false;

        // Acquire lock on position (the predecessor)        
        auto pre_lock  = std::move(pre->lock());

        // Check if both positions are still valid
        if ( pre->is_deleted() || !pre->m_next) {
            return false;
        }        

        // Acquire lock on the node to be deleted and perform actual erasure
        auto del = pre->m_next; // Extend delete node's lifetime, 
                                // otherwise it could vanish once modify pre->m_next
        {                        
            auto del_lock = std::move(del->lock());
            pre->m_next = del->m_next;
            if (!del->mark_as_deleted()) {
                throw std::runtime_error{ "node is already marked as deleted!" };
            }
        }        
        return true;     
    }

    // Capacity
    bool empty() const noexcept {
        return !std::atomic_load_explicit(&m_head, std::memory_order_acquire);
    }
};

}; // end of namespace hungbiu
