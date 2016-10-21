all: raycast.c
	gcc main.c -o raycast

clean:
	rm -rf raycast *~
