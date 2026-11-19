CFLAGS := -Wall -Wextra -O2 -g

basic: host_main.c ../source/basic.c ../source/basic.h
	$(CC) $(CFLAGS) -o $@ host_main.c ../source/basic.c -lm

test: basic
	(cat test1.bas; echo RUN; echo Alice; echo abc; echo 21; echo 'PRINT 1+'; echo 'GOTO 999'; echo LIST 10-30) | ./basic

test-files: basic
	rm -rf files.tmp && mkdir files.tmp
	printf '%s\n' '10 PRINT "HI"' '20 X=1' 'SAVE "hello"' 'SAVE "two.bas"' 'DIR' 'NEW' 'LIST' \
		'LOAD "HELLO"' 'LIST' 'SAVE' 'DELETE "TWO"' 'DIR' 'LOAD "NOPE"' 'SAVE "BAD NAME"' \
		'SAVE "TOOLONGNAME"' | BASIC_DIR=files.tmp ./basic
	@echo "--- without storage:"; echo 'SAVE "X"' | ./basic
	rm -rf files.tmp

clean:
	rm -f basic
