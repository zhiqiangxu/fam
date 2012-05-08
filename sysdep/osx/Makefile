CC            = g++
CFLAGS        = -Wall -g
LFLAGS        = -lpthread -framework CoreServices -framework CoreFoundation
INCLUDE       = -I../..

one_targets = signal
two_targets = file_utils

$(one_targets): % :    %.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -o "$@" $^ $(LFLAGS)


$(two_targets):  % :     %.cpp test_%.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -o "$@" $^ $(LFLAGS)

dir_watch:  dir_watch.cpp test_dir_watch.cpp file_utils.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -o "$@" $^ $(LFLAGS)

.PHONY: clean
clean:
	rm -rf $(two_targets) $(one_targets) dir_watch *.dSYM
