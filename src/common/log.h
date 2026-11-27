// Logging: game, graphics and network log files (Data/env) mirrored to crash diagnostics.
#ifndef LOG_H_INCLUDED
#define LOG_H_INCLUDED

void ypa_log__ypa_general_log();
void ypa_log_out(const char *format, ...);

void init_d3dlog();
void log_d3dlog(const char *format, ...);

void log_netlog(const char *format, ...);

#endif // LOG_H_INCLUDED
