#include <sched.h>

/*
 * Parses string with list of CPU ranges.
 * Use "all" for all CPUs, "none" for none of the CPUs
 */
bool CpusetParse(const char *str, cpu_set_t *set, size_t ncpu);