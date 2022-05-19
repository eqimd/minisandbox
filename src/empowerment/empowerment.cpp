#include <iostream>
#include <unistd.h>
#include <sys/capability.h>
#include <cstring>
#include <cassert>

#include "empowerment.h"


bool minisandbox::empowerment::set_uid() {
    uid_t ruid, euid, suid;
    errno = 0;
    if (getresuid(&ruid, &euid, &suid) == -1) {
        perror("set_uid: getresuid failed");
        return false;
    }

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

// debug only
static void show_capabilities(const char* filename) {
    errno = 0;
    cap_t caps = cap_get_file(filename);
    if (caps == NULL) {
        if (errno != ENODATA) {
            perror("show_capabilities.cap_get_file failed");
            return;
        }
        else {
            std::cout << "caps: " << std::endl;
        }
    }
    else {
        char* txt_caps = cap_to_text(caps, NULL);
        if (txt_caps == NULL) {
            cap_free(caps);
            perror("show_capabilities.cap_to_text failed");
            return;
        }
        else {
            std::cout << "caps: " << txt_caps << std::endl;
            if (cap_free(txt_caps) != 0 || cap_free(caps) != 0)
                perror("show_capabilities.cap_free failed");
            return;
        }
    }
    
    if (cap_free(caps) != 0)
        perror("show_capabilities.cap_free failed");
}

bool minisandbox::empowerment::set_capabilities(const char* filename) {
    // show_capabilities(filename);
    char txt_caps[] = "cap_net_raw=";
    errno = 0;
    cap_t caps = cap_from_text(txt_caps);
    if (caps == NULL) {
        perror("set_capabilities.cap_from_text failed");
        return false;
    }
    else {
        if (cap_set_file(filename, caps) == -1) {
            std::cerr << "Failed to set capabilities of file " << filename << ": " << std::strerror(errno) << std::endl;
            return false;
        }
        else {
            if (cap_free(caps) != 0)
                perror("set_capabilities.cap_free failed");
//             show_capabilities(filename);
        }
    }
    return true;
}
