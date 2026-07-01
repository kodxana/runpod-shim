#include <stdio.h>
#include <sys/sysinfo.h>
#include <unistd.h>

int main(void) {
    struct sysinfo info;
    if (sysinfo(&info) != 0) {
        return 2;
    }
    long pages = sysconf(_SC_PHYS_PAGES);
    long avail = sysconf(_SC_AVPHYS_PAGES);
    long page_size = sysconf(_SC_PAGESIZE);
    printf("totalram=%llu freeram=%llu pages=%ld avail=%ld pagesize=%ld\n",
           (unsigned long long)info.totalram * info.mem_unit,
           (unsigned long long)info.freeram * info.mem_unit,
           pages,
           avail,
           page_size);
    return 0;
}
