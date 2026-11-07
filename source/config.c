/* config.c -- simple configuration parser
 *
 * Copyright (C) 2021 Andy Nguyen, fgsfds
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <switch.h>

#include "config.h"

#define CONFIG_VARS \
  CONFIG_VAR_INT(screen_width); \
  CONFIG_VAR_INT(screen_height); \
  CONFIG_VAR_INT(autoplay); \
  CONFIG_VAR_INT(log_jni); \
  CONFIG_VAR_INT(log_imports);

Config config;

// actual screen size that is in use right now
int screen_width = 1280;
int screen_height = 720;

static inline void parse_var(const char *name, const char *value) {
  #define CONFIG_VAR_INT(var) if (!strcmp(name, #var)) { config.var = atoi(value); return; }
  CONFIG_VARS
  #undef CONFIG_VAR_INT
}

int read_config(const char *file) {
  char line[1024] = { 0 };

  memset(&config, 0, sizeof(Config));
  config.screen_width = -1;  // auto (720p portatil / 1080p acoplado)
  config.screen_height = -1;
  config.autoplay = 0;
  config.log_jni = 1;        // por ahora ayuda a depurar: poner 0 cuando funcione
  config.log_imports = 1;

  FILE *f = fopen(file, "r");
  if (f == NULL)
    return -1;

  do {
    char *name = NULL, *value = NULL, *tmp = NULL;
    if (fgets(line, sizeof(line), f) != NULL) {
      name = line;
      while (*name && isspace((int)*name)) ++name;
      if (name[0] == '#') continue; // skip comments
      for (tmp = name; *tmp && !isspace((int)*tmp); ++tmp);
      if (*tmp != 0) {
        *tmp = 0;
        for (value = tmp + 1; *value && isspace((int)*value); ++value);
        for (tmp = value + strlen(value) - 1; isspace((int)*tmp); --tmp) *tmp = 0;
        parse_var(name, value);
      }
    }
  } while (!feof(f));

  fclose(f);

  return 0;
}

int write_config(const char *file) {
  FILE *f = fopen(file, "w");
  if (f == NULL)
    return -1;

  #define CONFIG_VAR_INT(var) fprintf(f, "%s %d\n", #var, config.var)
  CONFIG_VARS
  #undef CONFIG_VAR_INT

  fclose(f);

  return 0;
}

static SetLanguage console_language(void) {
  u64 lcode = 0;
  SetLanguage sl = SetLanguage_ENUS;
  if (R_SUCCEEDED(setInitialize())) {
    if (R_SUCCEEDED(setGetSystemLanguage(&lcode)))
      setMakeLanguage(lcode, &sl);
    setExit();
  }
  return sl;
}

// locale tipo Android ("es_ES") que Java pasa a UAFJNILib.init; sigue el idioma de la consola
const char *config_locale(void) {
  switch (console_language()) {
    case SetLanguage_ENGB:  return "en_GB";
    case SetLanguage_FR:    return "fr_FR";
    case SetLanguage_FRCA:  return "fr_CA";
    case SetLanguage_DE:    return "de_DE";
    case SetLanguage_IT:    return "it_IT";
    case SetLanguage_ES:    return "es_ES";
    case SetLanguage_ES419: return "es_MX";
    case SetLanguage_NL:    return "nl_NL";
    case SetLanguage_PT:    return "pt_PT";
    case SetLanguage_PTBR:  return "pt_BR";
    case SetLanguage_RU:    return "ru_RU";
    case SetLanguage_JA:    return "ja_JP";
    case SetLanguage_KO:    return "ko_KR";
    case SetLanguage_ZHCN:
    case SetLanguage_ZHHANS:return "zh_CN";
    case SetLanguage_ZHTW:
    case SetLanguage_ZHHANT:return "zh_TW";
    default:                return "en_US";
  }
}

// codigo de dos letras del idioma de la consola
const char *config_lang_iso2(void) {
  u64 lcode = 0;
  SetLanguage sl = SetLanguage_ENUS;
  if (R_SUCCEEDED(setInitialize())) {
    if (R_SUCCEEDED(setGetSystemLanguage(&lcode)))
      setMakeLanguage(lcode, &sl);
    setExit();
  }
  switch (sl) {
    case SetLanguage_FR:
    case SetLanguage_FRCA:  return "fr";
    case SetLanguage_DE:    return "de";
    case SetLanguage_IT:    return "it";
    case SetLanguage_ES:
    case SetLanguage_ES419: return "es";
    case SetLanguage_NL:    return "nl";
    case SetLanguage_PT:
    case SetLanguage_PTBR:  return "pt";
    case SetLanguage_RU:    return "ru";
    case SetLanguage_JA:    return "ja";
    case SetLanguage_KO:    return "ko";
    case SetLanguage_ZHCN:
    case SetLanguage_ZHHANS:
    case SetLanguage_ZHTW:
    case SetLanguage_ZHHANT:return "zh";
    default:                return "en";
  }
}
