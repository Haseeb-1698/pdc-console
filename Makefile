CC = mpicc
CFLAGS = -Wall -O2
LDFLAGS = -lm

all: q1 q2 q3

q1: q1-Lubna/main.c
	$(CC) $(CFLAGS) -o q1 q1-Lubna/main.c $(LDFLAGS)

q2: q2-Insharah/main.c q2-Insharah/attack_detection.c q2-Insharah/attack_detection.h
	$(CC) $(CFLAGS) -o q2 q2-Insharah/main.c q2-Insharah/attack_detection.c $(LDFLAGS)

q3: q3-haseeb/main.c
	$(CC) $(CFLAGS) -o q3 q3-haseeb/main.c $(LDFLAGS)

clean:
	rm -f q1 q2 q3

.PHONY: all clean
