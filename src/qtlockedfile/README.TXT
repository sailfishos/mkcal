Locked File v2.4

The QtLockedFile class extends QFile with advisory locking
functions.

This class extends the QFile class with inter-process file locking
capabilities. If an application requires that several processes
should access the same file, QtLockedFile can be used to easily
ensure that only one process at a time is writing to the file, and
that no process is writing to it while others are reading it.

