#ifndef __PERF_H__
#define __PERF_H__

#define PERF_HASH_BITS 10
#define PERF_METHOD_x86
#define INT64 unsigned long long

void perf_enable(void);
void perf_enter(char *section);
void perf_summarize(void);

#endif /*__PERF_H__*/
