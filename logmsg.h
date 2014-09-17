#ifndef LOGMSG_H
#define LOGMSG_H

#include <stdarg.h>

void logmsg(const char *format, ...);
void die(const char *format, ...);

#endif