#CXXFLAGS = `pkg-config  --cflags libomxil-bellagio`
#LDFLAGS = `pkg-config  --libs libomxil-bellagio`
CXXFLAGS = -I/opt/vc/include  -fpermissive
#-Wall
CFLAGS = -I/opt/vc/include  -fpermissive
#-Wall
LDFLAGS = -L/opt/vc/lib -lmmal_util -lmmal -lmmal_core -lvcos -lpthread -ldl -lvchostif -lvchiq_arm -lbcm_host

OFILES = mmaltest.o RaspiCLI.o RaspiHelpers.o RaspiPreview.o RaspiCommonSettings.o RaspiCamControl.o 

mmaltest: $(OFILES)
	$(CXX) -o $@ $(OFILES) $(LDFLAGS)

clean:
	$(RM) mmaltest $(OFILES)
