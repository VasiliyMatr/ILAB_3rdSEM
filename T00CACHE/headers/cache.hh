
#include <list>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <string>
#include <cassert>

#ifndef CACHE_HH_INCL
#define CACHE_HH_INCL

#include "page.hh"

namespace caches {

template <typename T, typename T_id>
class Cache2Q
{
    // To store elements + ids.
    typedef typename std::pair<T, T_id> TnId;
    // To store ptr to elements + ids.
    typedef typename std::pair<T*, T_id> TPnId;

    // Type for iterator for list with elements.
    typedef typename std::list<TnId>::iterator tListIt;
    // Type for iterator for list with elements ptrs.
    typedef typename std::list<TPnId>::iterator tPListIt;

    // AM & ALin stores elements.
    // ALout stores only ptrs to elements, stored in memory.

    // Stores elements that are probably hot.
    std::list<TnId> AM_List_;
    std::unordered_map<T_id, tListIt> AM_HashTable_;

    // Stores once accesed elements. Managed as FIFO.
    std::list<TnId> ALin_List_; 
    std::unordered_map<T_id, tListIt> ALin_HashTable_;

    // Stores ptrs to way back accessed elements. Managed as FIFO.
    std::list<TPnId> ALout_List_;
    std::unordered_map<T_id, tPListIt> ALout_HashTable_;

    // Max lists sizes.
    const size_t AM_Size_ = 0;
    const size_t ALin_Size_ = 0;
    const size_t ALout_Size_ = 0;

    // Numbers of currently cached elements.
    size_t AM_CachedElemNum_ = 0;
    size_t ALin_CachedElemNum_ = 0;
    size_t ALout_CachedElemNum_ = 0;

    // Counter for number of hits in tests.
    size_t hitsCount_ = 0;

    // Loads uncached element to ALin.
    T load2Cache( T_id );
    // Counts hits for given cache size & elems ids sequence.
    static size_t countHits( size_t elemsNum, T_id elemsIds[] );

public:
    // cacheSize = number of elements that can be cached in memory.
    Cache2Q( size_t cacheSize );
    // Searches element by it's id. Caches frequiently accessed elements.
    T getPage( T_id );

    // Efficiency tests stuff
    static void test( char * filename );
    static size_t test();
    void resetHitsCount() { hitsCount_ = 0; }
    size_t getHitsCount() { return hitsCount_; }
};

// ?safe?
constexpr double AM_QUOTA = 0.25;
constexpr double ALIN_QUOTA = 0.25;

#if AM_QUOTA + ALIN_QUOTA > 1
#error AM_QUOTA + ALIN_QUOTA > 1.0
#endif

template <typename T, typename T_id>
Cache2Q<T, T_id>::Cache2Q( size_t cacheSize ) :
    AM_Size_ (AM_QUOTA * cacheSize),
    ALin_Size_ (ALIN_QUOTA * cacheSize),
    ALout_Size_ (cacheSize - AM_Size_ - ALin_Size_) // ? order ?
{}

template <typename T, typename T_id>
T Cache2Q<T, T_id>::getPage( T_id elem_id )
{
    auto amHIt = AM_HashTable_.find (elem_id);
    // move elem to the head of AM
    if (amHIt != AM_HashTable_.end ())
    {
        ++hitsCount_;

        auto amIt = amHIt->second;

        AM_List_.splice (AM_List_.begin (), AM_List_, amIt);

        return AM_List_.front ().first;
    }

    auto aloutHIt = ALout_HashTable_.find (elem_id);
    // move element form memory to element slot
    if (aloutHIt != ALout_HashTable_.end ())
    {
        ++hitsCount_;

        auto aloutIt = aloutHIt->second;

        if (AM_Size_ <= AM_CachedElemNum_)
        {
            AM_HashTable_.erase (AM_List_.back ().second);
            AM_List_.pop_back ();
            --AM_CachedElemNum_;
        }

        AM_List_.emplace_front (TnId (*(aloutIt->first), elem_id));
        AM_HashTable_[elem_id] = AM_List_.begin ();
        ++AM_CachedElemNum_;

        return AM_List_.front ().first;
    }

    auto alinHIt = ALin_HashTable_.find (elem_id);
    // do nothing
    if (alinHIt != ALin_HashTable_.end ())
    {
        ++hitsCount_;

        return alinHIt->second->first;
    }

    // elem isn't cached => load elem to the head of ALin
    return load2Cache (elem_id);
}

template <typename T, typename T_id>
T Cache2Q<T, T_id>::load2Cache( T_id elem_id )
{
    T elem = slowGetDataFunc (elem_id);

    // need to free space in ALin
    if (ALin_Size_ <= ALin_CachedElemNum_)
    {
        // need to free space in ALout
        if (ALout_Size_ <= ALout_CachedElemNum_)
        {
            auto aloutBack = ALout_List_.back ();
            ALout_HashTable_.erase (aloutBack.second);
            delete aloutBack.first;
            ALout_List_.pop_back ();
            --ALout_CachedElemNum_;
        }

        auto alinBack = ALin_List_.back ();
        ALin_HashTable_.erase (alinBack.second);
        ALin_List_.pop_back ();
        --ALin_CachedElemNum_;

        T * aloutElemPtr = new T;
        *aloutElemPtr = alinBack.first;
        ALout_List_.emplace_front (TPnId (aloutElemPtr, alinBack.second));
        ALout_HashTable_[alinBack.second] = ALout_List_.begin ();
        ++ALout_CachedElemNum_;
    }

    // placing cached element in ALin
    ALin_List_.emplace_front (TnId (elem, elem_id));
    ALin_HashTable_[elem_id] = ALin_List_.begin ();
    ++ALin_CachedElemNum_;

    return elem;
}

template <typename T, typename T_id>
void Cache2Q<T, T_id>::test( char * filename )
{
    assert (filename != nullptr);

    std::ifstream in(filename);
    if (!in.is_open ())
    {
        std::cout << "Cannot open file " << filename <<std::endl;
        return;
    }
    std::streambuf * cinbuf = std::cin.rdbuf ();
    std::cin.rdbuf (in.rdbuf ());

    size_t hitsNum = test ();

    std::cin.rdbuf (cinbuf);

    std::cout << "Testing with input file " << '\"' << filename << '\"' << std::endl;
    std::cout << "Hits number: " << hitsNum << std::endl;
}

template <typename T, typename T_id>
size_t Cache2Q<T, T_id>::test()
{
    size_t cacheSize = 0;
    size_t inputSize = 0;

    std::cin >> cacheSize >> inputSize;

    pageId_t * pIds = new pageId_t[inputSize];
    for (size_t i = 0; i < inputSize; ++i)
        std::cin >> pIds[i];

    Cache2Q<Page, pageId_t> cache { cacheSize };
    cache.resetHitsCount ();

    for (size_t i = 0; i < inputSize; ++i)
        cache.getPage (pIds [i]);

    delete pIds;

    return cache.getHitsCount ();
}

} // namespace caches

#endif // #ifndef CACHE_HH_INCL