# Phase 1-3
## Data Structures:

The whole program depends on four different data structures that corresponds to
these blocks: superblock, FAT, root, and a file table. Choosing which data type
was fairly simple as 8 bits corresponds to 1 byte thus each data needed will
depend on how many bytes it needs. The superblock, FAT, and root all correspond
to the project specification. The superblock has these data types for each
description: char for the signature, int16_t for total blocks, root directory
index, data block index, and total amount of data blocks, int8_t for number of
FAT blocks, and the rest of the 4079 is just an array of uint8_t for padding.
For FAT we used a simple dynamically allocated array of uint16_t that is
malloced by BLOCK_SIZE * numFATBlocks to allocate enough memory for each FAT
block. The root directory has a char array of size FS_FILENAME_LEN (16 bytes),
uint32_t for file size, uint16_t for the first data block index, and the rest is
an array of uint8_t for padding. The last struct is for the file table that
contains a 2D array to hold the files and its index will be the file descriptor.
It also contains an int for the current size of the table and another array for
an offset that is indexed by the file descriptor of the file. 

The difficulty of the project revolves around these four data structures and how
they each communicate with one another. It is imperative that we understand it
and that is why we tried to simplify certain data types like using char for
the filename. 

## Functions:

Once the data structures were established, working on the first three phases was
fairly simple. Phase 1 handles mounting and unmounting. For mounting,
block_read() was used for each block: the superblock, FAT, and root. The
superblock must be read first since it contains the indices of the other blocks.
Once the superblock is read, we can dynamically allocate the FAT data structure
based on the numFATBlocks variable contained within the superblock which is then
looped with block_read() to read the contents of the FAT blocks inside the disk.
Then the root is simply read from the disk by the root index within the
superblock. Fs_unmount is the complete opposite of mount, instead of
block_read(), block_write() is used to write all the meta data info back into
the disk. Fs_info() simply displays the proper information inside these data
structures.

Fs_create() loops through the entire root directory to find an empty space (if
the filename is a NULL character or 0). Once found we initialize it with the
provided filename, a file size of 0, and FAT_EOC for the start index.
Fs_delete() is similar to create but must go through the FAT and data blocks and
clears the data within them that corresponds to the file. This is done through a
while loop that looks through the entries of the FAT table until it finds a
FAT_EOC and then clears the entry and the data block that corresponds to it.
Fs_ls() loops through the entire root directory for valid entries and displays
their information. 

Phase 3 now goes into the use of the file table data structure. Fs_open() goes
through the filetable and looks for an available entry to open the file. Once
found, it will give the index of the 2D array which corresponds to the file
descriptor for it. It will also initialize the offset to be 0 and then increase
the size of the file table. Fs_delete() is the opposite and will simply use the
given file descriptor to make the entry the NULL character to close the file and
then decrement the size. Fs_stat() will go to the entry array to get the
filename. With that filename, we loop through the root to find it and then give
its file size from the root struct. Fs_lseek will use the file descriptor as an
index for the offset array within the file table struct and then set the value
as the given offset.

# Phase 4
## Read

First, the name of the file and file offset is retrieved from the file table by
using the file descriptor that is given; with the file name, the file’s first
data block index and the file size are retrieved from the root. This is all the
information needed to read the file. Next, the starting location to be read from
is calculated with the offset and the number of bytes in a block. The index is
calculated by the sum of the original starting index and the floor of the
quotient of the offset and the BLOCK_SIZE (dataIndex + (offset/BLOCK_SIZE). The
starting location in a block is calculated by the difference between BLOCK_SIZE
and the mod of the offset and BLOCK_SIZE (BOCK_SIZE - (offset%BLOCK_SIZE)). This
is the ensure that the file is read from the correct location if the offset is
greater than BLOCK_SIZE. Then, the file is read one byte at a time in a while
loop that exits if the count or the number of bytes left in the file reaches 0.
This is to signify the end of the read. In the loop, we also check the number of
bytes left in the data block being read from. In the case that we reach the end
of a block, we get the index of the next data block from the FAT; then the whole
next block is read one byte at a time. This process repeats until the end of the
file or when count equals 0. 

## Write
The structure for fs_write is very similar to fs_read. Once all the information
about the file is retrieved from the file table and root, we check that the
index of the data block to be written to equals FAT_EOC. If so, then it
signifies that this is a new file being written to the file system and an empty
data block needs to be located. The FAT is searched through to find an entry
that equals 0. The entry is then changed to FAT_EOC and the index of the entry
is returned as the index of the new data block to be written into. The adjusted
data block index and starting location are calculated in the same way as
fs_read. Next, the file is written into the file system one byte at a time in a
loop until the number of bytes to be written reaches 0. To write, we have to
read the whole data block, alter the data block, then write the whole data black
back into the filesystem. When we reach the end of a data block, we either have
to continue writing into the file's next data block or possibly extending the
file by writing into an available data block. If we are writing into the file's
next data block, we just grab the next index from the FAT, and write normally
like before. However, if we need to get a new data block, then the FAT is
searched for an available block. If there are none available, then we stop
writing the file and exit. If there is a new data block available, the current
index in the FAT will point to the index of the new data block. Writing will
then continue. After the file is done writing, the new file size is calculated.
If this is a new file, file size equals count. If the offset + bytesWritten is
greater than the original file size, then the new file size equals offset +
bytesWritten. Otherwise, the file size remains the same. The difficulty in write
revolves around the transitioning of one data block to another and the changes
required in the root and FAT when new data blocks are added.

# Testing

For testing, we used the test_fs.x given to us as a base tester and compare the
results to the reference program fs_ref.x. To test the first few phases, we used
the reference program to add text files inside the disk and then used the info,
ls, and stat commands to read the contents of the disk. This makes sure that we
are reading the contents of the disk correctly without having to use fs_read()
and fs_write() yet. 

Once we have fs_read() and write() going, we then used our program to add and
remove files from the disk all while making sure we cat the contents. To test
the offset, we made changes within the cat command to read through the file with
a given offset using fs_lseek(). We also made sure that multiple reads will
increase the offset and that reading over the file size will not produce any
errors. As for fs_write(), we made a disk that only has one 2 data blocks (size
data block 1 is always empty) and tried to write a file that is over 4096 bytes
into the disk. This guarantees that our API will not exceed the provided disk
space.
