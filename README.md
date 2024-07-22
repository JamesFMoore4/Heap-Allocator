Heap allocator project from 'Computer Systems: A Programmer's Perspective'. This is essentially an extensively modified version of the example allocator created in chapter 9. Malloc is used to receive a large block of memory, which is then managed by the my_alloc and my_free functions. The allocator uses a segregated free list and a deferred coalescing strategy.

Block format:

Allocated:
---------------------------------------
| Block size (29 bits) | a/f (3 bits) |
---------------------------------------
|                                     |
|                Payload              |
|                                     |
---------------------------------------
|	    Padding (optional)        |
---------------------------------------
| Block size (29 bits) | a/f (3 bits) |
---------------------------------------
Minimum block size is 16 bytes

Free:
---------------------------------------
| Block size (29 bits) | a/f (3 bits) |
---------------------------------------
|       Successor Pointer (8 bytes)   |
---------------------------------------
| Block size (29 bits) | a/f (3 bits) |
---------------------------------------
Minimum block size is 16 bytes
