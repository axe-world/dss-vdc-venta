/*
 Author: Alexander Knauer <a-x-e@gmx.net>
 License: Apache 2.0
 */
#ifdef HAVE_CONFIG_H
    #include "config.h"
#endif

#include "venta.h"

#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>
#include <string.h>
#include <pthread.h>
#include <inttypes.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>


static pthread_mutex_t reportMutex;
static int debugLevel = LOG_WARNING;
static void printMessage(char *buf);

static double get_timestamp(void) {
  struct timeval tv;

  gettimeofday(&tv, NULL);
  return (tv.tv_sec + tv.tv_usec*1e-6);
}

void vdc_init_report() {
  pthread_mutex_init(&reportMutex, NULL);
}

void vdc_set_debugLevel(int debug) {
  debugLevel = debug;
}

int vdc_get_debugLevel() {
  return debugLevel;
}

void vdc_report(int errlevel, const char *fmt, ... ) {
  if (errlevel <= debugLevel) {
    char buf[BUFSIZ];
    va_list ap;
    pthread_mutex_lock(&reportMutex);

    strncpy(buf, "venta: ", BUFSIZ);
    va_start(ap, fmt);
    vsnprintf(buf + strlen(buf), sizeof(buf)-strlen(buf), fmt, ap);
    va_end(ap);

    printMessage(buf);

    pthread_mutex_unlock(&reportMutex);
  }
}

void vdc_report_extraLevel(int errlevel, int maxErrlevel, const char *fmt, ... ) {
  if (errlevel <= maxErrlevel) {
    char buf[BUFSIZ];
    va_list ap;
    pthread_mutex_lock(&reportMutex);

    strncpy(buf, "klafs: ", BUFSIZ);
    va_start(ap, fmt);
    vsnprintf(buf + strlen(buf), sizeof(buf)-strlen(buf), fmt, ap);
    va_end(ap);

    printMessage(buf);

    pthread_mutex_unlock(&reportMutex);
  }
}

static void printMessage(char *buf) {
  char buf2[BUFSIZ], *sp, tsbuf[30];
  struct timeval t;
  gettimeofday(&t, NULL);
  strftime(tsbuf, sizeof(tsbuf), "%Y-%m-%d %H:%M:%S", localtime(&t.tv_sec));
  (void)snprintf(buf2, BUFSIZ, "[%s.%03d] ", tsbuf, (int)t.tv_usec / 1000);
  for (sp = buf; *sp != '\0'; sp++)
    if (isprint(*sp) || (isspace(*sp) && (sp[1]=='\0' || sp[2]=='\0')))
      (void)snprintf(buf2+strlen(buf2), 2, "%c", *sp);
    else
      (void)snprintf(buf2+strlen(buf2), 6, "\\x%02x", (unsigned)*sp);
  (void)fputs(buf2, stderr);
}