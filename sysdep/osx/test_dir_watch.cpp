#include <iostream>
#include <vector>
#include <map>
#include <unistd.h>
#include <cstdlib>
#include <pthread.h>
#include <errno.h>
#include <stddef.h>
#include "dir_watch.h"
#include <signal.h>

void onsig(int signal)
{
    // to call functions registered by atexit
    exit(EXIT_FAILURE);
}

extern "C" int main(int argc, char* argv[])
{
	PDirWatch dirWatch1;
    DirWatchNotifications notifications;
    signal(SIGINT, onsig);
    /*
    if (0 != dir_watch_Add("/Users/xuzhiqiang/Develop/c/fam/sysdep/linux", dirWatch1))
    {
		std::cerr << "dir_watch_Add fail" << std::endl;
		return EXIT_FAILURE;
    }
    std::cout << "dirWatch1 OK" << std::endl;
    */
    if (0 != dir_watch_Add("/Users/xuzhiqiang/Develop/c/fam/sysdep/osx", dirWatch1))
    {
		std::cerr << "dir_watch_Add fail" << std::endl;
		return EXIT_FAILURE;
    }
    std::cout << "dirWatch2 OK" << std::endl;

    do {
        sleep(4);
        if (0 != dir_watch_Poll(notifications))
        {
		    std::cerr << "dir_watch_Poll fail" << std::endl;
		    return EXIT_FAILURE;
        }

	    for(size_t i = 0; i < notifications.size(); ++i)
        {
            std::cout << notifications[i].Pathname() << std::endl;
            switch(notifications[i].Type())
            {
                case DirWatchNotification::Created:
                    std::cout << "event:Created" << std::endl;
                    break;
                case DirWatchNotification::Deleted:
                    std::cout << "event:Deleted" << std::endl;
                    break;
                case DirWatchNotification::Changed:
                    std::cout << "event:Changed" << std::endl;
                    break;
            }
        }
        if (notifications.size() > 0)
            notifications.clear();
        else
            std::cout << "nothing fetched" << std::endl;
    } while(true);

    dirWatch1.reset();

    return 0;
}
