# load the program
file ./wserver

# set args
set args -d ./basedir/ -p 8003 -t 8 -b 16 -s FIFO

# layout
layout src

# fork and exec
set follow-fork-mode parent
set detach-on-fork on

# increase the visibility
set pagination off
set print pretty on
set print thread-events on
set print elements 0

# (while running)multi-thread debug settings
define hookpost-run
  set scheduler-locking step
end

# break points
break main
break 253

# auto print(like logs)
break request_handle
commands
    silent
    printf "[pthread %lu] fd=%d\n", (unsigned long)pthread_self(), fd
    continue
end

# look at the threads
define lt
  info threads
  thread apply all bt
end
