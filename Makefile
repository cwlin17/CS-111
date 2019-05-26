# NAME: Carol Lin
# EMAIL: carol9gmail@yahoo.com
# ID: 804984337

default:
	gcc -o lab3a lab3a.c -Wall -Wextra -g

dist:
	tar -czvf lab3a-804984337.tar.gz lab3a.c README Makefile ext2_fs.h

clean:
	rm -f lab3a lab3a-804984337.tar.gz
