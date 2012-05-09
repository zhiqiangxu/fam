#include <iostream>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <vector>
#include <map>
#include <string.h>
#include <limits.h>
#include <libgen.h>
#include <unistd.h>
#include "dir_watch.h"
#include "file_utils.h"


void cleanup_run_loop_signal_handler();

int dir_watch_Delete(const char* path)
{
    return 0;
}

struct DirWatch
{
    DirWatch()
    {
    }

    ~DirWatch()
    {
        dir_watch_Delete(path.c_str());
    }

    std::string path;
};


static pthread_t g_event_loop_thread;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;
static std::vector<DirWatchNotification> g_notifications;
CFRunLoopRef loop = NULL;
static int initialized = 0;
static CFMutableArrayRef     g_paths;
static volatile bool done = false;
static int pipes[2];

static void fam_deinit()
{
    done = true;
    cleanup_run_loop_signal_handler();
    CFRunLoopStop(loop);

    // Wait for the thread to finish
    pthread_join(g_event_loop_thread, NULL);
    std::cout << "thread join successfully" << std::endl;

}

typedef std::map< std::string, std::map<std::string, struct dentry> > t_timestamp_map;
typedef std::map<std::string, struct dentry> t_timestamp_inner_map;
typedef std::map< std::string, std::map<std::string, struct dentry> >::iterator t_timestamp_map_iterator;
typedef std::map<std::string, struct dentry>::iterator t_timestamp_inner_map_iterator;
t_timestamp_map g_timestamps;
char        path_buff[PATH_MAX];

struct dentry
{
    struct timespec ts;
    off_t size;
    bool is_dir;
};

static void dump_timestamps(t_timestamp_map& map)
{
    for (t_timestamp_map_iterator it = map.begin(); it != map.end(); it++)
    {
        std::cout << it->first << std::endl;
        for (t_timestamp_inner_map_iterator it_inner = it->second.begin(); it_inner != it->second.end(); it_inner++)
        {
            std::cout << it_inner->first << std::endl;
        }
    }
}

static void record_timestamps(t_timestamp_map* pmap, const char* path)
{
    struct stat st;
    if (lstat(path, &st) == 0)
    {
        strcpy(path_buff, path);
        char* dir = dirname(path_buff);
        t_timestamp_map_iterator it = pmap->find(dir);
        struct dentry d = {ts:st.st_mtimespec, size:st.st_size, is_dir: S_ISDIR(st.st_mode)};
        if (it != pmap->end())
        {
            it->second.insert(std::pair<std::string, struct dentry>(path, d));
        }
        else
        {
            t_timestamp_inner_map inner_map;
            inner_map.insert(std::pair<std::string, struct dentry>(path, d));
            pmap->insert(std::pair<std::string, t_timestamp_inner_map>(dir, inner_map));
        }
    }
}

static inline void add_to_g_paths(const char* path)
{
    vfs::ForEachFile(path, (void *)record_timestamps, true, &g_timestamps);

    CFStringRef macpath = CFStringCreateWithCString(
        NULL,
        path,
        kCFStringEncodingUTF8
    );
    if (macpath == NULL) 
    {
        std::cerr << "ERROR: CFStringCreateWithCString() => NULL" << std::endl;
        return;
    }
    CFArrayInsertValueAtIndex(g_paths, 0, macpath);
    CFRelease(macpath);
}

static void compare_timestamps(t_timestamp_map& old_map, t_timestamp_map& new_map)
{
    #define time_equal(a, b) (a.tv_sec == b.tv_sec && a.tv_nsec == b.tv_nsec)

    t_timestamp_map_iterator it_old;
    t_timestamp_map_iterator it_new;
    t_timestamp_inner_map_iterator it_old_inner;
    t_timestamp_inner_map_iterator it_new_inner;
    t_timestamp_inner_map inner_map;
    for (it_old = old_map.begin(); it_old != old_map.end(); it_old++)
    {
        inner_map = it_old->second;
        it_new = new_map.find(it_old->first);
        for (it_old_inner = inner_map.begin(); it_old_inner != inner_map.end(); it_old_inner++)
        {
            if ((it_new == new_map.end()) || ((it_new_inner = it_new->second.find(it_old_inner->first)) == it_new->second.end()))
            {
                // deleted
                DirWatchNotification dn(it_old_inner->first, DirWatchNotification::Deleted);
                g_notifications.push_back(dn);
            }
            else if (!time_equal(it_new_inner->second.ts, it_old_inner->second.ts) || (it_new_inner->second.size != it_old_inner->second.size))
            {
                // updated
                DirWatchNotification dn(it_old_inner->first, DirWatchNotification::Changed);
                g_notifications.push_back(dn);
            }
        }
    }
    for (it_new = new_map.begin(); it_new != new_map.end(); it_new++)
    {
        inner_map = it_new->second;
        it_old = old_map.find(it_new->first);
        for (it_new_inner = inner_map.begin(); it_new_inner != inner_map.end(); it_new_inner++)
        {
            if ((it_old == old_map.end()) || ((it_old_inner = it_old->second.find(it_new_inner->first)) == it_old->second.end()))
            {
                // new
                DirWatchNotification dn(it_new_inner->first, DirWatchNotification::Created);
                g_notifications.push_back(dn);
            }
        }
    }

    #undef time_equal
}

