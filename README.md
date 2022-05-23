## Simple linux sandboxing tool

### Usage
Run with superuser privileges:
```bash
./minisandbox <config path>
```

### Building
You need to install next libraries to build minisandbox by yourself
```bash
sudo apt-get install nlohmann-json3-dev
sudo apt-get install libseccomp-dev
sudo apt-get install libcap-dev
sudo apt-get install libgtest-dev
```

### Config sample
```json
{
    "executable": "ubuntu-rootfs/bin/bash",
    "argv": [""],
    "envp": [""],
    "rootfs": "ubuntu-rootfs",
    "perm_flags": 0,
    "ram_limit": 100000000,
    "stack_size": 100000000,
    "time_limit": 1000,
    "fork_limit": 5,
    "priority": 0,
    "io_priority": 0
}
```