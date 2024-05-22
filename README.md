# TCP over UDP Project

## Project Overview
This project implements a TCP-like reliable data transfer protocol over UDP in C++. The project includes both client and server applications. The client sends a file to the server, and you can enable debugging mode if needed.

## Table of Contents
- [Installation](#installation)
- [Usage](#usage)
  - [Running the Server](#running-the-server)
  - [Running the Client](#running-the-client)
- [Performance](#performance)
- [Documentation](#documentation)
- [License](#license)

## Installation
1. Ensure you have a C++ compiler and `make` installed on your system.
2. Clone the repository to your local machine.
3. Navigate to the project directory.
4. Run `make` to compile the project.

```bash
make
```

## Usage

### Running the Server
Start the server using the following command:
```bash
./bin/server1-Thehunters Port
```
Replace `Port` with the port number you want the server to listen on.

### Running the Client
Start the client using the following command:
```bash
./bin/client2 ServerIP ServerPort FiletoTransfer '0 or 1'
```
- `ServerIP`: The IP address of the server.
- `ServerPort`: The port number the server is listening on.
- `FiletoTransfer`: The path to the file you want to transfer.
- `'0 or 1'`: Set to `1` to enable debugging mode, or `0` to disable it.

Example:
```bash
./bin/client2 192.168.1.1 8080 example.txt 1
```

## Performance
On a network with a maximum speed of 10MB/s, this implementation achieved a transfer speed of 8.5MB/s.

## Documentation
For more detailed information, please refer to the project presentation [here](pres/The%20Hunters.pdf).

## License
This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for more details.

---
