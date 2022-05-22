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
    "ram_limit": 102400,
    "stack_size": 102400,
    "time_limit": 60,
    "fork_limit": 5
}
```