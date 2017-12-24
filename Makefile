# Makefile for PiMSF
# Mark Street <marksmanuk@gmail.com>

CC		= g++
RM		= rm
FLAGS	= -O3 -Wall
EXE		= pimsf

%.o: %.c
	$(CC) $(FLAGS) -c -o $@ $<

all: $(EXE)

clean:
	$(RM) *.o $(EXE)

$(EXE): pimsf.o
	$(CC) -o $@ $<
