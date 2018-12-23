# cpu-resource-container
A resource container running on kernel mode for sharing CPU resource.

## Step
### Install Ubuntu
To run this project, you will need to set up a machine or a virtual machine with clean Ubuntu 16.04 installation. 

### Kernel Compilation
```shell
cd kernel_module
sudo make
sudo make install
cd ..
```

### User Space Library Compilation
```shell
cd library
sudo make
sudo make install
cd ..
```

### Benchmark Compilation
```shell
cd benchmark
make
cd ..
```

### Run
```shell
./test.sh <num_of_containers> [<num_of_task_for_container1> ...]

# example
./test.sh 1 2
./test.sh 2 2 4
```

## Functions
The following features have been implemented:

    - create: support create operation that creates a container if the corresponding cid hasn't been assigned yet, and assign the task to the container. These create requests are invoked by the user-space library using ioctl interface. The ioctl system call will be redirected to `process_container_ioctl` function located in `src/ioctl.c`

    - delete: support delete operation that removes tasks from the container. If there is no task in the container, the container should be destroyed as well. These delete requests are invoked by the user-space library using ioctl interface. The ioctl system call will be redirected to `process_container_ioctl` function located in `src/ioctl.c`

    - switch: support Linux process scheduling mechanism to switch tasks between threads.

    - lock/unlock: support locking and unlocking that guarantees only one process can access an object at the same time. These lock/unlock functions are invoked by the user-space library using ioctl interface. The ioctl system call will be redirected to `process_container_ioctl` function located in `src/ioctl.c`

