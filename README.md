![stars](https://img.shields.io/github/stars/shyweeds/webserver?style=social)
![CI](https://github.com/shyweeds/webserver/actions/workflows/ci.yml/badge.svg)
# WebServer
A lightweight Linux-based multithreaded WebServer.
This [project](./Project.md) is mainly for learning and practice.

---

## Project Features

### HTTP Support

- HTTP/1.0 and HTTP/1.1: GET only
- Supports `.cgi` files and one environment variable
- Supports static file responses
- Supports 404 page handling

### Concurrency Model

- Fixed-size thread pool for request handling
- Main thread responsible for `accept`
- Worker threads responsible for HTTP request processing

### Engineering

- Built with Makefile
- Bash-based CI automated testing
- Modular directory structure
- Supports stress testing

---

# Project Structure

```text
.
├── .github/
├── bin/            #.gdbinit here & files for server
├── include/         
├── src/         
├── tests/          # Test scripts and examples
├── .clang-format
├── .gitignore
├── Makefile
├── Project.md
└── README.md
```

---

# Building the Project

## Requirements

- Linux
- gcc
- make

Recommended:

- Arch Linux
- gcc 16+

---

## Build

```bash
make
```

---

# Running the Server

Example:

```bash
./wserver -d ./basedir/ -p 8003 -t 8 -b 16 -s FIFO
```

Parameter descriptions:

| Parameter | Meaning |
|---|---|
| `-p` | Listening port |
| `-d` | Root directory |
| `-t` | Number of threads |
| `-b` | Number of buffers |
| `-s` | Scheduling policy |

---

# Access Testing

In a browser:

```text
http://127.0.0.1:port/a.html
```

Or:

```bash
./wclient localhost 8003 /notfind.html
```

Parameter description:

Usage: `./wclient <host> <port> <filename>`

---

# Testing

Run the CI tests:

```bash
make test
```

Test coverage includes:

- Static file tests
- CGI tests
- 404 tests
- Concurrent request tests (static and `.cgi` files)

---

# Development Goals

Current goals:

- [x] Return static files in single-threaded mode
- [x] Return `.cgi` processing results in single-threaded mode
- [x] Return static files in multithreaded mode
- [x] Return `.cgi` files in multithreaded mode
- [ ] Support SFF (theoretically supported, not yet tested)

---

# Learning Topics

This project is well suited for learning:

- Linux system programming
- Socket network programming
- HTTP protocol
- Reactor model
- High-performance servers

---

# License

MIT License
