#include <iostream>
#include <unistd.h>
#include <sys/capability.h>
#include <cstring>
#include <cassert>

#include "empowerment.h"


void minisandbox::empowerment::drop_privileges() {
    gid_t gid;
	uid_t uid;

	// no need to "drop" the privileges that you don't have in the first place!
	if (getuid() != 0) {
		return;
	}

	// when your program is invoked with sudo, getuid() will return 0 and you
	// won't be able to drop your privileges
	if ((uid = getuid()) == 0) {
		const char *sudo_uid = secure_getenv("SUDO_UID");
		if (sudo_uid == NULL) {
			throw std::runtime_error(
                "Environment variable `SUDO_UID` not found."
            );
		}
		errno = 0;
		uid = (uid_t) strtoll(sudo_uid, NULL, 10);
		if (errno != 0) {
            throw std::runtime_error(
                "Under-/over- flow in converting `SUDO_UID` to integer."
            );
		}
	}

	// again, in case your program is invoked using sudo
	if ((gid = getgid()) == 0) {
		const char *sudo_gid = secure_getenv("SUDO_GID");
		if (sudo_gid == NULL) {
            throw std::runtime_error(
                "Environment variable `SUDO_GID` not found."
            );
		}
		errno = 0;
		gid = (gid_t) strtoll(sudo_gid, NULL, 10);
		if (errno != 0) {
            throw std::runtime_error(
                "Under-/over- flow in converting `SUDO_GID` to integer."
            );
		}
	}

    errno = 0;
	if (setgid(gid) != 0) {
        throw std::runtime_error(
            "Error calling setgid: " +
            std::string(strerror(errno))
        );
	}
	if (setuid(uid) != 0) {
        throw std::runtime_error(
            "Error calling setuid: " +
            std::string(strerror(errno))
        );
	}

	// check if we successfully dropped the root privileges
	if (setuid(0) == 0 || seteuid(0) == 0) {
        throw std::runtime_error(
            "Could not drop root privileges."
        );
	}
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
