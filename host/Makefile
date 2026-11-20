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

test-gfx: basic
	(cat test_gfx.bas; echo RUN) | ./basic

test-sprite: basic
	rm -rf files.tmp && mkdir files.tmp
	(cat test_sprite.bas; echo RUN; echo 'SAVE "SPR"'; echo 'SPRSET 5,0,0,-1'; echo 'LOAD "SPR"'; echo 'RUN 20') | BASIC_DIR=files.tmp ./basic
	grep '^#SPR' files.tmp/SPR.BAS
	rm -rf files.tmp

test-sound: basic
	rm -rf files.tmp && mkdir files.tmp
	printf '%s\n' '10 PRINT "START": SOUND 3,1: PRINT "AFTER WAIT"' '20 SOUND 3: PRINT "ASYNC": SOUND -1' '#SND 3 1 140 60:4 0:2 64:4 999:99 67:8' > files.tmp/MUSIC.BAS
	printf '%s\n' 'LOAD "MUSIC"' 'RUN' 'SAVE "COPY"' 'SOUND 10' | BASIC_DIR=files.tmp ./basic
	grep '^#SND' files.tmp/COPY.BAS
	rm -rf files.tmp

test-hwsprite: basic
	(cat test_hwsprite.bas; echo RUN) | ./basic

clean:
	rm -f basic
