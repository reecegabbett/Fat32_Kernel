# FAT32_fs
3rd project for COP4610 Operating Systems @ FSU
Operating Systems Project 3 Group 18
Code written by: Dylan Dalal, Reece Gabbett, Finley Talley

## Division of Labor
### Dylan:
- shell interface additions
- error messages
- creat implementation
- rm implementation
- bugfixing

### Reece:
- Implemented special Makefile commands
- Implemented the Open command
- Implemented the Close command
- Implemented the Writing command
- Implemented the Reading command
- Implemented the Lseek command
- Implemented the OpenFile structure
- Implemented dynamically allocating clusters
- Began and wrote the README
- Edited main() to recognize my implemented commands
- Bug fixing

### Finley:
- shell interface and parsing in main
- reading from boot sector and basic setup for traversing img file
- directory entries
- helper functions (getDirEntry, getDirOffset, getClustNum, getHiClusNum, getLoClusNum, makeStrUpper, compareStr, checkValidDirName)
- info command
- ls command
- size command
- cd command
- rename command
- bugfixing on read
- other bug fixes and testing

## How to Compile and Run Project 3
1. We will assume that you have the project contents un-tared into a folder on linprog.
2. Compile the project3 shell with the `make` command.
3. Once the files have finished compiling and generating the proj3 shell, you can run the shell with the fat32.img using the `make run` command or `./project3 fat32.img`.

After you are finished with the proj3 shell, you can clean up the file contents with the command `make clean`.

## Contents of .tar
### Project3.tar
- Makefile: the makefile that compiles all of the files
- README.md: a markdown text file to list out details about project 3
- log.txt: a list of all the git pushes (git commit log)
- main.c: C file that acts as the main shell
- main.h: Header file for main
- parser.c: A file that reads in the user input into a struct

## Bugs

### Writing a massive string into a new file
Type: `make: *** [Run]` Runtime error, File size limit
When creating a new file with `creat`, it is able to dynamically allocate a new cluster, but if the
given string is too long, the system crashes. May also happen during a write/read. Rare.

### Creat error
Type: runtime, no crash
When creating a new file with `creat`, files are only created in the root regardless of the
director you are in. They also do not properly check for files already made.

## Data Structs in Project3

### BootSector
- BS_jmpBoot: Jump instruction to boot code
- BS_OEMName: OEM  Name  Identifie
- BPB_BytsPerSec: Count of bytes per sector
- BPB_SecPerClus: Number of sectors per allocation unit
- BPB_RsvdSecCnt: Number of reserved sectors in the reserved region
- BPB_NumFATs: The count of file allocation tables
- BPB_RootEntCnt: Value for FAT12 and FAT16
- BPB_TotSec16: the old 16-bit total count of sectors on the volume
- BPB_Media: For removable media
- BPB_FATSz16: the FAT12/FAT16 16-bit count of sectors occupied by one FAT
- BPB_SecPerTrk: Sectors per track
- BPB_NumHeads: Number of heads for interrupt
- BPB_HiddSec: Count of hidden sectors preceding the partition that contains this FAT volume
- BPB_TotSec32: This field is the new 32-bit total count of sectors on the volume
- BPB_FATSz32: the FAT32 32-bit count of sectors occupied by one FAT
- BPB_ExtFlags: Holds certain flags with different meanings
- BPB_FSVer: High byte is major revision number
- BPB_RootClus: This is set to the cluster number of the first cluster of the root directory
- BPB_FSInfo: Sector number of FSINFO structure
- BPB_BkBootSec: indicates the sector number in the reserved area of the volume of a copy of the boot record.
- BPB_Reserved: Reserved. Must be set to 0x0.
- BS_DrvNum: Interrupt 0x13 drive number. Set value to 0x80 or 0x00.
- BS_Reserved1: Reserved. Set value to 0x0
- BS_BootSig: Extended boot signature
- BS_VolID: Volume serial number
- BS_VolLab: Volume label
- BS_FilSysType: Set to the string: ”FAT32”
- Blank1: 0x00
- Signature_word: 0x55

