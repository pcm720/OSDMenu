#include "config.h"
#include "common.h"
#include "defaults.h"
#include "dprintf.h"
#include "hdd.h"
#include <ctype.h>
#include <errno.h>
#include <osd_config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define NEWLIB_PORT_AWARE
#include <fileio.h>

Settings settings;

int loadConfig(void);
void initConfig(void);

char configPath[] = "pfs0:" HOSDMBR_CONF_PATH;

int loadConfig() {
  // Init default values
  settings.osdLanguage = 1;
  settings.osdScreenType = -1;

  // Mount the config partition
  if (mountPFS(HOSD_CONF_PARTITION))
    return -ENODEV;

  // Open the config file
  FILE *file = fopen(configPath, "rb");
  if (!file) {
    umountPFS();
    return -ENOENT;
  }

  launchPath *currentPath = NULL;
  char lineBuffer[PATH_MAX] = {0};
  char *valuePtr = NULL;
  char *optPtr = NULL;
  while (fgets(lineBuffer, sizeof(lineBuffer), file)) { // fgets returns NULL if EOF or an error occurs
    // Find the start of the value
    valuePtr = strchr(lineBuffer, '=');
    if (!valuePtr)
      continue;
    *valuePtr = '\0';

    // Trim whitespace and terminate the value
    do {
      valuePtr++;
    } while (isspace((int)*valuePtr));
    valuePtr[strcspn(valuePtr, "\r\n")] = '\0';

    if (strlen(valuePtr) == 0)
      continue;

    if (!strncmp(lineBuffer, "boot_", 5)) {
      // Handle the path option

      // Get the pointer to _<key>
      optPtr = strchr(lineBuffer, '_');
      if (!optPtr)
        continue;
      optPtr++;

      TriggerType btn;
      if (!strncmp(optPtr, "auto", 4))
        btn = TRIGGER_TYPE_AUTO;
      else if (!strncmp(optPtr, "start", 5))
        btn = TRIGGER_TYPE_START;
      else if (!strncmp(optPtr, "triangle", 8))
        btn = TRIGGER_TYPE_TRIANGLE;
      else if (!strncmp(optPtr, "circle", 6))
        btn = TRIGGER_TYPE_CIRCLE;
      else if (!strncmp(optPtr, "cross", 5))
        btn = TRIGGER_TYPE_CROSS;
      else if (!strncmp(optPtr, "square", 6))
        btn = TRIGGER_TYPE_SQUARE;
      else
        continue;

      // Search for the existing option associated with the button
      currentPath = settings.paths;
      if (currentPath)
        while (currentPath->next) {
          if (currentPath->trigger == btn)
            break;

          currentPath = currentPath->next;
        }

      if (!currentPath || currentPath->trigger != btn) {
        // If it doesn't exist, initialize the path
        launchPath *newPath = malloc(sizeof(launchPath));
        if (!newPath)
          bootFail("Failed to allocate memory for a path");

        newPath->trigger = btn;
        newPath->paths = NULL;
        newPath->args = NULL;
        newPath->argCount = 0;
        newPath->next = NULL;

        // Insert the path into the list
        if (currentPath)
          currentPath->next = newPath;

        currentPath = newPath;
      }
      if (!settings.paths)
        settings.paths = currentPath;

      // Check if the option has an _arg suffix
      if ((optPtr = strrchr(lineBuffer, '_')) && !strncmp(++optPtr, "arg", 3)) {
        // Add the argument
        currentPath->args = addStr(currentPath->args, valuePtr);
        currentPath->argCount++;
      } else
        // Add the path
        currentPath->paths = addStr(currentPath->paths, valuePtr);

      continue;
    }

    // Handle flags
    if (!strncmp(lineBuffer, "cdrom_skip_ps2logo", 18)) {
      if (atoi(valuePtr))
        settings.flags |= FLAG_SKIP_PS2_LOGO;
      else
        settings.flags &= ~(FLAG_SKIP_PS2_LOGO);
      continue;
    }
    if (!strncmp(lineBuffer, "cdrom_disable_gameid", 20)) {
      if (atoi(valuePtr))
        settings.flags |= FLAG_DISABLE_GAMEID;
      else
        settings.flags &= ~(FLAG_DISABLE_GAMEID);
      continue;
    }
    if (!strncmp(lineBuffer, "cdrom_use_dkwdrv", 16)) {
      if (atoi(valuePtr))
        settings.flags |= FLAG_USE_DKWDRV;
      else
        settings.flags &= ~(FLAG_USE_DKWDRV);
      continue;
    }
    if (!strncmp(lineBuffer, "ps1drv_enable_fast", 18)) {
      if (atoi(valuePtr))
        settings.flags |= FLAG_PS1DRV_FAST;
      else
        settings.flags &= ~(FLAG_PS1DRV_FAST);
      continue;
    }
    if (!strncmp(lineBuffer, "ps1drv_enable_smooth", 20)) {
      if (atoi(valuePtr))
        settings.flags |= FLAG_PS1DRV_SMOOTH;
      else
        settings.flags &= ~(FLAG_PS1DRV_SMOOTH);
      continue;
    }
    if (!strncmp(lineBuffer, "ps1drv_use_ps1vn", 16)) {
      if (atoi(valuePtr))
        settings.flags |= FLAG_PS1DRV_USE_VN;
      else
        settings.flags &= ~(FLAG_PS1DRV_USE_VN);
      continue;
    }
    if (!strncmp(lineBuffer, "prefer_bbn", 10)) {
      if (atoi(valuePtr))
        settings.flags |= FLAG_PREFER_BBN;
      else
        settings.flags &= ~(FLAG_PREFER_BBN);
      continue;
    }
    if (!strncmp(lineBuffer, "app_gameid", 10)) {
      if (atoi(valuePtr))
        settings.flags |= FLAG_APP_GAMEID;
      else
        settings.flags &= ~(FLAG_APP_GAMEID);
      continue;
    }
    if (!strncmp(lineBuffer, "osd_screentype", 14)) {
      if (!strcmp(valuePtr, "4:3"))
        settings.osdScreenType = TV_SCREEN_43;
      else if (!strcmp(valuePtr, "16:9"))
        settings.osdScreenType = TV_SCREEN_169;
      else if (!strcmp(valuePtr, "full"))
        settings.osdScreenType = TV_SCREEN_FULL;
      continue;
    }
    if (!strncmp(lineBuffer, "osd_language", 12)) {
      if (!strcmp(valuePtr, "jap"))
        settings.osdLanguage = LANGUAGE_JAPANESE;
      else if (!strcmp(valuePtr, "eng"))
        settings.osdLanguage = LANGUAGE_ENGLISH;
      else if (!strcmp(valuePtr, "fre"))
        settings.osdLanguage = LANGUAGE_FRENCH;
      else if (!strcmp(valuePtr, "spa"))
        settings.osdLanguage = LANGUAGE_SPANISH;
      else if (!strcmp(valuePtr, "ger"))
        settings.osdLanguage = LANGUAGE_GERMAN;
      else if (!strcmp(valuePtr, "ita"))
        settings.osdLanguage = LANGUAGE_ITALIAN;
      else if (!strcmp(valuePtr, "dut"))
        settings.osdLanguage = LANGUAGE_DUTCH;
      else if (!strcmp(valuePtr, "por"))
        settings.osdLanguage = LANGUAGE_PORTUGUESE;
      else if (!strcmp(valuePtr, "rus"))
        settings.osdLanguage = LANGUAGE_RUSSIAN;
      else if (!strcmp(valuePtr, "kor"))
        settings.osdLanguage = LANGUAGE_KOREAN;
      else if (!strcmp(valuePtr, "tch"))
        settings.osdLanguage = LANGUAGE_TRAD_CHINESE;
      else if (!strcmp(valuePtr, "sch"))
        settings.osdLanguage = LANGUAGE_SIMPL_CHINESE;
      continue;
    }
  }
  fclose(file);
  umountPFS();
  return 0;
}

