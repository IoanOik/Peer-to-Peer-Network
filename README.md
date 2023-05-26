# Peer-to-Peer Network for Messaging and File Sharing

This project implements a peer-to-peer network that enables messaging and file sharing among multiple peers. The network utilizes a centralized sequencer for message broadcasting and file sharing coordination.

## Features

- Peer-to-peer messaging: Peers can send messages to each other through the centralized sequencer.
- File sharing: Peers can share files with each other by sending the file contents through the sequencer.
- Synchronization: Threads are used to handle message receiving and sending, ensuring proper synchronization and ordering of messages.
- Causality tracking: Vector clocks are used to track causal relationships between messages, maintaining consistency across peers.
- Asynchronous network: The network is designed to work in an asynchronous network environment, handling delays and potential message reordering.

## Usage

1. Clone the repository: `git clone https://github.com/.git`
2. Build the project: [Provide instructions for building the project, including any dependencies]

    2.1. Build the Sequencer: gcc sequencer.c DataStructs/List/LinkedList.c DataStructs/Node/Node.c -o sequencer.exe
    
    4.2. Build the peer: gcc peer.c DataStructs/List/LinkedList.c DataStructs/Node/Node.c -o peer

3. Run the sequencer: ./sequencer.exe
4. Run the peers: ./peer.exe [sequencer ip address] [sequencer port]
5. Interact with the network: Follow the instuctions provided by the command line interface on peer program

## Code Structure

The codebase is organized as follows:

- `sequencer.c`: Implementation of the centralized sequencer program.
- `peer.c`: Implementation of the peer program for message receiving and sending.
- `DataStructs`: Implementation of the basic data structure elements required for this project implementation.
