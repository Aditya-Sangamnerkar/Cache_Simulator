CC = g++
OPT = -O3
#OPT = -g
WARN = -Wall
CPP_STD = -std=c++11
CFLAGS = $(OPT) $(WARN) $(INC) $(LIB) $(CPP_STD)

# List all your .cc/.cpp files here (source files, excluding header files)
SIM_SRC = sim_mark_2.2.cpp

# List corresponding compiled object files here (.o files)
SIM_OBJ = sim_mark_2.2.o
 
#################################

# default rule

all: clean sim
	@echo "my work is done here..."


# rule for making sim

sim: $(SIM_OBJ)
	$(CC) -o sim $(CFLAGS) $(SIM_OBJ) -lm
	@echo "-----------DONE WITH sim-----------"


# generic rule for converting any .cc file to any .o file
 
$(SIM_OBJ): $(SIM_SRC)
	$(CC) $(CFLAGS) -c $(SIM_SRC)


# type "make clean" to remove all .o files plus the sim binary

clean:
	rm -f *.o sim


# type "make clobber" to remove all .o files (leaves sim binary)

clobber:
	rm -f *.o


