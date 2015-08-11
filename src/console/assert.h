#ifndef COMMON_ASSERT_HOO
#define COMMON_ASSERT_HOO


#ifdef DEBUG

void AssertionFailure(const char *file, int line, const char *pred) __attribute__ ((noinline, cold));

#define Assert(p) if (__builtin_expect(!(p), 0)) { AssertionFailure(__FILE__, __LINE__, #p); }

#else

#define Assert(p)

#endif

#endif // COMMON_ASSERT_HOO

