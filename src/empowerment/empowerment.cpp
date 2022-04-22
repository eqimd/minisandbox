#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <linux/capability.h>
#include <sys/syscall.h>
#include <sys/prctl.h>
#include <sys/capability.h>
#include <string>
#include <cstring>
#include <cassert>
#include <unordered_set>
#include <fstream>
#include <filesystem>
#include <exception>

using std::string;
using std::cout;
using std::cerr;
using std::endl;
using std::strerror;
using std::unordered_set;
using std::ifstream;
using std::runtime_error;
using std::filesystem::path;
using std::filesystem::equivalent;

// fix all later
unordered_set<string> get_not_allowed(string filename) {
    if (std::system("find . -perm /4000 > not_allowed.txt") != 0) {
        throw runtime_error("run find error");
    }
    unordered_set<string> not_allowed;
    ifstream not_allowed_files("not_allowed.txt");
    if (!not_allowed_files.is_open()) {
        throw runtime_error("run find error");
    }
    string file;
    while (not_allowed_files >> file) 
        not_allowed.insert(file);
    if (std::system("rm not_allowed.txt") != 0) {
        throw runtime_error("run find error");
    }
    return not_allowed;
}

// run it after start
bool set_uid() {
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

bool set_capabilities(string filename) {
    errno = 0;
    if (cap_set_file(filename.c_str(), NULL) == -1) {
        cerr << "Failed to set capabilities of file " << filename << ": " << strerror(errno) << endl;
        return false;
    }
    return true;
}


bool uid_permisson(string filename) {
    path fpath = path(filename);
    unordered_set<string> not_allowed;
    try {
        not_allowed = get_not_allowed(fpath.parent_path().c_str());
    }
    catch (const runtime_error& e) {
        cerr << e.what() << endl;
        return false;
    }
    for (path p : not_allowed) {
        if (equivalent(p, fpath)) {
            cout << "Run file " << filename << " not allowed (security) because of the suid" << endl;
            return false;
        }
    }
    return true;
}

bool capabilities_permission(string filename) {
    cap_t cap;
    errno = 0;
    cap = cap_get_file(filename.c_str());
    if (cap == nullptr) {
        if (errno == ENODATA) {
            return true;    // not capabilities
        }
        else {
            cerr << "Failed to get capabilities of file " << filename << ": " << strerror(errno) << endl;
            return false;
        }
    }
    bool ret = true;
    char* res = cap_to_text(cap, nullptr);
    if (!res) {
        cerr << "Failed to get capabilities of readable format at " << filename << ": " << strerror(errno) << endl;
        ret = false;
    }
    else if (string(res) != "=") {
        cout << "Run file " << filename << " not allowed (security), exists capabilities: " << res <<  "." << endl;
        ret = false;
    }
    cap_free(res);
    return ret;
}

bool run_permission(string filename) {
    return uid_permisson(filename) && capabilities_permission(filename);
}


int main(int argc, char *argv[]) {
    assert(argc == 2);
    cout << run_permission(argv[1]) << endl;
    return 0;
}
