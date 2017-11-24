# cchan
Go style channels in C.

# notes
I have not decided if his library will require a conservative garbage collector.

Use at your own risk, I have forgotten the state of this code, the ideas are correct, but the implementation might not be debugged.

The implementation is mostly inspired by the early go implementation of channels, one of the key ideas is using a sort by address to ensure locks are aquired in the correct order every time to avoid deadlock.
