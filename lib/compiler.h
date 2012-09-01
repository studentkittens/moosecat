#ifndef MC_COMPILER_H
#define MC_COMPILER_H

/*
 * Compiler specific macros.
 * This is stolen from libmpdclient's compiler.h
 */

#if !defined(SPARSE) && defined(__GNUC__) && __GNUC__ >= 3 && !defined(MC_DISABLE_CC_MACROS)

/* GCC 4.x */

#define mc_cc_unused __attribute__((unused))
#define mc_cc_malloc __attribute__((malloc))
#define mc_cc_pure __attribute__((pure))
#define mc_cc_const __attribute__((const))
#define mc_cc_sentinel __attribute__((sentinel))
#define mc_cc_printf(a,b) __attribute__((format(printf, a, b)))
#define mc_cc_hot __attribute__((hot))
#define mc_cc_unused __attribute__((unused))

#else

/* generic C compiler */

#define mc_cc_unused
#define mc_cc_malloc
#define mc_cc_pure
#define mc_cc_const
#define mc_cc_sentinel
#define mc_cc_printf(a,b)
#define mc_cc_hot
#define mc_cc_unused

#endif

#endif /* end of include guard: MC_COMPILER_H */

