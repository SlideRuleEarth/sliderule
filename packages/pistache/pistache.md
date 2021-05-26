See https://github.com/pistacheio/pistache for the latest instructions on how to install.

## Dependencies

Update system packages needed to build the code:
```bash
$ sudo apt install libssl-dev meson
```

## Install Pistache Library from Repository (Ubuntu Only)

```bash
$ sudo add-apt-repository ppa:pistache+team/unstable
$ sudo apt update
$ sudo apt install libpistache-dev
```

## Install Pistache Library from Source

```bash
$ git clone https://github.com/pistacheio/pistache.git
$ cd pistache
$ meson setup build
$ meson install -C build
```
