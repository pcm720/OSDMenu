#ifndef _INIT_H_
#define _INIT_H_

typedef enum {
  Target_Default,
  Target_HDD,
  Target_XFROM,
} TargetDevice;

// Loads IOP modules
int initModules(TargetDevice device);

#endif
