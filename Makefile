# Compiler and flags
CXX=clang++
CXXFLAGS= -Wall -std=c++17 -pthread

# Source files
SERVER_SOURCES=server.cc message.cc
CLIENT_SOURCES=client.cc message.cc

# Header files (for dependency tracking)
HEADERS=server.hh client.hh message.hh

# Executables
SERVER=server
CLIENT=client

# Default target builds both server and client
all: $(SERVER) $(CLIENT)

# Build server executable
$(SERVER): $(SERVER_SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(SERVER) $(SERVER_SOURCES)

# Build client executable
$(CLIENT): $(CLIENT_SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(CLIENT) $(CLIENT_SOURCES)

# Clean files
clean:
	rm -f $(SERVER) $(CLIENT) *.o

.PHONY: all clean
