# --- makefile ---
COMP_OPT = -Wall -O3

# L'eseguibile finale dipende dai 3 object file
IHTC_Test.exe: IHTC_Driver.o IHTC_Data.o IHTC_Solver.o
	g++ -o IHTC_Test.exe IHTC_Driver.o IHTC_Data.o IHTC_Solver.o

IHTC_Driver.o: IHTC_Driver.cc IHTC_Data.hh IHTC_Solver.hh
	g++ $(COMP_OPT) -c IHTC_Driver.cc

IHTC_Data.o: IHTC_Data.cc IHTC_Data.hh json.hpp
	g++ $(COMP_OPT) -c IHTC_Data.cc

IHTC_Solver.o: IHTC_Solver.cc IHTC_Solver.hh IHTC_Data.hh
	g++ $(COMP_OPT) -c IHTC_Solver.cc

clean:
	rm -f *.o IHTC_Test.exe