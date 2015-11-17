#Please specify the path of the following libraries
FASTFLOW_DIR	=  specify_the_path_to_fastflow
LMFIT_DIR		= specify_the_path_to_lmfit
MAMMUT_DIR		= specify_the_path_to_mammut

CXX             = g++
CXXFLAGS        = -O3 --std=c++11
LIBS            = -lpthread -lm -lrt
INCLUDES		= includes
AUX_DIR			= auxiliary
SRC		     	= src
MAMMUT_LIB		= $(MAMMUT_DIR)/lib/
MAMMUT_INC		= $(MAMMUT_DIR)/include/
LMFIT_INC		= $(LMFIT_DIR)/include/
LMFIT_LIB		= $(LMFIT_DIR)/lib/
TARGET				 = real_generator synthetic_generator elastic-hft derive_voltage_table
DEFINES				 = -DMONITORING 

.PHONY: all clean

all: real-generator synthetic-generator elastic-hft derive-voltage-table

real-generator: $(SRC)/real_generator.cpp $(AUX_DIR)/socket_func.cpp $(INCLUDES)/general.h utils.o
	$(CXX) $(CXXFLAGS) $(AUX_DIR)/socket_func.cpp $(SRC)/real_generator.cpp utils.o -o $@ $(DEFINES) $(LIBS)  -I$(FASTFLOW_DIR) -L$(MAMMUT_LIB) -lmammut

synthetic-generator: $(SRC)/synthetic_generator.cpp $(AUX_DIR)/socket_func.cpp $(INCLUDES)/general.h utils.o
	$(CXX) $(CXXFLAGS) $(AUX_DIR)/socket_func.cpp $(SRC)/synthetic_generator.cpp utils.o -o $@ $(DEFINES) $(LIBS)  -I$(FASTFLOW_DIR) -L$(MAMMUT_LIB) -lmammut

elastic-hft: elastic-hft.o splitter.o merger.o replica.o controller.o socket_func.o HoltWinters.o utils.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIBS) -I$(LMFIT_INC) $(LMFIT_LIB)/liblmfit.so -L$(MAMMUT_LIB) -lmammut  -lpthread -lrt -lm
 
derive-voltage-table: utils/derive_voltage_table.cpp
	$(CXX) $(CXXFLAGS) -o $@  $^ $(LIBS)  -I$(FASTFLOW_DIR) -I$(MAMMUT_INC) -L$(MAMMUT_LIB) -lmammut

HoltWinters.o: $(SRC)/HoltWinters.cc
	$(CXX) $(CXXFLAGS) -c -o  $@ $<

%.o: $(SRC)/%.cpp $(INCLUDES)/*
	$(CXX) $(CXXFLAGS) -c -o $@ $< -I$(FASTFLOW_DIR) $(DEFINES) $(LIBS) -I$(LMFIT_INC) -I$(MAMMUT_INC)

utils.o: $(SRC)/utils.cpp $(INCLUDES)/utils.h
	$(CXX) $(CXXFLAGS) -c -o $@ $< -I$(FASTFLOW_DIR) $(DEFINES) $(LIBS) -I$(LMFIT_INC) -I$(MAMMUT_INC)

socket_func.o: $(AUX_DIR)/socket_func.cpp $(INCLUDES)/general.h  $(INCLUDES)/affinities.h 
	$(CXX) $(CXXFLAGS) -c -o $@ $< -I$(FASTFLOW_DIR) $(DEFINES) $(LIBS)

clean:
	rm -f *.o *.~ $(TARGET)
