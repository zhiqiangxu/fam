CC            = g++
CFLAGS        = -Wall -g
LFLAGS        = -lpthread
INCLUDE       = -I../..

two_targets = file_utils

$(two_targets):  % :     %.cpp test_%.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -o "$@" $^ $(LFLAGS)

dir_watch:  dir_watch.cpp test_dir_watch.cpp file_utils.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -o "$@" $^ $(LFLAGS)

.PHONY: clean
clean:
	find . -executable -type f -exec rm -f {} \;
