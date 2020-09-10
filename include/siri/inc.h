#if UINTPTR_MAX == 0xffffffff
#define SIRIDB_IS64BIT 0
#elif UINTPTR_MAX == 0xffffffffffffffff
#define SIRIDB_IS64BIT 1
#else
#define SIRIDB_IS64BIT __WORDSIZE == 64
#endif


#if SIRIDB_IS64BIT
#define SIRIDB_EXPR_ALLOC 0
#else
#define SIRIDB_EXPR_ALLOC CLERI_VERSION_MINOR >= 12
#endif
