# What is the context?

Future is the main library that has two parts: promises and future where promise ensures there will be a write and future ensures the read of that write. So, the ultimate goal is to pass values securely between threads. Securely means there will be no data race, value alteration ensuring synchronization.

## Why not pass them as normal parameters to the threads?

Promises and futures share the same state. If we copy them there will be a chance for data alteration and improper synchronization.
