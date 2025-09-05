#!/bin/bash

# This script runs the full suite of performance tests for the client-server application.
# It logs data for both FCFS and RR policies into separate CSV files.

# Ensure old log files are cleared before starting
rm -f performance_data_fcfs.csv performance_data_rr.csv

# --- PART 1: FCFS TESTS ---
echo "========================================="
echo "  STARTING TESTS FOR FCFS (POLICY 1) "
echo "========================================="
./server 8080 1 performance_data_fcfs.csv &
SERVER_PID=$!
sleep 1 # Allow server a moment to start up

# A. Throughput vs. Message Size (1-32 KB)
echo "--- FCFS: Testing TCP Throughput (1-32 KB) ---"
for i in $(seq 1 32); do ./client 127.0.0.1 8080 tcp $i 1; done
echo "--- FCFS: Testing UDP Throughput (1-32 KB) ---"
for i in $(seq 1 32); do ./client 127.0.0.1 8080 udp $i 1; done

# B. Bulk TCP Transfer
echo "--- FCFS: Testing Bulk TCP Transfer (Total 1 MB & 10 MB) ---"
# 1MB
./client 127.0.0.1 8080 tcp 1 1024
./client 127.0.0.1 8080 tcp 16 64
./client 127.0.0.1 8080 tcp 64 16
# 10MB
./client 127.0.0.1 8080 tcp 40 256
./client 127.0.0.1 8080 tcp 80 128
./client 127.0.0.1 8080 tcp 160 64

# C. Bulk UDP Transfer
echo "--- FCFS: Testing Bulk UDP Transfer (Total 1 MB) ---"
./client 127.0.0.1 8080 udp 1 1024
./client 127.0.0.1 8080 udp 16 64
./client 127.0.0.1 8080 udp 32 32

# Stop the FCFS server
kill $SERVER_PID
sleep 1

# --- PART 2: RR TESTS ---
echo ""
echo "========================================="
echo "  STARTING TESTS FOR RR (POLICY 2) "
echo "========================================="
./server 8080 2 performance_data_rr.csv &
SERVER_PID=$!
sleep 1

# A. Throughput vs. Message Size (1-32 KB)
echo "--- RR: Testing TCP Throughput (1-32 KB) ---"
for i in $(seq 1 32); do ./client 127.0.0.1 8080 tcp $i 1; done
echo "--- RR: Testing UDP Throughput (1-32 KB) ---"
for i in $(seq 1 32); do ./client 127.0.0.1 8080 udp $i 1; done

# B. Bulk TCP Transfer
echo "--- RR: Testing Bulk TCP Transfer (Total 1 MB & 10 MB) ---"
# 1MB
./client 127.0.0.1 8080 tcp 1 1024
./client 127.0.0.1 8080 tcp 16 64
./client 127.0.0.1 8080 tcp 64 16
# 10MB
./client 127.0.0.1 8080 tcp 40 256
./client 127.0.0.1 8080 tcp 80 128
./client 127.0.0.1 8080 tcp 160 64

# C. Bulk UDP Transfer
echo "--- RR: Testing Bulk UDP Transfer (Total 1 MB) ---"
./client 127.0.0.1 8080 udp 1 1024
./client 127.0.0.1 8080 udp 16 64
./client 127.0.0.1 8080 udp 32 32

# Stop the RR server
kill $SERVER_PID
echo ""
echo "========================================="
echo "           ALL TESTS COMPLETE            "
echo "========================================="

# Note: The log files 'performance_data_fcfs.csv' and 'performance_data_rr.csv' will contain the results.
