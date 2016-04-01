all:window
install:
	sudo cp window /usr/bin/
window:window.c
	gcc window.c -L /usr/X11R6/lib/ -lX11 -o $@
clean:
	rm -vf window
