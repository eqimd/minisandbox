#include <iostream>
#include <unistd.h>
#include <sys/capability.h>
#include <cstring>
#include <cassert>

#include "empowerment.h"

using std::cout;
using std::cerr;
using std::endl;
using std::strerror;

bool set_uid() {
    uid_t ruid, euid, suid;
    errno = 0;
    if (getresuid(&ruid, &euid, &suid) == -1) {
        perror("set_uid: getresuid failed");
        return false;
    }
    std::cout << "empower " << ruid << " " << euid << " " << suid << "\n";  //

    while (setresuid(-1, ruid, ruid) == -1) {
        if (errno == EAGAIN) {
            errno = 0;
            continue;
        }
        perror("set_uid: setresuid failed");
        return false;
    }
    return true;
}

static void show_capabilities(const char* filename) {
    errno = 0;
    cap_t caps = cap_get_file(filename);
    if (caps == NULL) {
        if (errno != ENODATA) {
            perror("show_capabilities.cap_get_file");
            return;
        }
        else {
            cout << "caps: " << endl;
        }
    }
    else {
        char* txt_caps = cap_to_text(caps, NULL);
        if (txt_caps == NULL)
            perror("show_capabilities.cap_to_text");
        else {
            cout << "caps: " << txt_caps << endl;
            if (cap_free(txt_caps) != 0)
                perror("cap_free");
        }
    }
    
    if (cap_free(caps) != 0)
        perror("cap_free");
}

bool set_capabilities(const char* filename) {
    // show_capabilities(filename);

    bool ret = true;
    char txt_caps[] = "cap_net_raw=";
    errno = 0;
    cap_t caps = cap_from_text(txt_caps);
    if (caps == NULL) {
        perror("set_capabilities.cap_from_text");
        return false;
    }
    else {
        uid_t ruid, euid, suid;
        errno = 0;
        if (getresuid(&ruid, &euid, &suid) == -1) {
            perror("set_uid: getresuid failed");
            return false;
        }
        if (cap_set_file(filename, caps) == -1) {
            cerr << "Failed to set capabilities of file " << filename << ": " << strerror(errno) << endl;
            ret = false;
        }
        else {
            if (cap_free(caps) != 0)
                perror("cap_free");
            show_capabilities(filename);
        }
    }
    return ret;
}
