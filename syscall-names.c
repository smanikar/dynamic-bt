const char* const _syscall_names[] = {
  "restart_syscall",
  "exit",
  "fork",
  "read",
  "write",
  "open",
  "close",
  "waitpid",
  "creat",
  "link",
  "unlink",
  "execve",
  "chdir",
  "time",
  "mknod",
  "chmod",
  "lchown",
  "break",
  "oldstat",
  "lseek",
  "getpid",
  "mount",
  "umount",
  "setuid",
  "getuid",
  "stime",
  "ptrace",
  "alarm",
  "oldfstat",
  "pause",
  "utime",
  "stty",
  "gtty",
  "access",
  "nice",
  "ftime",
  "sync",
  "kill",
  "rename",
  "mkdir",
  "rmdir",
  "dup",
  "pipe",
  "times",
  "prof",
  "brk",
  "setgid",
  "getgid",
  "signal",
  "geteuid",
  "getegid",
  "acct",
  "umount2",
  "lock",
  "ioctl",
  "fcntl",
  "mpx",
  "setpgid",
  "ulimit",
  "oldolduname",
  "umask",
  "chroot",
  "ustat",
  "dup2",
  "getppid",
  "getpgrp",
  "setsid",
  "sigaction",
  "sgetmask",
  "ssetmask",
  "setreuid",
  "setregid",
  "sigsuspend",
  "sigpending",
  "sethostname",
  "setrlimit",
  "getrlimit",
  "getrusage",
  "gettimeofday",
  "settimeofday",
  "getgroups",
  "setgroups",
  "select",
  "symlink",
  "oldlstat",
  "readlink",
  "uselib",
  "swapon",
  "reboot",
  "readdir",
  "mmap",
  "munmap",
  "truncate",
  "ftruncate",
  "fchmod",
  "fchown",
  "getpriority",
  "setpriority",
  "profil",
  "statfs",
  "fstatfs",
  "ioperm",
  "socketcall",
  "syslog",
  "setitimer",
  "getitimer",
  "stat",
  "lstat",
  "fstat",
  "olduname",
  "iopl",
  "vhangup",
  "idle",
  "vm86old",
  "wait4",
  "swapoff",
  "sysinfo",
  "ipc",
  "fsync",
  "sigreturn",
  "clone",
  "setdomainname",
  "uname",
  "modify_ldt",
  "adjtimex",
  "mprotect",
  "sigprocmask",
  "create_module",
  "init_module",
  "delete_module",
  "get_kernel_syms",
  "quotactl",
  "getpgid",
  "fchdir",
  "bdflush",
  "sysfs",
  "personality",
  "afs_syscall",
  "setfsuid",
  "setfsgid",
  "_llseek",
  "getdents",
  "_newselect",
  "flock",
  "msync",
  "readv",
  "writev",
  "getsid",
  "fdatasync",
  "_sysctl",
  "mlock",
  "munlock",
  "mlockall",
  "munlockall",
  "sched_setparam",
  "sched_getparam",
  "sched_setscheduler",
  "sched_getscheduler",
  "sched_yield",
  "sched_get_priority_max",
  "sched_get_priority_min",
  "sched_rr_get_interval",
  "nanosleep",
  "mremap",
  "setresuid",
  "getresuid",
  "vm86",
  "query_module",
  "poll",
  "nfsservctl",
  "setresgid",
  "getresgid",
  "prctl",
  "rt_sigreturn",
  "rt_sigaction",
  "rt_sigprocmask",
  "rt_sigpending",
  "rt_sigtimedwait",
  "rt_sigqueueinfo",
  "rt_sigsuspend",
  "pread64",
  "pwrite64",
  "chown",
  "getcwd",
  "capget",
  "capset",
  "sigaltstack",
  "sendfile",
  "getpmsg",
  "putpmsg",
  "vfork",
  "ugetrlimit",
  "mmap2",
  "truncate64",
  "ftruncate64",
  "stat64",
  "lstat64",
  "fstat64",
  "lchown32",
  "getuid32",
  "getgid32",
  "geteuid32",
  "getegid32",
  "setreuid32",
  "setregid32",
  "getgroups32",
  "setgroups32",
  "fchown32",
  "setresuid32",
  "getresuid32",
  "setresgid32",
  "getresgid32",
  "chown32",
  "setuid32",
  "setgid32",
  "setfsuid32",
  "setfsgid32",
  "pivot_root",
  "mincore",
  "madvise",
  "madvise1",
  "getdents64",
  "fcntl64",
  "223",
  "gettid",
  "readahead",
  "setxattr",
  "lsetxattr",
  "fsetxattr",
  "getxattr",
  "lgetxattr",
  "fgetxattr",
  "listxattr",
  "llistxattr",
  "flistxattr",
  "removexattr",
  "lremovexattr",
  "fremovexattr",
  "tkill",
  "sendfile64",
  "futex",
  "sched_setaffinity",
  "sched_getaffinity",
  "set_thread_area",
  "get_thread_area",
  "io_setup",
  "io_destroy",
  "io_getevents",
  "io_submit",
  "io_cancel",
  "fadvise64",
  "exit_group",
  "lookup_dcookie",
  "epoll_create",
  "epoll_ctl",
  "epoll_wait",
  "remap_file_pages",
  "set_tid_address",
  "timer_create",
  "timer_settime",
  "timer_gettime",
  "timer_getoverrun",
  "timer_delete",
  "clock_settime",
  "clock_gettime",
  "clock_getres",
  "clock_nanosleep",
  "statfs64",
  "fstatfs64",
  "tgkill",
  "utimes",
  "fadvise64_64",
  "vserver",
  "mbind",
  "get_mempolicy",
  "set_mempolicy",
  "mq_open",
  "mq_unlink",
  "mq_timedsend",
  "mq_timedreceive",
  "mq_notify",
  "mq_getsetattr",
  "sys_kexec_load",
  "waitid",
  "sys_setaltroot",
  "add_key",
  "request_key",
  "keyctl",
};

const char const * const* syscall_names = _syscall_names;
