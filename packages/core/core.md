## Core Package Overview

The core package is written in C++ and provides a set of APIs for building science data processing applications. The primary building blocks of the library are: _Message Queues_ which implement a stream of data inside a process, _Record Objects_ which represent and provide direct access to the data inside a stream, and _Lua Objects_ which exist in the context of a Lua program and process the data.

The core library is kept as clean of dependencies as possible (only depending upon Lua), with the intention that extensions in functionality are provided through _packages_ (compile-time extensions) and _plugins_ (run-time extensions).

### Installing Support for LTTng

Tracing support in the core package is provided through the _Linux Tracing Toolkit: next generation (LTTng)_ framework.  See https://lttng.org/ for the latest installation instructions and documentation.

For convenience, the following steps can be used to install LTTng on Ubuntu 20.04:
```bash
# Install Dependencies
$ sudo apt install uuid-dev libpopt-dev liburcu-dev libxml2-dev libnuma-dev
# Install LTTng-modules
$ cd $(mktemp -d) &&
wget http://lttng.org/files/lttng-modules/lttng-modules-latest-2.12.tar.bz2 &&
tar -xf lttng-modules-latest-2.12.tar.bz2 &&
cd lttng-modules-2.12.* &&
make &&
sudo make modules_install &&
sudo depmod -a
```
```bash
# Install LTTng-UST
$ cd $(mktemp -d) &&
wget http://lttng.org/files/lttng-ust/lttng-ust-latest-2.12.tar.bz2 &&
tar -xf lttng-ust-latest-2.12.tar.bz2 &&
cd lttng-ust-2.12.* &&
./configure &&
make &&
sudo make install &&
sudo ldconfig
```
```bash
# Install LTTng-tools
$ cd $(mktemp -d) &&
wget http://lttng.org/files/lttng-tools/lttng-tools-latest-2.12.tar.bz2 &&
tar -xf lttng-tools-latest-2.12.tar.bz2 &&
cd lttng-tools-2.12.* &&
./configure &&
make &&
sudo make install &&
sudo ldconfig
```

### Core Library Modules

| **_Layer_** | **_Modules_** | **_Description_** |
| ----- | ------ | ----------- |
| Operating System | Thread, Mutex, Sem, Cond, Timer, LocalLib, SockLib, TTYLib | **Platform agnostic abstraction of operating system primitives.**  Each platform sliderule runs on has a specific implementation of these interfaces.  By convention, subsequent layers expose these interfaces by including "OsApi.h". |
| Data Structure | List, Dictionary, Ordering | **Generic data structure implementations.**  This layer aims to be a minimalistic replacement for the STL, Qt containers, boost, etc.  Having only a few data structures minimizes coding patterns.  Having these data structures open in the code allows for unique tailoring to specific performance needs and supports more consistent application behavior across platforms. Modules are not thread safe and require higher level locking for concurrent use. |
| Library | LogLib, StringLib, TimeLib | **Common utility routines.** These modules implement thread safe routines that are commonly used throughout the framework and merit a central implementation. |
| Messaging | [MsgQ](MsgQ.md), RecordObject | **Data objects and data streams.**  The _MsgQ_ class implements a process level (i.e. inter-thread) message queue system for attaching producers and consumers to a stream of data.  The _RecordObject_ class implements a container around raw data which provides access and (de)serialization methods. |
| Application | LuaEngine, LuaObject | **Base classes for project specific code.**  The _LuaEngine_ class is the central wrapper for an instantiation of a running Lua interpreter. The _LuaObject_ is the base class for any object that is accessible from a Lua program executing within the LuaEngine. |
| Devices | DeviceObject, DeviceIO, DeviceReader, DeviceWriter, File, ClusterSocket, TcpSocket, Uart, UdpSocket | **Devices for stream based I/O.**  All devices implement a standard interface for sending and receiving data to and from an I/O device. The _DeviceObject_ class is a pure virtual class that defines a device interface to be used by a _DeviceReader_ and _DeviceWriter_ object. Any _DeviceReader_ and _DeviceWriter_ can use any DeviceObject. |
| Record Dispatches | RecordDispatcher, RecordDispatch, CaptureDispatch, LimitDispatch, MetricDispatch, PublisherDispatch, ReportDispatch | **Dispatches for processing stream data.**  The _RecordDispatcher_ class implements a data consumer and thread pool which subscribes to a stream and manages dispatching messages to _RecordDispatch_ objects running in available threads. Each _RecordDispatch_ can be attached to a _RecordDispatcher_ and run by that dispatcher to process data read from a stream. |
