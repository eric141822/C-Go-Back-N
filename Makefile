COMPILERFLAGS = -g -Wall -Wextra -Wno-sign-compare 

LINKLIBS = -lpthread

SERVEROBJECTS = obj/receiver_main.o
CLIENTOBJECTS = obj/sender_main.o

.PHONY: all clean

all : obj \
reliable_sender \
reliable_receiver \

reliable_receiver: $(SERVEROBJECTS)
	$(CC) $(COMPILERFLAGS) $^ -o $@ $(LINKLIBS)

reliable_sender: $(CLIENTOBJECTS)
	$(CC) $(COMPILERFLAGS) $^ -o $@ $(LINKLIBS)



clean :
	$(RM) obj/*.o reliable_sender reliable_receiver
	rmdir obj

obj/%.o: src/%.c
	$(CC) $(COMPILERFLAGS) -c -o $@ $<
obj:
	mkdir -p obj

