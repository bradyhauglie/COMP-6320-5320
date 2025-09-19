# Programming Assignment 1
## Compilation of code:
### Lab 1-1 (Make sure you are in PA1-1)
```
gcc server11.c -o server11
gcc client11b.c -o client11b
gcc client11c.c -o client11c
```
### Lab 1-2 (Make sure you are in PA1-2)
```
gcc server12.c -o server12
gcc client12.c -o client12
```
## Running of Server and Client(s) (All run locally)
### Lab 1-1 (In PA1-1)
```
./server11
```
CTRL+z
```
bg [Job ID of server11]
./client11b localhost
```
(Type any message and press ENTER to send) \
CTRL+z (Client will still be bound to server)
```
./client11c localhost
```
(Numbers 1-10000 will be sent and client will end process after stats are printed) \
Kill any still running processes using:
```
kill %[Job ID of running process]
```
### Lab 1-2 (In PA1-2)
```
./server12
```
CTRL+z
```
bg [Job ID of server12]
./client12 [Operator] [Operand] [Operand]
```
Server will calculate the operation between the first and second operands and send the result to client.
