
OBJ_DIR := obj
SRC_DIR := src
EXE_DIR := bin
INC_DIR := include

CPP := g++

CFLAGS := -std=c++11 -O3 -I$(INC_DIR)

LIBS := -lpthread -lm

all: ols blr

pthreader: 

	$(CPP) -c $(SRC_DIR)/pthreader.cpp -o $(OBJ_DIR)/pthreader.o

ols: pthreader

	$(CPP) $(CFLAGS) -c $(SRC_DIR)/pt_ols.cpp -o $(OBJ_DIR)/pt_ols.o
	$(CPP) -o $(EXE_DIR)/pt_ols $(OBJ_DIR)/pthreader.o $(OBJ_DIR)/pt_ols.o $(LIBS)

blr: pthreader

	$(CPP) $(CFLAGS) -c $(SRC_DIR)/pt_blr.cpp -o $(OBJ_DIR)/pt_blr.o
	$(CPP) -o $(EXE_DIR)/pt_blr $(OBJ_DIR)/pthreader.o $(OBJ_DIR)/pt_blr.o $(LIBS)

clean: 

	rm -rf $(OBJ_DIR)/*.o $(EXE_DIR)/???*