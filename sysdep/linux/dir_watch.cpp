#include <iostream>
#include <vector>
#include <map>
#include <unistd.h>
#include <cstdlib>
#include <sys/inotify.h>
#include <pthread.h>
#include <errno.h>
#include <stddef.h>
#include "dir_watch.h"
#include <signal.h>

#define WATCH_MASK IN_CREATE | IN_DELETE | IN_MODIFY

static int inotifyfd; 
static std::map<int, DirWatch*> g_paths;
struct DirWatch
{
	DirWatch()
	{
	}

	~DirWatch()
	{
        std::map<int, DirWatch*>::iterator it = g_paths.find(fd);
        if (it != g_paths.end())
        {
            g_paths.erase(it);
        }
        if (0 == inotify_rm_watch(inotifyfd, fd))
        {
            std::cout << "inotify_rm_watch successfully:" << path << std::endl;
        }
        else
        {
            // maybe after exit
            std::cout << "inotify_rm_watch failed:" << path << std::endl;
        }
	}

	int fd;
    std::string path;
};
static pthread_t g_event_loop_thread;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
struct NotificationEvent
{
	std::string filename;
    uint32_t code;
    int wd;
};
static std::vector<NotificationEvent> g_notifications;
static int initialized = 0;

static void fam_deinit()
{
    close(inotifyfd);

	pthread_cancel(g_event_loop_thread);
	// NOTE: POSIX threads are (by default) only cancellable inside particular
	// functions (like 'select'), so this should safely terminate while it's
	// in select/FAMNextEvent/etc (and won't e.g. cancel while it's holding the
	// mutex)

	// Wait for the thread to finish
	pthread_join(g_event_loop_thread, NULL);
    std::cout << "thread join successfully" << std::endl;
}

static void event_loop_process_events()
{
    char buffer[65535];
    ssize_t r = read(inotifyfd, buffer, sizeof(buffer));
    if (r <= 0)
        return; 
    size_t event_size; 
    struct inotify_event *pevent; 
    ssize_t buffer_i = 0;
    while(buffer_i < r)
    {
        pevent = (struct inotify_event *) &buffer[buffer_i]; 
        event_size = offsetof(struct inotify_event, name) + pevent->len;
        NotificationEvent ne; 
        ne.wd = pevent->wd;
        ne.filename = pevent->name;
        ne.code = pevent->mask;

        pthread_mutex_lock(&g_mutex); 
        g_notifications.push_back(ne); 
        std::cout << "g_notifications.push_back" << std::endl;
        pthread_mutex_unlock(&g_mutex); 

        buffer_i += event_size;
    }
}

static void* event_loop(void*)
{
    while(true)
    {
		fd_set fdrset;
		FD_ZERO(&fdrset);
		FD_SET(inotifyfd, &fdrset);

        while(select(inotifyfd+1, &fdrset, NULL, NULL, NULL) < 0)
        {
            if (errno == EINTR)
            {
                // interrupted - try again 
                FD_ZERO(&fdrset); 
                FD_SET(inotifyfd, &fdrset);
            }
            else if (errno == EBADF)
            {
                std::cerr << "EBADF" << std::endl;
                return NULL;
            }
            else
            {
                std::cerr << "oops" << std::endl;
                return NULL;
            }
        }
		if (FD_ISSET(inotifyfd, &fdrset))
			event_loop_process_events();

    }
    return NULL;
}

int dir_watch_Add(const char* path, PDirWatch& dirWatch)
{
    std::cout << path << std::endl;
    if (!initialized)
    {
        if ((inotifyfd = inotify_init()) < 0)  
        {
            std::cerr << "inotify_init failed" << std::endl;
            return -1;
        }

		if (pthread_create(&g_event_loop_thread, NULL, &event_loop, NULL))
        {
            std::cerr << "pthread_create failed" << std::endl;
            return -1;
        }
        initialized = 1;
        atexit(fam_deinit);
    }

    int wd;
    if ( (wd = inotify_add_watch(inotifyfd, path, WATCH_MASK)) < 0) 
    {
        std::cerr << "inotify_add_watch failed" << std::endl;
        return -1;
    }
	PDirWatch tmpDirWatch(new DirWatch);
    dirWatch.swap(tmpDirWatch);
    dirWatch->fd = wd;
    dirWatch->path = path;
    std::cout << "debug1" << std::endl;
    g_paths.insert(std::pair<int, DirWatch*>(wd, dirWatch.get()));
    std::cout << "debug2" << std::endl;

    return 0;
}


int dir_watch_Poll(DirWatchNotifications& notifications)
{
    if (1 != initialized)
        return -1;
	std::vector<NotificationEvent> polled_notifications;

	pthread_mutex_lock(&g_mutex);
	g_notifications.swap(polled_notifications);
	pthread_mutex_unlock(&g_mutex);

    if (0 == polled_notifications.size())
    {
        std::cout << "polled_notifications.size is 0" << std::endl;
    }
    else
    {
        std::cout << "polled_notifications.size is " << polled_notifications.size() << std::endl;
    }
	for(size_t i = 0; i < polled_notifications.size(); ++i)
    {
		DirWatchNotification::EType type;
		switch (polled_notifications[i].code)
        {
            case IN_MODIFY:
                type = DirWatchNotification::Changed;
                break;
            case IN_CREATE: 
                type = DirWatchNotification::Created; 
                break;
            case IN_DELETE: 
                type = DirWatchNotification::Deleted; 
                break;
            default:
                std::cout << "ignored event:" << polled_notifications[i].code << std::endl;
                continue;
        }
        std::string filename;
        std::map<int, DirWatch*>::iterator it = g_paths.find(polled_notifications[i].wd);
        if (it != g_paths.end())
        {
            filename = it->second->path + "/" + polled_notifications[i].filename;
        }
        else
        {
            std::cout << "event of removed wd ignored" << std::endl;
            continue;
        }
        std::cout << filename << std::endl;
        notifications.push_back(DirWatchNotification(filename, type));
    }
    return 0;
}
