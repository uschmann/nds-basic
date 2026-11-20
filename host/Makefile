CFLAGS := -Wall -Wextra -O2 -g

basic: host_main.c ../source/php.c ../source/basic.h
	$(CC) $(CFLAGS) -o $@ host_main.c ../source/php.c -lm

# language test: compares the output with test_php.expected
test: basic
	(cat test_php.txt; echo RUN; echo Alice; echo 'echo 6*7, PHP_EOL;'; echo 'echo fib(12), PHP_EOL;'; \
	 echo 'echo 1 +;'; echo 'foo();'; echo '10 $$x = 0;'; echo '20 echo 1 / $$x;'; echo RUN) | ./basic > test.out
	diff -u test_php.expected test.out && echo "language test passed"
	rm -f test.out

test-files: basic
	rm -rf files.tmp && mkdir files.tmp
	printf '%s\n' '10 echo "HI";' '20   $$x = 1; // indented' 'SAVE "hello"' 'SAVE "two.php"' 'DIR' 'NEW' 'LIST' \
		'LOAD "HELLO"' 'LIST' 'SAVE' 'DELETE "TWO"' 'DIR' 'LOAD "NOPE"' 'SAVE "BAD NAME"' \
		'SAVE "TOOLONGNAME"' | BASIC_DIR=files.tmp ./basic
	cat files.tmp/HELLO.PHP
	@echo "--- without storage:"; echo 'SAVE "X"' | ./basic
	rm -rf files.tmp

test-examples: basic
	rm -rf files.tmp && mkdir files.tmp && cp ../examples/*.PHP files.tmp/
	(echo 'LOAD "SNAKE"'; echo RUN) | BASIC_DIR=files.tmp timeout 20 ./basic | tail -3
	(echo 'LOAD "PONG"'; echo RUN) | BASIC_DIR=files.tmp timeout 60 ./basic | tail -3
	(echo 'LOAD "PONG"'; echo 'SAVE "COPY"') | BASIC_DIR=files.tmp ./basic
	grep -c '^#S' files.tmp/COPY.PHP
	rm -rf files.tmp

clean:
	rm -f basic test.out
