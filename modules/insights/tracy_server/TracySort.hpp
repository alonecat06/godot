#ifndef __TRACYSORT_HPP__
#define __TRACYSORT_HPP__

#include "tracy_pdqsort.h"

// Compatibility shim: map ppqsort::sort(execution::par, ...) to tracy::pdqsort(...)
// ppqsort is a parallel sort library not available in the Godot build;
// we fall back to single-threaded pdqsort which has the same algorithmic guarantees.
namespace ppqsort {
    namespace execution {
        struct par_t {};
        constexpr par_t par{};
    }

    template<class Iter, class Compare>
    inline void sort(execution::par_t, Iter begin, Iter end, Compare comp) {
        tracy::pdqsort(begin, end, comp);
    }

    template<class Iter>
    inline void sort(execution::par_t, Iter begin, Iter end) {
        tracy::pdqsort(begin, end);
    }
}

#endif
