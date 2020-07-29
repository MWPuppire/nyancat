CC = gcc
CFLAGS = -Iinclude -lmikmod
.PHONY: clean install
nyancat: src/nyancat.c
	$(CC) $(CFLAGS) -o nyancat src/nyancat.c
clean:
	rm -f nyancat
install:
	cp -p nyancat /usr/local/bin/nyancat
	gzip -9 -c < nyancat.1 > /usr/local/share/man/man1/nyancat.1.gz
uninstall:
	rm -f /usr/local/bin/nyancat
	rm -f /usr/local/share/man/man1/nyancat.1.gz
