# NAME: Carol Lin, Don Le
# EMAIL: carol9gmail@yahoo.com, donle22599@g.ucla.edu
# ID: 804984337, 804971410

default:
	gcc -o lab3a lab3a.c -Wall -Wextra -g

dist:
	tar -czvf lab3a-804984337.tar.gz lab3a.c README Makefile ext2_fs.h

clean:
	rm -f lab3a lab3a-804984337.tar.gz