### DIRENTRY
- DIR_Name: “Short” file name limited to 11 characters
- DIR_Attr: Legal file attribute types
- DIR_NTRes: Reserved to 0x0
- DIR_CrtTimeTenth: Component of the file creation time
- DIR_CrtTime: Creation time
- DIR_CrtDate: Creation date
- DIR_LstAccDate: Last access date
- DIR_FstClusHI: High word of first data cluster number for file/directory described by this entry
- DIR_WrtTime: Last modification write time
- DIR_WrtDate: Last modification write date
- DIR_FstClusLO: Low word of first data cluster number for file/directory described by this entry
- DIR_FileSize: Containing size in bytes of file/directory

### currentDirectory
- clusterNum: The cluster index for a given file

### OpenFile
- fileEntry: the DIRENTRY structure for an opened file
- mode: the opened mode (read write or both)
- fileOffset: the offset value given by lseek

## Project 3 Functions

### getDirEntry()
- Brief: Takes in a file name and returns the matching directory structure.
- Parameters: char * filename
- Return type: DIRENTRY

### getDirOffset()
- Brief: Takes in a file directory structure and returns the offset to the written directory in the img.
- Parameters: DIRENTRY dir
- Return type: unsigned

### getClustNum()
- Brief: Takes dir's hi and lo number to return the cluster number using bitmasking
- Parameters: DIRENTRY dir
- Return type: unsigned

### getHiClusNum()
- Brief: Takes in cluster number and returns the hi number using bitmasking
- Parameters: unsigned num
- Return type: unsigned

### getLoClusNum()
- Brief: Takes in cluster number and returns the lo number using bitmasking
- Parameters: unsigned num
- Return type: unsigned

### makeStrUpper()
- Brief: Takes in string and returns string with all uppercase
- Parameters: char * string
- Return type: char *

### checkValidDirName()
- Brief: Scans DirName to see if it can be used as a directory name
- Parameters: unsigned char DirName[11]
- Return type: integer

### compareStr()
- Brief: Returns 0 if the strings don't match and a 1 if they do
- Parameters: char* givenFileName, unsigned char dirName[]
- Return type: integer

### getInfo()
- Brief: Prints out required data from the BootSector
- Parameters: void
- Return type: void

### creat()
- Brief: Creates a new file with the given name
- Parameters: char* filename
- Return type: void

### ls()
- Brief: Prints out all of the file names in the current or given directory
- Parameters: unsigned int clustNum
- Return type: void

### cd()
- Brief: Changes current directory to one with the given clustNum
- Parameters: unsigned int clustNum
- Return type: void

### renameFILE()
- Brief: Renames the `from` file to the `to` name
- Parameters: char\* from, char\* to
- Return type: void

### sizeFILENAME()
- Brief: Prints out the size of the given file
- Parameters: char* fileName
- Return type: void

### openFile()
- Brief: Opens the given file under the given mode
- Parameters: char\* fileName, char\* mode
- Return type: void

### closeFile()
- Brief: Closes the given file, removing it's mode
- Parameters: char* fileName
- Return type: void

### readFile()
- Brief: Finds and reads the given file for size characters
- Parameters: char* fileName, long int size
- Return type: void

### writeFile()
- Brief: Finds the given file and writes the string for size characters, allocating memory when needed
- Parameters: char\* fileName, long int size, char\* string
- Return type: void

### lSeek()
- Brief: Sets the offset of the given file to size
- Parameters: char* fileName, long int size
- Return type: void

## Limitations
- A maximum of 2048 files can be opened at the same time.
- All names are converted to uppercase, and filenames are limited to 8 characters. No special characters allowed in name.
- Some commands were not implemented in time, these include: mkdir, rm, cp, rmdir