// Attempts to load per-title GSM argument from the GSM_CONF_PATH/HOSDGSM_CONF_PATH depending on the device hint
char *getOSDGSMArgument(char *titleID) {
  if (!titleID)
    return;

  DPRINTF("Trying to load the eGSM config file\n");
  if (mountPFS(HOSD_CONF_PARTITION))
    return NULL;

  FILE *gsmConf = fopen("pfs0:" HOSDGSM_CONF_PATH, "r");
  if (!gsmConf)
    return NULL;

  char *defaultArg = NULL;
  char *titleArg = NULL;
  char lineBuffer[30] = {0};
  char *valuePtr = NULL;
  while (fgets(lineBuffer, sizeof(lineBuffer), gsmConf)) { // fgets returns NULL if EOF or an error occurs
    // Find the start of the value
    valuePtr = strchr(lineBuffer, '=');
    if (!valuePtr)
      continue;
    *valuePtr = '\0';

    // Trim whitespace and terminate the value
    do {
      valuePtr++;
    } while (isspace((int)*valuePtr));
    valuePtr[strcspn(valuePtr, "\r\n")] = '\0';

    if (!strncmp(lineBuffer, titleID, 11)) {
      DPRINTF("eGSM will use the title-specific config\n");
      titleArg = strdup(valuePtr);
      break;
    }

    if (!strncmp(lineBuffer, "default", 7))
      defaultArg = strdup(valuePtr);
  }

  fclose(gsmConf);
  umountPFS();

  if (titleArg) {
    // If there's a title-specific argument free the defaultArg and set the defaultArg to titleArg
    if (defaultArg)
      free(defaultArg);

    defaultArg = titleArg;
  }

  return defaultArg;
}
