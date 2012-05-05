CC            = g++
CFLAGS        = -Wall -g
LFLAGS        = -lpthread
INCLUDE       = -I../..

one_targets = test
two_targets = dir_watch

$(two_targets):  % :     %.cpp test_%.cpp
	$(CC) $(INCLUDE) $(CFLAGS) -o "$@" $^ $(LFLAGS)

$(one_targets):  % :     %.cpp 
	$(CC) $(INCLUDE) $(CFLAGS) -o "$@" $^ $(LFLAGS)
#sudo apt-get install freeglut3-dev

.PHONY: clean
clean:
	find . -executable -type f -exec rm -f {} \;
