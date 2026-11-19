CFLAGS := -Wall -Wextra -O2 -g

basic: host_main.c ../source/basic.c ../source/basic.h
	$(CC) $(CFLAGS) -o $@ host_main.c ../source/basic.c -lm

test: basic
	(cat test1.bas; echo RUN; echo Alice; echo abc; echo 21; echo 'PRINT 1+'; echo 'GOTO 999'; echo LIST 10-30) | ./basic

clean:
	rm -f basic
