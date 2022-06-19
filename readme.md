# Gaining efficiency in echo server performance by appropriate design and implementation choices

Author: [Yuriy Pasichnyk](https://github.com/Fenix-125)<br>

This repository is a part of a bachelor's thesis.

Thesis title: **Performance analysis of synchronous and asynchronous parallel network server implementations using the C++ language**


## Description

The main two paradigms for implementing parallel network servers are synchronous and asynchronous. After an overview of these methodologies and implementation choices, the most representative and valuable versions of a **stateful TCP echo server** were designed and implemented. The versions and their specification are listed below:

- **echo_server_simple_threaded** -- synchronous multithreaded
    * A separate thread per client is used.
    * A blocking I/O is used.
- **echo_server_simple** -- hybrid-synchronous single-threaded
    * The hybrid keyword is used to denote that an asynchronous syscall("poll syscall") is used.
    * A blocking I/O is used.
- **echo_server_custom_thread_pool** -- hybrid-synchronous multithreaded
    * The hybrid keyword is used to denote that an asynchronous syscall("poll syscall") is used.
    * Custom thread pool is used to distribute work between worker threads.
    * A blocking I/O is used.
- **echo_server_boost_asio** -- asynchronous single-threaded
    * For this implementation, the boost asynchronous lib was used.
    * A non-blocking I/O is used.
- **echo_server_boost_asio_threaded** -- asynchronous multithreaded
    * For this implementation, the boost asynchronous lib was used, too.
    * A non-blocking I/O is used.

All versions support **Google Logging**. The Logging in the not-debug compilation is reduced due to performance concerns.
The logging output is written to separate files in the newly created **./logs** directory.

### Server Requirements

- Send back received data from the client
- Hold the client session until the client terminates it
- Use TCP as the transport level protocol


## Prerequisites

The requirements for **apt** and **apk** Linux packet managers are listed in the corresponding files in the **dependencies** directory.
An example how to install dependencies you can find below:

```{bash}
$ apt update && apt upgrade 
$ xargs apt install -y << ./dependencies/apt.txt 
```

## Compilation

The compilation is automated using the **compile.sh**. Please refer to the help of the script via the **-h** option.
To compile all the versions with optimization and install them to the **./bin** directory (created automatically), use the command below:

```{bash}
$ bash ./compile.sh
```

## Usage

The compiled executables can be run in normal user mode as shown below for differnt versions:

```{bash}
$ ./echo_server_simple_threaded
$ ./echo_server_simple
$ ./echo_server_custom_thread_pool
$ ./echo_server_boost_asio
$ ./echo_server_boost_asio_threaded
```

## Testing description

The perfomance testing of this versions is done using the [Fortio](https://github.com/fortio/fortio) opern source testing tool with parameters listed below:

- **"-qps 0"** 	-- try to send maximum number of queries per second
- **"-t 60 s"** 	-- test duration 60 seconds
- **"-c <client-num>"** 	-- client number parameter
- **"-payload-size 64"** -- set the client message size
- **"-uniform"** 	-- de-synchronize parallel clients’ requests uniformly

The server was run on a PC with characteristics listed below:
  
| Characteristic | Value |
|----------------|-------|
|CPU Architecture| x86_64|
|CPU Model name  | 11th Gen Intel(R) Core(TM) i5-1135G7 @ 2.40GHz|
|Logical CPUs    | 8     |
|Physical CPUs   | 4     |
|CPU max MHz     | 4200  |
|CPU min MHz     | 400   |
|CPU Byte Oserver| Little Endian |
|L1d cache       | 192 KiB (4 instances)|
|L1i cache       | 128 KiB (4 instances)|
|L2 cache        | 5 MiB (4 instances)  |
|L3 cache        | 8 MiB (1 instance)   |
|RAM             | 12.0 GB |
|RAM type        | DDR4 SDRAM |
| OS             | Ubuntu 20.04 LTS |
| OS Kernel      | Linux Kernel 5.4 |
| NIC            | Gigabit Ethernet LAN |
  
The load for the server was generated from 3 PCs, which were interconnected with a Gigabit Ethernet network using a Cisco Switch.
The metrics that we used to measure the performance are:

- Connected clients number
- Throughput
- Latency
- CPU consumption
- Memory consumption

## Performance Visualizations

Below you can see visualizations of data collected from the Fortio load tests.

### Throughput in respect to clients number
![chart-throughput-1](https://user-images.githubusercontent.com/44115554/174493902-5a443db8-d388-4cb6-a8c6-ce589cf2a761.png)

### Average latency in respect to clients number
  ![chart-avg-latency-1](https://user-images.githubusercontent.com/44115554/174493923-57d92a2c-208e-4d07-ac15-c45740871067.png)

### 90 percentile latency in respect to clients number
![chart-latency-persentile-90-1](https://user-images.githubusercontent.com/44115554/174493951-23898f1b-536d-4f7d-bf7a-9d6e254199d0.png)

### 99 percentile latency in respect to clients number
![chart-latency-persentile-99-1](https://user-images.githubusercontent.com/44115554/174493947-d4dfbf48-7524-4aec-9c6d-a471fd1159d1.png)
  
### 99.9 percentile latency in respect to clients number
![chart-latency-persentile-999-1](https://user-images.githubusercontent.com/44115554/174493954-b26e2221-c3ca-4e82-8f15-374c01f509cb.png)

### CPU usage in respect to clients number
![chart-cpu-1](https://user-images.githubusercontent.com/44115554/174493984-806543e4-73eb-4938-9623-08e9dd1c6a68.png)

### Memory usage in respect to clients number
![chart-mem-1](https://user-images.githubusercontent.com/44115554/174493981-ebd5d198-09f8-4e67-a6eb-0a2a472d3cf3.png)
