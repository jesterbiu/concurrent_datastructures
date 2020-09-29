#include "concurrent_forward_list.hpp"
#include <thread>
#include <stdio.h>
#include <numeric>
#include <atomic>
#include <cassert>

#define BUGGY_PART 1

using namespace hungbiu;

template<typename list_type>
size_t list_size(const list_type &lst) {
    size_t sz = 0;
    for ( auto i = lst.cbegin(); 
          i != lst.cend(); 
          ++i, ++sz) ;
    return sz;
}

std::atomic<int> PopTimes{ 20 }, PushTimes{ 10 };
std::atomic<int> InsertTimes{ 30 }, EraseTimes{ 20 };

int main()
{
    concurrent_forward_list<int> cflist{};
    
    // push_front
    constexpr auto Max = 100;
    constexpr auto Sum = (1 + Max) * Max / 2;
    for (auto i = 1; i <= Max; ++i) {
        cflist.push_front(i);
    }
    assert(std::accumulate(cflist.cbegin(), cflist.cend(), 0) == Sum);
    for (auto it = cflist.cbegin(); it != cflist.cend(); ++it) {
        assert(it.is_valid());
    }
    printf("push_front: pass\n");
    
    // pop_front
    cflist.pop_front();
    assert(*cflist.cbegin() == 99);
    cflist.push_front(100);
    printf("pop_front: pass\n");

    // simultaneous push_front() and pop_front()
    size_t ElementsLeftCount = Max - PopTimes.load() + PushTimes.load();
    auto pop = [](concurrent_forward_list<int> *plst) {
        while (PopTimes.load(std::memory_order_acquire)) {
            plst->pop_front();
            PopTimes.fetch_sub(1, std::memory_order_acq_rel);
        }
    };
    auto push = [](concurrent_forward_list<int> *plst) {
        auto i = 0;
        while ( (i = PushTimes.load(std::memory_order_acquire)) ) {
            plst->push_front(i);
            PushTimes.fetch_sub(1, std::memory_order_acq_rel);
        }
    };
    std::thread t1{ pop, &cflist };
    std::thread t2{ push, &cflist };
    t1.join();
    t2.join();
    assert(list_size(cflist) == ElementsLeftCount);
    printf("simultaneous push_front() and pop_front(): pass\n");

    // insert_after()
    cflist.clear();
    cflist.push_front(0);
    auto beg = cflist.cbegin();
    for (auto i = 1; i < 100; ++i, ++beg) {
        if (!cflist.insert_after(beg, i)) {
            printf("insert_after() failed at: %d\n", i);
            std::terminate();
        }        
    }
    beg = cflist.cbegin();
     for (auto i = 0; i < 100; ++i) {
        assert(*beg++ == i);
    }
    assert(list_size(cflist) == 100);
    printf("insert_after(): pass\n");

    // erase_after()
    beg = cflist.cbegin();
    for (auto i = 1; i < 100; ++i) {
        if (!cflist.erase_after(beg)) {
            printf("erase_after() failed at: %d\n", i);
            std::terminate();
        }
    }
    assert(*beg == 0);
    assert(list_size(cflist) == 1);
    printf("erase_after(): pass\n");

    // simultaneous insert_after() and erase_after()   
     for (auto i = 1; i < 100; ++i, ++beg) {
        cflist.insert_after(beg, i);
    }            
    InsertTimes.store(30);
    EraseTimes.store(20);
    ElementsLeftCount = list_size(cflist) 
                        + InsertTimes.load()
                        - EraseTimes.load();
	auto eraser = [](concurrent_forward_list<int> *plst) {
		while (EraseTimes.load(std::memory_order_acquire)) {
			if (plst->erase_after(plst->cbegin())) {
                printf( "erase: %d\n", 
				    EraseTimes.fetch_sub(1, std::memory_order_acq_rel));
            }					
		}
	};
	auto insertor = [](concurrent_forward_list<int> *plst) {		
		auto i = 0;
		while ((i = InsertTimes.load(std::memory_order_acquire))) {
            auto after_head = ++plst->cbegin();
			if (plst->insert_after(after_head, i)) {
                printf( "insert: %d\n", 
			        InsertTimes.fetch_sub(1, std::memory_order_acq_rel));
            }			
		}
	};		
	std::thread t3{ eraser, &cflist };
	std::thread t4{ insertor, &cflist };
	t3.join();
	t4.join();
    auto actual_sz = list_size(cflist);
    printf("actual: %lu\nexpected: %lu\n", actual_sz, ElementsLeftCount);
    assert(actual_sz == ElementsLeftCount);
    printf("simultaneous insert_after() and erase_after(): pass\n");
}
