CS 6210: ADVANCED OPERATING SYSTEMS






PROJECT 4: Recoverable Virtual Memory










Team Members:


Anouksha Narayan (anarayan33@gatech.edu)
Nikita Mundada (mundada.nikita@gatech.edu)






________________


Q. How you use log files to accomplish persistency plus transaction semantics? What goes in them? How do the files get cleaned up, so that they do not expand indefinitely?


A. We keep redo log files for each segment on the disk in order to ensure persistency of data structures. We also have an undo record in memory,  that has the original data at the location being modified. In case a transaction commits, we write the new memory value along  with its offset to the redo log corresponding to that particular segment. In case a transaction aborts, we re-write the  original value from the undo record to the corresponding memory location (that the transaction was modifying). When a program calls map() without having called unmap() previously on an existing  segment, the library returns an error.   If the program wakes up from a crash, then there would be a segment file for it on disk but no record of it in memory. In this case, the program checks if there is a log file corresponding to that segment on the disk. If there is such a file, it applies  changes in that log file to the segment on the disk making the segment up-to-date and the library then reads the current data in  the segment to memory. It compares the current size of the segment on the disk to the size being passed by the program to map(). If the current size is smaller then, it expands the size of the segment on the disk to accommodate the extra demand for space. If the program is calling map() for the first time, then an in memory copy as well as a file on the disk is created for that segment.


There are two instances where the log files get cleaned up on the disk:


1. In map(), before mapping the segment, if a redo log file exists for it then it first flushes the changes to the segment on the disk and then continues with the mapping. This prevents the log file from expanding indefinitely and also ensures that the segment has up-to-date data after recovering from a crash.


2. The programmer can explicitly call truncate_log() to flush out all changes in each redo log file on the disk to its corresponding segment. It’s up to the programmer to periodically call truncate_log() to flush out all changes in one go.
