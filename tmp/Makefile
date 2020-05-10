#CXXFLAGS = `pkg-config  --cflags libomxil-bellagio`
#LDFLAGS = `pkg-config  --libs libomxil-bellagio`
CXXFLAGS = -I/opt/vc/include
LDFLAGS = -L/opt/vc/lib -lmmal -lmmal_core

OFILES = mmaltest.o

mmaltest: $(OFILES)
	$(CXX) -o $@ $(OFILES) $(LDFLAGS)
