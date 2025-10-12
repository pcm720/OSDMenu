#ifndef _GSM_API_H_
#define _GSM_API_H_
#include <stdint.h>

// Compatibility flags
#define EGSM_FLAG_FLD_FP (1 << 0)  // GSM: Field Mode: Force Progressive
#define EGSM_FLAG_FRM_FP1 (1 << 1) // GSM: Frame Mode: Force Progressive (240p)
#define EGSM_FLAG_FRM_FP2 (1 << 2) // GSM: Frame Mode: Force Progressive (line-double)
#define EGSM_FLAG_NO_576P (1 << 3) // GSM: Disable GSM 576p mode
#define EGSM_FLAG_C_1 (1 << 4)     // GSM: Enable FIELD flip type 1
#define EGSM_FLAG_C_2 (1 << 5)     // GSM: Enable FIELD flip type 2
#define EGSM_FLAG_C_3 (1 << 6)     // GSM: Enable FIELD flip type 3

// eGSM argv[0] must be an uint32_t value pointing to an initialized eGSMArguments struct
typedef struct {
  uint32_t flags;
  void *entry;
  void *gp;
  char *bootPath; // If not NULL, eGSM will use LoadExecPS2 instead of ExecPS2.
  int argc;
  char **argv;
} eGSMArguments;

#endif
