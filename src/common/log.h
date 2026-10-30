// Logging: log_netlog / log_d3dlog style helpers and the debug log files.
#ifndef LOG_H_INCLUDED
#define LOG_H_INCLUDED

void ypa_log__ypa_general_log();
void ypa_log_out(const char *format, ...);

void init_d3dlog();
void log_d3dlog(const char *format, ...);

void init_dinputlog();



void log_d3d_fail(const char *title, const char *msg, unsigned int errID);


void log_dinputlog(const char *format, ...);
void log_dinput_error(const char *title, const char *msg, unsigned int msgid);

void log_netlog(const char *format, ...);

#endif // LOG_H_INCLUDED
