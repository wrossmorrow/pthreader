
OBJ_DIR := obj
SRC_DIR := src
EXE_DIR := bin

all: ols blr

pthreader: 

	g++ -std=c++11 -O3 -c $(SRC_DIR)/pthreader.cpp -o $(OBJ_DIR)/pthreader.o

ols: pthreader

	g++ -std=c++11 -O3 -c $(SRC_DIR)/pt_ols.cpp -o $(OBJ_DIR)/pt_ols.o
	g++ -o $(EXE_DIR)/pt_ols $(OBJ_DIR)/pthreader.o $(OBJ_DIR)/pt_ols.o -lpthread -lm

blr: pthreader

	g++ -std=c++11 -O3 -c $(SRC_DIR)/pt_blr.cpp -o $(OBJ_DIR)/pt_blr.o
	g++ -o $(EXE_DIR)/pt_blr $(OBJ_DIR)/pthreader.o $(OBJ_DIR)/pt_blr.o -lpthread -lm

clean: 

	rm -rf $(OBJ_DIR)/*.o $(EXE_DIR)/???*