#include "config.h"
#include "defaults.h"
#include "hdd.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define NEWLIB_PORT_AWARE
#include <fileio.h>

Settings settings;

int loadConfig(void);
void initConfig(void);

char configPath[] = "pfs0:" HOSDMBR_CONF_PATH;

// Adds a new string to linkedStr and returns
linkedStr *addStr(linkedStr *lstr, char *str) {
  linkedStr *newLstr = malloc(sizeof(linkedStr));
  newLstr->str = strdup(str);
  newLstr->next = NULL;

  if (lstr) {
    linkedStr *tLstr = lstr;
    // If lstr is not null, go to the last element and
    // link the new element
    while (tLstr->next)
      tLstr = tLstr->next;

    tLstr->next = newLstr;
    return lstr;
  }

  // Else, return the new element as the first one
  return newLstr;
}

// Frees all elements of linkedStr
void freeLinkedStr(linkedStr *lstr) {
  if (!lstr)
    return;

  linkedStr *tPtr = lstr;
  while (lstr->next) {
    free(lstr->str);
    tPtr = lstr->next;
    free(lstr);
    lstr = tPtr;
  }
  free(lstr->str);
  free(lstr);
}

int loadConfig() {
  // Mount the config partition
  mountPFS(HOSD_CONF_PARTITION);

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
        newPath->trigger = btn;
        newPath->paths = NULL;
        newPath->args = NULL;

        // Insert the path into the list
        if (currentPath)
          currentPath->next = newPath;

        currentPath = newPath;
      }
      if (!settings.paths)
        settings.paths = currentPath;

      // Check if the option has an _arg suffix
      if ((optPtr = strrchr(lineBuffer, '_')) && !strncmp(++optPtr, "arg", 3))
        // Add the argument
        currentPath->args = addStr(currentPath->args, valuePtr);
      else
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
  }
  fclose(file);
  umountPFS();
  return 0;
}
