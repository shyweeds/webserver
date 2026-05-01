# load the program
file ./wserver

# set args
set args -d . -p 8000 -t 8 -b 16 -s SFF

# layout
layout src

# fork and exec
set follow-fork-mode parent
set detach-on-fork on

# increase the visibility
set pagination off
set print pretty on
set print thread-events off
set print elements 0

# (while running)multi-thread debug settings
define hookpost-run
  set scheduler-locking off
end

# break points
break main

# auto print(like logs)
break request_handle
commands
    silent
    printf "[pthread %lu] fd=%d\n", (unsigned long)pthread_self(), fd
    continue
end

# add log support
set logging enabled off
set logging file gdb.txt
set logging overwrite on
set logging redirect on
set logging enabled on

# look at the threads
define lt
  info threads
  thread apply all bt
end
