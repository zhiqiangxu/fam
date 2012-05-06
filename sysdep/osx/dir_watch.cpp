#include <iostream>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include "file_utils.h"
#include <pthread.h>


// ----------------- run loop signal handling stuff ---------------------
CFFileDescriptorRef   kq_cffd   = NULL;
CFRunLoopSourceRef    kq_rl_src = NULL;
int                   kq_fd     = -1;
CFRunLoopRef loop = NULL;

static void kq_cffd_callback(CFFileDescriptorRef kq_cffd, CFOptionFlags callBackTypes, void *)
{
    CFRunLoopStop(loop);
}

int setup_run_loop_signal_handler()
{
    CFFileDescriptorContext my_context;
    struct kevent           kev[4];

    kq_fd = kqueue();
    if (kq_fd < 0) 
    {
	    return -1;
    }

    EV_SET(&kev[0], SIGINT,  EVFILT_SIGNAL, EV_ADD, 0, 0, 0);
    EV_SET(&kev[1], SIGQUIT, EVFILT_SIGNAL, EV_ADD, 0, 0, 0);
    EV_SET(&kev[2], SIGTERM, EVFILT_SIGNAL, EV_ADD, 0, 0, 0);
    EV_SET(&kev[3], SIGHUP,  EVFILT_SIGNAL, EV_ADD, 0, 0, 0);

    if (kevent(kq_fd, &kev[0], 4, NULL, 0, NULL) != 0) 
    {
        close(kq_fd);
        return -1;
    }
    
    memset(&my_context, 0, sizeof(CFFileDescriptorContext));
    my_context.info = (void *)loop;

    kq_cffd = CFFileDescriptorCreate(NULL, kq_fd, 1, kq_cffd_callback, &my_context);
    if (kq_cffd == NULL) 
    {
        close(kq_fd);
        return -1;
    }

    kq_rl_src = CFFileDescriptorCreateRunLoopSource(NULL, kq_cffd, (CFIndex)0);
    if (kq_rl_src == NULL) 
    {
        // Dispose the kq_cffd
        CFFileDescriptorInvalidate(kq_cffd);
        CFRelease(kq_cffd);
        kq_cffd = NULL;

        return -1;
    }

    CFRunLoopAddSource(loop, kq_rl_src, kCFRunLoopDefaultMode);
    CFFileDescriptorEnableCallBacks(kq_cffd, kCFFileDescriptorReadCallBack);

    return 0;
}

void cleanup_run_loop_signal_handler(CFRunLoopRef loop)
{
    CFRunLoopRemoveSource(loop, kq_rl_src, kCFRunLoopDefaultMode);

    CFFileDescriptorInvalidate(kq_cffd);
    CFRelease(kq_rl_src);
    CFRelease(kq_cffd);
    close(kq_fd);

    kq_rl_src = NULL;
    kq_cffd   = NULL;
    kq_fd     = -1;
}

struct DirWatch
{
	DirWatch()
	{
	}

	~DirWatch()
	{
        // Although it's not strictly necessary, make sure we see any pending events... 
        FSEventStreamFlushSync(stream);
        FSEventStreamStop(stream);
        FSEventStreamInvalidate(stream);
        FSEventAStreamRelease(stream);
        stream = NULL;
	}

    std::string path;
};


static pthread_t g_event_loop_thread;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
struct NotificationEvent
{
	std::string filename;
    FSEventStreamEventId id;
    FSEventStreamEventFlags flags;
};
static std::vector<NotificationEvent> g_notifications;
CFMutableArrayRef     g_paths;
static volatile bool done = false;

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

static inline add_paths(char* path)
{
    CFStringRef macpath = CFStringCreateWithCString(
        NULL,
        (char*)path,
        kCFStringEncodingUTF8
    );
    if (macpath == NULL) 
    {
        std::cerr << "ERROR: CFStringCreateWithCString() => NULL" << std::endl;
	    CFRelease(cfArray);
        return NULL;
    }
    CFArrayInsertValueAtIndex(g_paths, 0, macpath);
    CFRelease(macpath);
}

static void* event_loop(void*)
{
    loop = CFRunLoopGetCurrent();
    FSEventStreamRef      stream_ref = NULL;

    do {
        stream_ref = FSEventStreamCreate(kCFAllocatorDefault,
                                    (FSEventStreamCallback)&fsevents_callback,
                                    &context,
                                    g_paths,
                                    settings->since_when,
                                    settings->latency,
                                    kFSEventStreamCreateFlagNone);
        CFRunLoopRun();
        FSEventStreamFlushSync(stream_ref);
        FSEventStreamStop(stream_ref);
        FSEventStreamUnscheduleFromRunLoop(stream_ref, loop, kCFRunLoopDefaultMode);
        FSEventStreamInvalidate(stream_ref);
        FSEventStreamRelease(stream_ref);
    }
    while(!done);
}

int dir_watch_Delete(const char* path)
{
}

int dir_watch_Add(const char* path, PDirWatch& dirWatch)
{
    std::cout << path << std::endl;
    if (!initialized)
    {
        g_paths = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        if (g_paths == NULL) 
        {
            std::cerr << "ERROR: CFArrayCreateMutable() => NULL" << std::endl;
            return NULL;
        }

		if (pthread_create(&g_event_loop_thread, NULL, &event_loop, NULL)
        {
            std::cerr << "pthread_create failed" << std::endl;
            return -1;
        }
        initialized = 1;
    }
    else
    {
        add_paths(path);
        CFRunLoopStop(loop);
    }

	PDirWatch tmpDirWatch(new DirWatch);
    dirWatch.swap(tmpDirWatch);
    dirWatch->path = path;

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
