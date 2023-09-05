# C UDP Sockets
UDP sockets in C, implementing the Go-back-N protocol for reliable transfer.

Sends a specified amount of bytes of a file to the receiver.

`make` to compile the files and run the executables in two seperate terminals.

## Usage

`./reliable_receiver $port $filename`
`./reliable_sender $receiver_name $receiver_port $filename $num_bytes_to_transfer`