static void fsevents_callback(FSEventStreamRef streamRef, void *clientCallBackInfo,
                  int numEvents,
                  const char *const eventPaths[],
                  const FSEventStreamEventFlags *eventFlags,
                  const uint64_t *eventIDs)
{
    std::cout << "fsevents_callback" << std::endl;
    for (int i=0; i < numEvents; i++)
    {
        bool recursive = false;
        if (eventFlags[i] & kFSEventStreamEventFlagHistoryDone) 
        {
            std::cout << "Done processing historical events." << std::endl;
            continue;
        }
        else if (eventFlags[i] & kFSEventStreamEventFlagRootChanged) 
        {
            // do not handle this flag temporarily
            continue;
        }
        else if (eventFlags[i] & kFSEventStreamEventFlagMustScanSubDirs)
        {
            recursive = true;
        }
        t_timestamp_map new_fs;
        vfs::ForEachFile(eventPaths[i], (void *)record_timestamps, recursive, &new_fs);
        t_timestamp_map_iterator it_new;
        t_timestamp_map_iterator it_g;
        t_timestamp_map old_fs;
        for (it_new = new_fs.begin(); it_new != new_fs.end(); it_new++)
        {
            it_g = g_timestamps.find(it_new->first);
            if (it_g != g_timestamps.end())
            {
                old_fs.insert(std::pair<std::string, t_timestamp_inner_map>(it_g->first, it_g->second));
            }
        }
        std::cout << "dumping g fs:" << std::endl;
        dump_timestamps(g_timestamps);
        std::cout << "dumping old fs:" << std::endl;
        dump_timestamps(old_fs);
        std::cout << "dumping new fs:" << std::endl;
        dump_timestamps(new_fs);
        compare_timestamps(old_fs, new_fs);
        for (it_new = new_fs.begin(); it_new != new_fs.end(); it_new++)
        {
            g_timestamps[it_new->first] = it_new->second;
        }
    }
}

static CFFileDescriptorRef fdref = NULL;
static CFRunLoopSourceRef fd_rl_src = NULL;

static void pipe_callback(CFFileDescriptorRef kq_cffd, CFOptionFlags callBackTypes, void *info)
{
    std::cout << "pipe_callback" << std::endl;
    char c;
    while(read(pipes[0], &c, 1) > 0);
    //CFRunLoopWakeUp(loop);
    CFRunLoopStop(loop);
    std::cout << "CFRunLoopWakeUp return" << std::endl;
    CFFileDescriptorEnableCallBacks(fdref, kCFFileDescriptorReadCallBack);
}

int setup_run_loop_pipe_read_handler()
{
    fdref = CFFileDescriptorCreate(NULL, pipes[0], 1, pipe_callback, NULL);
    if (fdref == NULL)
    {
        return -1;
    }

    fd_rl_src = CFFileDescriptorCreateRunLoopSource(NULL, fdref, (CFIndex)0);
    if (fd_rl_src == NULL)
    {
        CFFileDescriptorInvalidate(fdref);
        CFRelease(fdref);
        fdref = NULL;
        return -1;
    }

    CFRunLoopAddSource(loop, fd_rl_src, kCFRunLoopDefaultMode);
    CFFileDescriptorEnableCallBacks(fdref, kCFFileDescriptorReadCallBack);

    return 0;
}

void cleanup_run_loop_signal_handler()
{
    CFRunLoopRemoveSource(loop, fd_rl_src, kCFRunLoopDefaultMode);

    CFFileDescriptorInvalidate(fdref);
    CFRelease(fd_rl_src);
    CFRelease(fdref);

    fd_rl_src = NULL;
    fdref   = NULL;
}

static void* event_loop(void*)
{
    loop = CFRunLoopGetCurrent();
    FSEventStreamRef      stream_ref = NULL;
    FSEventStreamEventId sinceWhen = kFSEventStreamEventIdSinceNow;
    setup_run_loop_pipe_read_handler();

    do {
        stream_ref = FSEventStreamCreate(kCFAllocatorDefault,
                                    (FSEventStreamCallback)&fsevents_callback,
                                    NULL,
                                    g_paths,
                                    sinceWhen,
                                    0.3,
                                    kFSEventStreamCreateFlagNone);
        FSEventStreamScheduleWithRunLoop(stream_ref, loop, kCFRunLoopDefaultMode);
        if (!FSEventStreamStart(stream_ref))
        {
            std::cout << "Failed to start the FSEventStream" << std::endl;
            return NULL;
        }
        std::cout << "starting runloop.." << std::endl;

        CFRunLoopRun();
        std::cout << "CFRunLoopRun waken up" << std::endl;
        FSEventStreamFlushSync(stream_ref);
        FSEventStreamStop(stream_ref);
        FSEventStreamUnscheduleFromRunLoop(stream_ref, loop, kCFRunLoopDefaultMode);
        FSEventStreamInvalidate(stream_ref);
        FSEventStreamRelease(stream_ref);
        sinceWhen = FSEventsGetCurrentEventId();
    }
    while (!done);
    return NULL;
}

int dir_watch_Add(const char* path, PDirWatch& dirWatch)
{
    std::cout << path << std::endl;
    if (!initialized)
    {
        if(0 != pipe(pipes))
        {
            std::cout << "pipe failed" << std::endl;
            return -1;
        }
        g_paths = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        if (g_paths == NULL)
        {
            std::cerr << "ERROR: CFArrayCreateMutable() => NULL" << std::endl;
            return -1;
        }
        add_to_g_paths(path);

        if (pthread_create(&g_event_loop_thread, NULL, &event_loop, NULL))
        {
            std::cerr << "pthread_create failed" << std::endl;
            return -1;
        }
        atexit(fam_deinit);
        initialized = 1;
    }
    else
    {
        write(pipes[1], "1", 1);
        add_to_g_paths(path);
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
	std::vector<DirWatchNotification> polled_notifications;

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
	for (size_t i = 0; i < polled_notifications.size(); ++i)
    {
        notifications.push_back(polled_notifications[i]);
    }
    return 0;
}
