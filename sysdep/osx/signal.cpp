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

static void kq_cffd_callback(CFFileDescriptorRef kq_cffd, CFOptionFlags callBackTypes, void *)
{
    std::cout << "kq_cffd_callback" << std::endl;
    int kq = CFFileDescriptorGetNativeDescriptor(kq_cffd);
    if (kq < 0)
        return;
    struct kevent event;
    struct timespec timeout = {0, 0};
    // consume the kevent
    while (kevent(kq, NULL, 0, &event, 1, &timeout) > 0)
    {
        std::cout << "kevent pulled out" << std::endl;
    }
    // (Re-)Enable a one-shot (the only kind) callback
    CFFileDescriptorEnableCallBacks(kq_cffd, kCFFileDescriptorReadCallBack);
}

static int setup_run_loop_signal_handler()
{
    struct kevent           kev[4];
    signal(SIGINT, SIG_IGN);

    int kq_fd = kqueue();
    if (kq_fd < 0) 
    {
        std::cout << "kqueue failed" << std::endl;
	    return -1;
    }

    EV_SET(&kev[0], SIGINT,  EVFILT_SIGNAL, EV_ADD, 0, 0, 0);
    EV_SET(&kev[1], SIGQUIT, EVFILT_SIGNAL, EV_ADD, 0, 0, 0);
    EV_SET(&kev[2], SIGTERM, EVFILT_SIGNAL, EV_ADD, 0, 0, 0);
    EV_SET(&kev[3], SIGHUP,  EVFILT_SIGNAL, EV_ADD, 0, 0, 0);

    if (kevent(kq_fd, &kev[0], 4, NULL, 0, NULL) != 0) 
    {
        std::cout << "kevent failed" << std::endl;
        close(kq_fd);
        return -1;
    }

    CFFileDescriptorRef kq_cffd = CFFileDescriptorCreate(NULL, kq_fd, true, kq_cffd_callback, NULL);
    if (kq_cffd == NULL)
    {
        std::cout << "CFFileDescriptorCreate" << std::endl;
        close(kq_fd);
        return -1;
    }

    CFRunLoopSourceRef kq_rl_src = CFFileDescriptorCreateRunLoopSource(NULL, kq_cffd, (CFIndex)0);
    if (kq_rl_src == NULL)
    {
        std::cout << "CFFileDescriptorCreateRunLoopSource" << std::endl;
        // Dispose the kq_cffd
        CFFileDescriptorInvalidate(kq_cffd);
        CFRelease(kq_cffd);
        kq_cffd = NULL;

        return -1;
    }

    CFRunLoopAddSource(CFRunLoopGetCurrent(), kq_rl_src, kCFRunLoopDefaultMode);
    CFFileDescriptorEnableCallBacks(kq_cffd, kCFFileDescriptorReadCallBack);

    std::cout << "setup_run_loop_signal_handler OK" << std::endl;
    return 0;
}


int main(int argc, char* argv[])
{
    setup_run_loop_signal_handler();
    CFRunLoopRun();
    return 0;
}
