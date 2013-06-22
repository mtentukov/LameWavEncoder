
#ifndef COMDEF_H

#define COMDEF_H

#ifdef _WIN32

#define COM_INLINE _inline
#define  CUSTOM_ALLOCA _alloca
#define int16_t __int16
#define int32_t __int32
#define int64_t __int64
#define PRId16 "hd"
#define PRId32 "d"
#define PRId64 "ld"

#else

#define COM_INLINE inline
#define  CUSTOM_ALLOCA alloca

#endif

#define COM_INT int

#endif
