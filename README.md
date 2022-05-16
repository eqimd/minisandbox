## Simple linux sandboxing tool

### Usage
Run with superuser privileges:
```bash
./minisandbox <config path>
```

### Building
You need to install `nlohmann-json` and `boost` libraries. For Debian-based distributives, simply run
```bash
sudo apt-get install nlohmann-json3-dev
sudo apt-get install libboost-all-dev
```

### Config sample
```json
{
    "executable": "ubuntu-rootfs/bin/bash",
    "argv": "",
    "envp": "",
    "rootfs": "ubuntu-rootfs",
    "perm_flags": 0,
    "ram_limit": 10240,
    "stack_size": 10240,
    "time_limit": 60
}
```