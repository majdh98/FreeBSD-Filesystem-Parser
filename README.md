# FreeBSD Filesystem Parser


Author: Majd Hamdan

Included is fs_find.c and fs_cat.c, a pair of programs that parse and traverse the various data structures that make up a UFSfilesystem on FreeBSD. Both programs operate on  raw partition data, not on files.


## fs_find.c

This program works like the [find(1)](https://man7.org/linux/man-pages/man1/find.1.html) shell command. find(1) options are not implemented in fs-find, only its general functionality.
fs-find takes as parameter a filesystem image and print a hierarchy of all directories and files within it. For example, here is the result of running
find(1) on the filesystem when it is mounted, against running fs-find on an image of that filesystem:

find(1):
```console
$ cd /mnt/partition
$ find .
.
./.snap
./beef
./big-file
./dead
./dir1
./dir1/dir1.1
./dir1/dir1.1/file2
./dir1/dir1.2
./dir1/dir1.3
./dir2
./dir2/dir2.1
./dir2/dir2.2
./dir2/dir2.3
./dir3
./dir3/dir3.1
./dir3/dir3.2
./dir3/dir3.3
./file1
```

fs-find:

```console
$ ./fs-find partition.img
.snap
dir1
dir1.1
file2
dir1.2
dir1.3
dir2
dir2.1
dir2.2
dir2.3
dir3
dir3.1
dir3.2
dir3.3
file1
dead
beef
big-file
```



## fs-cat
This program works like the [cat(1)](https://man7.org/linux/man-pages/man1/cat.1.html) shell command. cat(1) options are not implemented in fs-cat, only its general functionality.

fs-cat takes as parameters a filesystem image and the path of a file within that filesystem and outputs the contents of the file.

For example, in the hierarchy shown above, the contents of "file2" are "hello\n". when we run fs-cat, we obtain:
```console
$ ./fs-cat 30mb-partition.img dir1/dir1.1/file2
hello
```


## Running the programs

Both programs can be compiled using the Makefile.

To run fs_find on p1.img, run the following on a terminal:
```console
$ make
$ ./fs_find p1.img
```
To run fs_cat on p1.img,  run the following on a terminal:
```console
$ make
$ ./fs_find p1.img <path>
```

## REQUIREMENTS

The files dinode.h, dir.h, fs.h, and *.img must be in the same directory as
fs_find.c.

## Notes
We tested these programs on p1.img, p2.img, p3.img, p4.img, p5.img, and p5.img which can be found [here](https://github.com/majdh98/FreeBSD-Filesystem-Parser/tree/main/Partitions) along with their hexdump and dumpfs. The size of each partition is 30Mib. Larger partitions (600Mib) were also tested but are not shown here due to GitHub size limit. The output of fs_find for each partition is shown here:
```console
$ ./fs_find p1.img
.snap
file1
p1.img
$ ./fs_find p2.img
.snap
file1
dir1
   file2
$ ./fs_find p3.img
.snap
file1
dir1
   file2
   dir2
   dir3
      file3
      file4
dir4
dir5
   file5
$ ./fs_find p4.img
.snap
file1
dir1
   file2
   dir2
   dir3
      file3
      file4
dir4
dir5
   file5
$ ./fs_find p5.img
.snap
file1
dir1
   file2
   dir2
   dir3
      file3
      file4
dir4
dir5
   file5
2mb_file
 $ ./fs_find p6.img
.snap
file1
dir1
   file2
   dir2
   dir3
      file3
      file4
dir4
dir5
   file5
2mb

```

We use file# to test fs_cat. file1 to file5 contain repeated single letters from “a” to “d” with known numbers of repetitions. The repetitions increase by an intentional amount to make sure the file is stored across multiple inodes/sectors. 2mb is a 2Mib file of increasing integers, one integer in every line. The output of fs_cat is written to a file which is then compared with the original file with the diff/cmp commands.  
