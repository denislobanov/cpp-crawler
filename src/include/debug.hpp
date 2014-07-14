#if !defined(DEBUG_H)
#define DEBUG_H

#define DEBUG 2

#if !defined(dbg)
    #if defined(DEBUG)
        #define dbg std::cout<<__FILE__<<"("<<__LINE__<<"): "
    #else
        #define dbg 0 && std::cout
    #endif
    #if !defined(dbg_1) && DEBUG > 1
        #define dbg_1 std::cout<<__FILE__<<"("<<__LINE__<<"): "
    #else
        #define dbg_1 0 && std::cout
    #endif
    #if !defined(dbg_2) && DEBUG > 2
        #define dbg_2 std::cout<<__FILE__<<"("<<__LINE__<<"): "
    #else
        #define dbg_2 0 && std::cout
    #endif
#endif

#endif
