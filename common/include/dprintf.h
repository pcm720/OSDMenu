#ifndef _DPRINTF_H_
#define _DPRINTF_H_

#ifdef ENABLE_PRINTF
    #define DPRINTF(x...) printf(x)
#else
    #define DPRINTF(x...)
#endif

#endif

