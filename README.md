`syxlog` is a Mac command line utility that listens to all the connected MIDI inputs and prints ASCII strings to the standard output in response to specific SysEx messages containing them.

## Accepted SysEx string format

To be printed to the standard output, the string must be sent as a SysEx with the following format:

```
0xF0 0x70 <ASCII bytes> 0xF7
```

We use manufacturer ID `0x70` because it is not likely to be used for other purposes.
You are free to choose another byte by changing the constant `sysex_id_byte` in your code.

## How to use

First, compile the program:

```
make
```

Then, launch it:

```
./syxlog
```

Press _Ctrl+C_ to exit.
