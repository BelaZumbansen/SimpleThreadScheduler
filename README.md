# SimpleThreadScheduler
** NECESSARY PROJECT SETUP FOR COMPILATION **\
projectFolder\
    sut.c\
    sut.h\
    tcb.h\
    tcb.c\
    queue.h\
    queue.c\
    test4.txt\
    test5.txt

** COMPILATION INSTRUCTIONS **\
Test 1: gcc test1.c sut.c queue.c tcb.c -pthread && ./a.out > test1-stdout.txt\
Test 2: gcc test2.c sut.c queue.c tcb.c -pthread && ./a.out > test2-stdout.txt\
Test 3: gcc test3.c sut.c queue.c tcb.c -pthread && ./a.out > test3-stdout.txt\
Test 4: gcc test4.c sut.c queue.c tcb.c -pthread && ./a.out > test4-stdout.txt\
Test 5: gcc test5.c sut.c queue.c tcb.c -pthread && ./a.out > test5-stdout.txt

I had to C_INCLUDE_PATH=<path to projectFolder> export C_INCLUDE_PATH to get a compilation success but I assume that was a problem with my environment.
