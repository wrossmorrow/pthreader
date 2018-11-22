
.SHELL  := bash

CONFIG  := mkdir -p bin obj

OBJ_DIR := obj
SRC_DIR := src
EXE_DIR := bin
INC_DIR := include

EXM_DIR := examples

CPP 	:= g++

CFLAGS 	:= -std=c++11 -xHost -O3 -prec-div -no-ftz -restrict -I$(INC_DIR)

LIBS 	:= -lpthread -lm

# if you have GSL, fill these out
GSL_SHARED_LIB  := /share/software/user/open/gsl/2.3/lib
GSL_LIBS		:= -L$(GSL_SHARED_LIB) -lgsl -lgslcblas -lm
GSL_INCL 		:= -I/share/software/user/open/gsl/2.3/include

all: pthreader examples

pthreader: env

	$(CPP) $(CFLAGS) -c $(SRC_DIR)/pthreader.cpp -o $(OBJ_DIR)/pthreader.o

examples: env ols blr

ols: env pthreader

	$(CPP) $(CFLAGS) -c $(EXM_DIR)/pt_ols.cpp -o $(OBJ_DIR)/pt_ols.o
	$(CPP) -o $(EXE_DIR)/pt_ols $(OBJ_DIR)/pthreader.o $(OBJ_DIR)/pt_ols.o $(LIBS)

blr: env pthreader

	$(CPP) $(CFLAGS) -c $(EXM_DIR)/pt_blr.cpp -o $(OBJ_DIR)/pt_blr.o
	$(CPP) -o $(EXE_DIR)/pt_blr $(OBJ_DIR)/pthreader.o $(OBJ_DIR)/pt_blr.o $(LIBS)

gsl: env pthreader

	$(CPP) $(CFLAGS) $(GSL_INCL) -c $(EXM_DIR)/pt_ols_gsl.cpp -o $(OBJ_DIR)/pt_ols_gsl.o
	$(CPP) -o $(EXE_DIR)/pt_ols_gsl $(OBJ_DIR)/pthreader.o $(OBJ_DIR)/pt_ols_gsl.o $(LIBS) $(GSL_LIBS)
	@echo " "
	@echo "You may need to add $(GSL_SHARED_LIB) to LD_LIBRARY_PATH to run $(EXE_DIR)/pt_ols_gsl"
	@echo " "

env: 

	$(CONFIG)

clean: 

	rm -rf $(OBJ_DIR)/*.o $(EXE_DIR)/???*