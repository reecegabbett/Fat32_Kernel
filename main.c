#include "main.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

// General char* size
//#ifndef BUFFER
//#define BUFFER = 64
//#ifndef BIG_BUF
//#define BIG_BUF = 256
//#endif



// STRUCTURES
typedef struct __attribute__((__packed__)) BootSector {
  // BPB Structure
  unsigned char BS_jmpBoot[3];
  unsigned char BS_OEMName[8];
  unsigned short BPB_BytsPerSec;
  unsigned char BPB_SecPerClus;
  unsigned short BPB_RsvdSecCnt;
  unsigned char BPB_NumFATs;
  unsigned short BPB_RootEntCnt;
  unsigned short BPB_TotSec16;
  unsigned char BPB_Media;
  unsigned short BPB_FATSz16;
  unsigned short BPB_SecPerTrk;
  unsigned short BPB_NumHeads;
  unsigned BPB_HiddSec;
  unsigned BPB_TotSec32;
  unsigned BPB_FATSz32;
  unsigned short BPB_ExtFlags;
  unsigned short BPB_FSVer;
  unsigned BPB_RootClus;
  unsigned short BPB_FSInfo;
  unsigned short BPB_BkBootSec;
  unsigned char BPB_Reserved[12];
  // Extended BPB Structure
  unsigned char BS_DrvNum;
  unsigned char BS_Reserved1;
  unsigned char BS_BootSig;
  unsigned BS_VolID;
  unsigned char BS_VolLab[11];
  unsigned long long BS_FilSysType;
  unsigned char Blank1[420];
  unsigned short Signature_word;
  // leave out last field with remaining bytes
} BootSector;





typedef struct __attribute__((__packed__)) DIRENTRY {
  unsigned char DIR_Name[11];
  unsigned char DIR_Attr;
  unsigned char DIR_NTRes;
  unsigned char DIR_CrtTimeTenth;
  unsigned short DIR_CrtTime;
  unsigned short DIR_CrtDate;
  unsigned short DIR_LstAccDate;
  unsigned short DIR_FstClusHI;
  unsigned short DIR_WrtTime;
  unsigned short DIR_WrtDate;
  unsigned short DIR_FstClusLO;
  unsigned DIR_FileSize;
} DIRENTRY;


typedef struct currentDirectory {
  unsigned clusterNum;
} currentDirectory;

//Contains a file and the read/write mode
typedef struct OpenFile {
  DIRENTRY fileEntry;
  char* mode;
  unsigned int fileOffset;
} OpenFile;

// HELPERS
DIRENTRY getDirEntry(char*);
unsigned getDirOffset(DIRENTRY);
unsigned getClustNum(DIRENTRY);
unsigned short getHiClusNum(unsigned);
unsigned short getLoClusNum(unsigned);
char* makeStrUpper(char*);
int compareStr(char*, unsigned char[]);
int checkValidDirName(unsigned char[]);

// COMMANDS
void getInfo(void);
void creat(char* filename);
void ls(unsigned int);
void cd(unsigned int);
void renameFILE(char*, char*);
void sizeFILENAME(char*);
void openFile(char* fileName, char* mode);
void closeFile(char* fileName);
void readFile(char* fileName, long int size);
void writeFile(char* fileName, long int size, char* string);
void lSeek(char* fileName, long int offset);

// GLOBALS
BootSector bb;
FILE * fp;
unsigned FirstDataSector;
currentDirectory env;
OpenFile opened_files[2048];
int num_opened_files;

// MAIN SHELL
int main(int argc, char const *argv[]) {
  num_opened_files = 0;
  if (argc != 2) {
    printf("usage: shell <image file>\n");
    return -1;
  }

  char *img_name = malloc(sizeof(char*)); // Questions about behavior of "malloc(sizeof(char*))" - Dylan
  strcpy(img_name, argv[1]);
  char *img_ext = malloc(sizeof(char*));
  strcpy(img_ext, &img_name[strlen(img_name)-4]);

  if (strcmp(img_ext, ".img")) {
    printf( "not an appropriate image filename\n" );
    return -1;
  }

  if (access(img_name, F_OK) == -1) {
    printf("image file %s not found\n", img_name );
    return -1;
  }


  fp = fopen(img_name, "rb+");
  // read all data from the boot sector
  fread(&bb, sizeof(BootSector), 1, fp);

  // calculate first data sectors
  FirstDataSector = bb.BPB_RsvdSecCnt + (bb.BPB_NumFATs * bb.BPB_FATSz32);
  env.clusterNum = bb.BPB_RootClus;

  while (1) {
    tokenlist *input = parser(img_name);
    if (input->size > 0) {
      char *cmd = malloc(sizeof(char*));
      strcpy(cmd, input->items[0]);

      // EXIT
      if (!strcmp(cmd, "exit")) {
        free_tokens(input);
        fclose(fp);
        exit(0);
      }

      // INFO
      else if (!strcmp(cmd, "info")) {
        getInfo();
      }

      // SIZE
      else if (!strcmp(cmd, "size")) {
        if (input->size < 2) {
          printf("ERROR: not enough arguments provided\n");
          printf("usage: size FILENAME\n");
        }
        else if (input->size > 2) {
          printf("ERROR: too many arguments provided\n");
          printf("usage: size FILENAME\n");
        }
        else {
          sizeFILENAME(input->items[1]);
        }
      }

      // LS
      else if (!strcmp(cmd, "ls")) {
        if (input->size == 1) {
          if (bb.BPB_RootClus < 2) {
            printf("ERROR: invalid root cluster\n");
          }
          else {
            ls(env.clusterNum);
          }
        }
        else if (input->size > 2) {
          printf("ERROR: more than one argument provided for ls\n");
          printf("usage: ls DIRNAME\n");
        }
        else {
          DIRENTRY fileDir = getDirEntry(input->items[1]);
          // check if name exists or if not directory
          if (strlen((const char*)fileDir.DIR_Name) == 0 || fileDir.DIR_Attr != 0x10) {
            printf("ERROR: could not find directory %s\n", input->items[1]);
          }
          else {
            //get the cluster number then ls
            unsigned fileDirNum = getClustNum(fileDir);
            ls(fileDirNum);
          }
        }
      }

      // CD
      else if (!strcmp(cmd, "cd")) {
        if (input->size != 2) {
          printf("ERROR: no or more than one argument provided for cd\n");
          printf("usage: cd DIRNAME\n");
        }
        else {
          DIRENTRY fileDir = getDirEntry(input->items[1]);
          // check if name exists or if not directory
          if (strlen((const char*)fileDir.DIR_Name) == 0 || fileDir.DIR_Attr != 0x10) {
            printf("ERROR: could not find directory %s\n", input->items[1]);
          }
          else {
            //get the cluster number then cd
            unsigned fileDirNum = getClustNum(fileDir);
            cd(fileDirNum);
          }
        }
      }

      // CREAT
      else if (!strcmp(cmd, "creat")) {

        if (input->size == 1) {
        	printf("Usage: creat FILENAME");
        }
        else if (input->size == 2) {
        	if (strlen(input->items[1]) > 8)
        		printf("Please input a FILENAME of 8 or less characters.");
    			else
    				creat(input->items[1]);
        }
      }

      // MKDIR
      else if (!strcmp(cmd, "mkdir")) {
    	  if (input->size == 1) {
    	    printf("Usage: creat FILENAME");
    	  }
    	  else if (input->size == 2) {
    	    if (strlen(input->items[1]) > 8)
    	      printf("Please input a FILENAME of 8 or less characters.");
    	  	else
    	  		creat(input->items[1]);
        }
      }
      // OPEN
      else if (!strcmp(cmd, "open")) {
        if (input->size < 3) {
          printf("ERROR: not enough arguments provided\n");
          printf("usage: open FILENAME MODE\n");
        }
        else if (input->size > 3) {
          printf("ERROR: too many arguments provided\n");
          printf("usage: open FILENAME MODE\n");
        }
        else {
          openFile(input->items[1], input->items[2]);
        }
      }

      // CLOSE
      else if (!strcmp(cmd, "close")) {
        if (input->size < 2) {
          printf("ERROR: not enough arguments provided\n");
          printf("usage: close FILENAME\n");
        }
        else if (input->size > 2) {
          printf("ERROR: too many arguments provided\n");
          printf("usage: close FILENAME\n");
        }
        else {
          closeFile(input->items[1]);
        }
      }

      // LSEEK
      else if (!strcmp(cmd, "lseek")) {
        if (input->size < 3) {
          printf("ERROR: not enough arguments provided\n");
          printf("usage: lseek FILENAME OFFSET\n");
        }
        else if (input->size > 3) {
          printf("ERROR: too many arguments provided\n");
          printf("usage: lseek FILENAME OFFSET\n");
        }
        else {
          unsigned long fileOffset = strtol(input->items[2],NULL,10);
          lSeek(input->items[1],fileOffset);
        }
      }

      // READ
      else if (!strcmp(cmd, "read")) {
        if (input->size < 3) {
          printf("ERROR: not enough arguments provided\n");
        }
        else if (input->size > 3) {
          printf("ERROR: too many arguments provided\n");
        }
        else {
          long int fileSize;
          fileSize = strtol(input->items[2],NULL,10);
          if(fileSize>0){
            readFile(input->items[1],fileSize);
          }
          else{
            printf("Please input the size as an int above 0\n");
          }
        }
      }

      // WRITE
      else if (!strcmp(cmd, "write")) {
        if (input->size < 4) {
          printf("ERROR: not enough arguments provided\n");
        }
        else {
          for(int i = 3; i< input->size-1; i++){
            input->items[3] = strcat(input->items[3]," ");
            input->items[3] = strcat(input->items[3],input->items[i+1]);
          }
          long int fileSize;
          fileSize = strtol(input->items[2],NULL,10);
          if(fileSize>0){
            writeFile(input->items[1],fileSize,input->items[3]);
          }
          else{
            printf("Please input the size as an int above 0\n");
          }
        }
      }

      // RM
      else if (!strcmp(cmd, "rm")) {
        printf("the command typed was %s\n", cmd);
      }

      // RENAME
      else if (!strcmp(cmd, "rename")) {
        if (input->size != 3) {
          printf("ERROR: too many or not enough arguments provided\n");
          printf("usage: rename OLDDIRNAME NEWDIRNAME\n");
        }
        else {
          renameFILE(input->items[1], input->items[2]);
        }
      }

      // CP
      else if (!strcmp(cmd, "cp")) {
        printf("the command typed was %s\n", cmd);
      }

      // RMDIR
      else if (!strcmp(cmd, "rmdir")) {
        printf("the command typed was %s\n", cmd);
      }

      // OTHER
      else {
        printf("you didn't enter any available command\n");
      }

      //Bugtesting: Printing the opened files
      /*
      printf("\nopenedfile1: %s in mode: %s\n", opened_files[0].fileEntry.DIR_Name, opened_files[0].mode);
      printf("openedfile2: %s in mode: %s\n", opened_files[1].fileEntry.DIR_Name, opened_files[1].mode);
      printf("arraySize: %u\n",num_opened_files);
      */
    }
    printf("\n");
  }

  return 0;
}

// FUNCTIONS


DIRENTRY getDirEntry(char *fileName) {

  unsigned long int currentCluster= env.clusterNum;
  int moreDirectories=1;
  while (moreDirectories) {
    unsigned Offset = FirstDataSector + (currentCluster-2) * bb.BPB_SecPerClus;
    unsigned ByteOffset = Offset * bb.BPB_BytsPerSec;
    if (currentCluster==0) {
      DIRENTRY emptyDir;
      return emptyDir;
      break;
    }
    else {
      // DIRENTRY is 32 bytes, and so is the long directory name entry
      int i=0;
      while (i*sizeof(DIRENTRY) < bb.BPB_BytsPerSec) {
        fseek(fp, ByteOffset + i*sizeof(DIRENTRY), SEEK_SET);
        unsigned char start_line;
        fread(&start_line, sizeof(unsigned char), 1, fp);
        // if this is a long directory name entry, skip
        if (start_line == 0x41) {
          i++;
          continue;
        }
        else if (start_line == 0x00) {
          // no more directory entries here
          break;
        }


        // move file pointer backwards by 1 since fread moves the pointer
        fseek(fp, -1, SEEK_CUR);
        DIRENTRY currentFile;
        fread(&currentFile, sizeof(DIRENTRY), 1, fp);
        if (compareStr(makeStrUpper(fileName), currentFile.DIR_Name)) {
          return currentFile;
        }

        i++;
      }

      unsigned long int fatOffset = bb.BPB_RsvdSecCnt * bb.BPB_BytsPerSec + currentCluster * 4;
      unsigned nextCluster;
      fseek(fp,fatOffset,SEEK_SET);
      fread(&nextCluster, sizeof(unsigned), 1, fp);
      if (nextCluster >= 0x0FFFFFF8 && nextCluster<=0x0FFFFFFF) {
        nextCluster=0;
      }

      currentCluster = nextCluster;
    }

  }

  // empty directory to return if no matching dir found
  DIRENTRY emptyDir;
  return emptyDir;
}

unsigned getDirOffset(DIRENTRY dir) {
  unsigned long int currentCluster= env.clusterNum;
  int moreDirectories=1;
  while (moreDirectories) {
    unsigned Offset = FirstDataSector + (currentCluster-2) * bb.BPB_SecPerClus;
    unsigned ByteOffset = Offset * bb.BPB_BytsPerSec;
    if (currentCluster==0) {
      return 0;
    }
    else {
      // DIRENTRY is 32 bytes, and so is the long directory name entry
      int i=0;
      while (i*sizeof(DIRENTRY) < bb.BPB_BytsPerSec) {
        fseek(fp, ByteOffset + i*sizeof(DIRENTRY), SEEK_SET);
        unsigned char start_line;
        fread(&start_line, sizeof(unsigned char), 1, fp);
        // if this is a long directory name entry, skip
        if (start_line == 0x41) {
          i++;
          continue;
        }
        else if (start_line == 0x00) {
          // no more directory entries here
          break;
        }


        // move file pointer backwards by 1 since fread moves the pointer
        fseek(fp, -1, SEEK_CUR);
        DIRENTRY currentFile;
        fread(&currentFile, sizeof(DIRENTRY), 1, fp);
        if (!strcmp((const char*)dir.DIR_Name, (const char*)currentFile.DIR_Name)) {
          return (ByteOffset + i*sizeof(DIRENTRY));
        }

        i++;
      }

      unsigned long int fatOffset = bb.BPB_RsvdSecCnt * bb.BPB_BytsPerSec + currentCluster * 4;
      unsigned nextCluster;
      fseek(fp,fatOffset,SEEK_SET);
      fread(&nextCluster, sizeof(unsigned), 1, fp);
      if (nextCluster >= 0x0FFFFFF8 && nextCluster<=0x0FFFFFFF) {
        nextCluster=0;
      }

      currentCluster = nextCluster;
    }

  }

  // Error didn't find directory
  return 0;
}

unsigned getClustNum(DIRENTRY dir) {
  unsigned short left = dir.DIR_FstClusHI;
  unsigned short right = dir.DIR_FstClusLO;
  unsigned final = (left << 4) | right;
  return final;
}

unsigned short getHiClusNum(unsigned num) {
  unsigned short high;
  num = (num & 4294901760) >> 4;
  high = (unsigned short)num;
  return high;
}

unsigned short getLoClusNum(unsigned num) {
  unsigned short low;
  num = (num & 65535);
  low = (unsigned short)num;
  return low;
}

char* makeStrUpper(char* string) {
  for (int i = 0; string[i] != '\0'; i++) {
    string[i] = toupper(string[i]);
  }
  return string;
}

int compareStr(char* givenFileName, unsigned char dirName[]){
  int len1 = strlen(givenFileName);

  for (int i = 0; i < len1; i++) {
    if (givenFileName[i] != dirName[i]) {
      return 0;
    }
  }

  if (isspace(dirName[len1])) {
    return 1;
  }
  return 0;
}

int checkValidDirName(unsigned char DirName[11] ) {
  if (DirName[0] == 0x20) {
    return 0;
  }
  for (int i=0; i<11; i++) {
    switch (DirName[i]) {
      case 0x22:
        return 0;
      case 0x2A:
        return 0;
      case 0x2B:
        return 0;
      case 0x2C:
        return 0;
      case 0x2E:
        return 0;
      case 0x2F:
        return 0;
      case 0x3A:
        return 0;
      case 0x3B:
        return 0;
      case 0x3C:
        return 0;
      case 0x3D:
        return 0;
      case 0x3E:
        return 0;
      case 0x3F:
        return 0;
      case 0x5B:
        return 0;
      case 0x5C:
        return 0;
      case 0x5D:
        return 0;
      case 0x7C:
        return 0;
    }

    if (DirName[i] < 0x20) {
      return 0;
    }
  }
  return 1;
}

void getInfo(void) {
  printf("Bytes per sector: %u\n", bb.BPB_BytsPerSec);
  printf("Sectors per cluster: %u\n", bb.BPB_SecPerClus);
  printf("Reserved sector count: %u\n", bb.BPB_RsvdSecCnt);
  printf("Number of FATs: %u\n", bb.BPB_NumFATs);
  printf("Total sectors: %u\n", bb.BPB_TotSec32);
  printf("FAT size: %u\n", bb.BPB_FATSz32);
  printf("Root cluster: %u\n", bb.BPB_RootClus);
}

void creat(char* filename) {
	int finished = 0;
	int found = 0;
	/* Format Filename */
	char *p;
	char *pad = "           \0";
	// As per the project specifications, filenames can only be uppercase
	// and must be 11 characters long.
	for (p = filename; *p != '\0'; ++p) {
		*p = toupper(*p);
	}
	strcat(filename, pad);

	// Note for my own fucking tiny brain: a 'byte' is 8 bits; a character 'a'
	// is 4 bits, so a byte is 2 characters. The null charcater '\0' is one byte.

	/* Initialize File */
	DIRENTRY NewDir;
	memcpy(NewDir.DIR_Name, filename, 11);	// 1
	NewDir.DIR_Name[10] = '\0'; 		   	// 1   11 bytes		unsigned char
	NewDir.DIR_Attr = '\0';					// 2	1 byte		unsigned char
	NewDir.DIR_NTRes = '\0';				// 3	1 byte		unsigned char
	NewDir.DIR_CrtTimeTenth = '\0';			// 4	1 byte		unsigned char
	NewDir.DIR_CrtTime = 1000;				// 5	2 bytes		unsigned short
	NewDir.DIR_CrtDate = 1000;				// 6	2 bytes		unsigned short
	NewDir.DIR_LstAccDate = 1000;			// 7	2 bytes		unsigned short
 // NewDir.DIR_FstClusHI = " \0"; 			// 8	2 bytes		unsigned short
	NewDir.DIR_WrtTime = 1000;				// 9	2 bytes		unsigned short
	NewDir.DIR_WrtDate = 1000;				// 10	2 bytes		unsigned short
 // NewDir.DIR_FstClusLO = " \0"; 			// 11	2 bytes		unsigned short
	NewDir.DIR_FileSize = 0;				// 12	4 bytes		unsigned

	// As per the description, all of these fields can be given temporary information
	// except for the name, which has already been formatted; the filesize, which
	// starts at 0; and the first cluster values of HI and LO, which can only be
	// discovered by iterating through the DA TA to find an empty cluster.

	unsigned FirstFATSector = bb.BPB_RsvdSecCnt * 512;
	unsigned DataOffset = FirstDataSector * bb.BPB_BytsPerSec;
	unsigned FATOffset = bb.BPB_RsvdSecCnt * bb.BPB_BytsPerSec;
	// Beginning of the FAT table
	printf ("Creating file...");

	 /* Find empty DATA entry to store NewDIR */
	int i=0;
	while (i * sizeof(DIRENTRY) < bb.BPB_BytsPerSec) {
		fseek(fp, DataOffset + i*sizeof(DIRENTRY), SEEK_SET);
		// Iterate through first sectors of DATA table to find where to add
		// NewDir. Set beginning of the global file 'fp' to be offset by
		// ByteOffset (start of the DATA table) + size of whichever directory
		// you're examining.

		unsigned char dir_location;
		fread(&dir_location, sizeof(unsigned char), 1, fp);
		// Read sizeof(unsigned char)*1 bytes (1 bytes, 2 characters) of
		// global file 'fp' into the unsigned char variable dir_location.
		if (dir_location == 0x00 || dir_location == 0xE5) {
			// An open entry has been found in the DATA. It is now time to
			// find an open cluster in the FAT and complete initialization
			// of DIRENTRY in order to write it into the FAT.

			/* Find empty FAT entry to store as accompanying NewDir cluster */
			int N = 3; 			// Start at the third entry
			while (FATOffset + N * sizeof(unsigned char) * 4 < DataOffset) {
				unsigned CurrentFATSector = FirstFATSector + (N * 4);
				fseek(fp, CurrentFATSector, SEEK_SET);
				// In the FAT table, parse the entry in four values:
				unsigned char fourth, third, second, first;
				fseek(fp, CurrentFATSector, SEEK_SET);
				fread(&fourth, sizeof(unsigned char), 1, fp);
				fseek(fp, CurrentFATSector+1, SEEK_SET);
				fread(&third, sizeof(unsigned char), 1, fp);
				fseek(fp, CurrentFATSector+2, SEEK_SET);
				fread(&second, sizeof(unsigned char), 1, fp);
				fseek(fp, CurrentFATSector+3, SEEK_SET);
				fread(&first, sizeof(unsigned char), 1, fp);
				// Calculate if this entry equals 0 with this conversion:
				unsigned long int total= ((16777216*first)+(65536*second)+(third*256)+(fourth));
				// If it equals 0, the entry is empty.
				if(total==0){
					// Set the starting cluster number equal to the empty entry number
					NewDir.DIR_FstClusLO = getLoClusNum(CurrentFATSector);
          NewDir.DIR_FstClusHI = getHiClusNum(CurrentFATSector);

					// Replace the empty entry with the end of string value.
					unsigned int FF = 255;
					unsigned int OF = 15;
					fseek(fp, CurrentFATSector, SEEK_SET);
					fwrite(&FF, sizeof(unsigned int), 1, fp);
					fseek(fp, CurrentFATSector+1, SEEK_SET);
					fwrite(&FF, sizeof(unsigned int), 1, fp);
					fseek(fp, CurrentFATSector+2, SEEK_SET);
					fwrite(&FF, sizeof(unsigned int), 1, fp);
					fseek(fp, CurrentFATSector+3, SEEK_SET);
					fwrite(&OF, sizeof(unsigned int), 1, fp);
					fseek(fp, CurrentFATSector+4, SEEK_SET);
					found = 1; // Found Indicator
					break;
				}
			  // Continue iterating looking for empty FAT entries
			  N++;
			}
		}
		/* Set the file in DATA */
		if (found == 1) {
			fseek(fp, DataOffset + i*sizeof(DIRENTRY), SEEK_SET);
			fwrite(&NewDir, sizeof(NewDir), 1, fp); // i think these numbers are right?
			printf ("created file %s", NewDir.DIR_Name);
			finished = 1;
			break;
		}
	  // Continue iterating looking for empty DATA entries to store NewDir (DIRENTRY)
	  i++;
	}
	if (finished == 0) {
		printf ("Unable to add new file- out of space!");
	}
}

void ls(unsigned int clustNum) {
  unsigned long int currentCluster= clustNum;
  // DIRENTRY is 32 bytes, and so is the long directory name entry
  int moreDirectories=1;
  while (moreDirectories) {
    unsigned Offset = FirstDataSector + (currentCluster-2) * bb.BPB_SecPerClus;
    unsigned ByteOffset = Offset * bb.BPB_BytsPerSec;
    if (currentCluster==0) {
      return;
    }
    else {
      int i=0;
      while (i*sizeof(DIRENTRY) < bb.BPB_BytsPerSec) {
        fseek(fp, ByteOffset + i*sizeof(DIRENTRY), SEEK_SET);
        unsigned char start_line;
        fread(&start_line, sizeof(unsigned char), 1, fp);

        fseek(fp, ByteOffset + i*sizeof(DIRENTRY) + 11, SEEK_SET);
        unsigned char check_bytes;
        fread(&check_bytes, sizeof(unsigned char), 1, fp);
        // if this is a long directory name entry, skip
        // TODO: add condition to look for attr_long_name
        if (start_line == 0x41 || check_bytes == 15) {
          i++;
          continue;
        }
        else if (start_line == 0x00) {
          // no more directory entries here
          break;
        }

        // move file pointer to where it should be fread moves the pointer
        fseek(fp, ByteOffset + i*sizeof(DIRENTRY), SEEK_SET);

        DIRENTRY currentFile;
        fread(&currentFile, sizeof(currentFile), 1, fp);
        printf("%s\n", currentFile.DIR_Name);
        i++;
      }
      unsigned long int fatOffset = bb.BPB_RsvdSecCnt * bb.BPB_BytsPerSec + currentCluster * 4;
      unsigned nextCluster;
      fseek(fp,fatOffset,SEEK_SET);
      fread(&nextCluster, sizeof(unsigned), 1, fp);
      if (nextCluster >= 0x0FFFFFF8 && nextCluster<=0x0FFFFFFF) {
        nextCluster=0;
      }

      currentCluster = nextCluster;
    }
  }

}

void cd(unsigned int clustNum) {
  // change the environment's cluster number
  env.clusterNum = clustNum;
  if (clustNum == 0) {
    // must be root directory
    env.clusterNum = bb.BPB_RootClus;
  }
  // also change the path string
  return;
}

void renameFILE(char* from, char* to) {
  // check if from and to are the same, if so, return
  if (!strcmp(from, to)) {
    printf("ERROR: cannot rename file to the same thing\n" );
    return;
  }

  if (strlen(to) > 10) {
    printf("ERROR: filename %s is too long\n", to);
    return;
  }

  // check if to already exists, if so, error
  DIRENTRY toDir = getDirEntry(to);

  if (strlen((const char*)toDir.DIR_Name) != 0) {
    printf("ERROR: %s already exists\n", to);
    return;
  }

  // check if from exists, if not, error
  DIRENTRY fromDir = getDirEntry(from);
  if (strlen((const char*)fromDir.DIR_Name) == 0) {
    printf("ERROR: %s does not exist\n", from);
    return;
  }

  unsigned offset = getDirOffset(fromDir);

  for (int i = 0; i < strlen(to); i++) {
    fromDir.DIR_Name[i] = toupper(to[i]);
  }

  for (int i = strlen(to); i < 10; i++) {
    fromDir.DIR_Name[i] = ' ';
  }


  fromDir.DIR_Name[10] = '\0';

  fseek(fp, offset, SEEK_SET);
  fwrite(&fromDir, sizeof(DIRENTRY), 1, fp);


  return;
}

//Prints size of given file
void sizeFILENAME(char *fileName) {

  unsigned long int currentCluster= env.clusterNum;
  // DIRENTRY is 32 bytes, and so is the long directory name entry
  int moreDirectories=1;
  while (moreDirectories) {
    unsigned Offset = FirstDataSector + (currentCluster-2) * bb.BPB_SecPerClus;
    unsigned ByteOffset = Offset * bb.BPB_BytsPerSec;
    if (currentCluster==0) {
      return;
    }
    else {
      int i=0;
      while (i*sizeof(DIRENTRY) < bb.BPB_BytsPerSec) {
        fseek(fp, ByteOffset + i*sizeof(DIRENTRY), SEEK_SET);
        unsigned char start_line;
        fread(&start_line, sizeof(unsigned char), 1, fp);

        fseek(fp, ByteOffset + i*sizeof(DIRENTRY) + 11, SEEK_SET);
        unsigned char check_bytes;
        fread(&check_bytes, sizeof(unsigned char), 1, fp);
        // if this is a long directory name entry, skip
        // TODO: add condition to look for attr_long_name
        if (start_line == 0x41 || check_bytes == 15) {
          i++;
          continue;
        }
        else if (start_line == 0x00) {
          // no more directory entries here
          break;
        }

        // move file pointer to where it should be fread moves the pointer
        fseek(fp, ByteOffset + i*sizeof(DIRENTRY), SEEK_SET);

        DIRENTRY currentFile;
        fread(&currentFile, sizeof(currentFile), 1, fp);

        if (compareStr(makeStrUpper(fileName), currentFile.DIR_Name)) {
          printf("%u\n", currentFile.DIR_FileSize);
          return;
        }
        i++;
      }
      unsigned long int fatOffset = bb.BPB_RsvdSecCnt * bb.BPB_BytsPerSec + currentCluster * 4;
      unsigned nextCluster;
      fseek(fp,fatOffset,SEEK_SET);
      fread(&nextCluster, sizeof(unsigned), 1, fp);
      if (nextCluster >= 0x0FFFFFF8 && nextCluster<=0x0FFFFFFF) {
        nextCluster=0;
      }

      currentCluster = nextCluster;
    }
  }

  printf("ERROR: could not find file or directory %s\n", fileName);
}

//Adds given file to opened file array
void openFile(char* fileName, char* mode){
  fileName = makeStrUpper(fileName);
  unsigned Offset = FirstDataSector + (env.clusterNum-2) * bb.BPB_SecPerClus;
  unsigned ByteOffset = Offset * bb.BPB_BytsPerSec;

  // DIRENTRY is 32 bytes, and so is the long directory name entry
  int i=0;
  while (i*sizeof(DIRENTRY) < bb.BPB_BytsPerSec) {
    fseek(fp, ByteOffset + i*sizeof(DIRENTRY), SEEK_SET);
    unsigned char start_line;
    fread(&start_line, sizeof(unsigned char), 1, fp);

    fseek(fp, ByteOffset + i*sizeof(DIRENTRY) + 11, SEEK_SET);
    unsigned char check_bytes;
    fread(&check_bytes, sizeof(unsigned char), 1, fp);
    // if this is a long directory name entry, skip
    // TODO: add condition to look for attr_long_name
    if (start_line == 0x41 || check_bytes == 15) {
      i++;
      continue;
    }
    else if (start_line == 0x00) {
      // no more directory entries here
      break;
    }

    // move file pointer to where it should be fread moves the pointer
    fseek(fp, ByteOffset + i*sizeof(DIRENTRY), SEEK_SET);

    DIRENTRY currentFile;
    fread(&currentFile, sizeof(currentFile), 1, fp);
    // found the matching file
    if (compareStr(fileName, currentFile.DIR_Name)) {

      //Scan opened files
      for (int j=0; j < num_opened_files; j++)
      {
        //Check to see if already opened
        if(compareStr(fileName, opened_files[j].fileEntry.DIR_Name)){
          printf("ERROR: File already opened\n");
          return;
        }
      }
      //Adding File and given Mode to Opened File Array
      if (!strcmp(mode, "r")) {
        OpenFile openableFile;
        openableFile.fileEntry = currentFile;
        openableFile.mode = "r";
        opened_files[num_opened_files]= openableFile;
        num_opened_files++;
        openableFile.fileOffset = 0;
      } else if (!strcmp(mode, "w")) {
        OpenFile openableFile;
        openableFile.fileEntry = currentFile;
        openableFile.mode = "w";
        opened_files[num_opened_files]= openableFile;
        num_opened_files++;
        openableFile.fileOffset = 0;
      } else if (!strcmp(mode, "rw")) {
        OpenFile openableFile;
        openableFile.fileEntry = currentFile;
        openableFile.mode = "rw";
        opened_files[num_opened_files]= openableFile;
        num_opened_files++;
        openableFile.fileOffset = 0;
      } else if (!strcmp(mode, "wr")) {
        OpenFile openableFile;
        openableFile.fileEntry = currentFile;
        openableFile.mode = "wr";
        opened_files[num_opened_files]= openableFile;
        num_opened_files++;
        openableFile.fileOffset = 0;
      } else {
        printf("ERROR: Invalid mode\n");
        return;
      }

      return;
    }

    i++;

  }

  printf("ERROR: Could not find file  %s\n", fileName);

}

//If file is in opened file array, remove it.
void closeFile(char* fileName) {
  fileName = makeStrUpper(fileName);
  //Scan opened files

  for (int i=0; i < num_opened_files; i++)
  {
    //if file is opened
    if (compareStr(fileName, opened_files[i].fileEntry.DIR_Name)){

      //Remove from array by bringing every file above it down by 1.
      for (int j=i; j < num_opened_files; j++)
      {
        opened_files[j] = opened_files[j+1];
      }
      //Set last opened file to an empty struct
      opened_files[num_opened_files-1]= opened_files[num_opened_files+1];
      num_opened_files--;
      return;
    }
  }
  //if file is not opened
  printf("ERROR: Opened file does not exist\n");
  return;

}

void readFile(char* fileName, long int size){
  fileName = makeStrUpper(fileName);
  //Scan opened files

  for (int i=0; i < num_opened_files; i++)
  {
    //if file is opened
    if (compareStr(fileName, opened_files[i].fileEntry.DIR_Name))
    {
      //if it is in any mode besides just write
      if(strcmp(opened_files[i].mode,"w")){
        int moreData = 1;
        unsigned long int currentCluster= getClustNum(opened_files[i].fileEntry);
        //if offset+size > fileSize -> changing size to size-offset
        if(size+opened_files[i].fileOffset > opened_files->fileEntry.DIR_FileSize){
          size = size - opened_files[i].fileOffset;
        }
        long int charLeft=size;
        printf("\n");
        //while there is still data to read
        while(moreData){
          //if the current cluster is the end
          if(currentCluster==268435455){
            return;
          }
          //if we have to read a whole cluster
          else if(charLeft>512){
            char buffer[512];
            //read at the beginning of the currentCluster
            unsigned long int offset = (((bb.BPB_RsvdSecCnt + (bb.BPB_NumFATs * bb.BPB_FATSz32)) + (currentCluster - 2) * bb.BPB_SecPerClus) * bb.BPB_BytsPerSec);
            fseek(fp, offset+opened_files[i].fileOffset, SEEK_SET);
            fread(buffer, 512, 1, fp);
            //update chars left to print
            charLeft=charLeft-512;
            //cap the string and print to user
            buffer[511]='\0';
            printf("%s",buffer);

            //get the offset to a given cluster in the FAT and read in the four bytes of data
            unsigned long int fatOffset = bb.BPB_RsvdSecCnt * bb.BPB_BytsPerSec + currentCluster * 4;
            unsigned char fourth;
            unsigned char third;
            unsigned char second;
            unsigned char first;
            fseek(fp,fatOffset,SEEK_SET);
            fread(&fourth, sizeof(unsigned char), 1, fp);
            fseek(fp,fatOffset+1,SEEK_SET);
            fread(&third, sizeof(unsigned char), 1, fp);
            fseek(fp,fatOffset+2,SEEK_SET);
            fread(&second, sizeof(unsigned char), 1, fp);
            fseek(fp,fatOffset+3,SEEK_SET);
            fread(&first, sizeof(unsigned char), 1, fp);
            //use the byte data to calculate the next cluster number and itterate
            unsigned long int nextCluster= ((16777216*first)+(65536*second)+(third*256)+(fourth));
            currentCluster = nextCluster;
          }
          //if we are reading a portion of a cluster
          else {
            //calculate buffer length and offset to the cluster
            char buffer[charLeft];
            unsigned long int offset = (((bb.BPB_RsvdSecCnt + (bb.BPB_NumFATs * bb.BPB_FATSz32)) + (currentCluster - 2) * bb.BPB_SecPerClus) * bb.BPB_BytsPerSec);
            //read the number of chars left into the buffer, print the buffer, then kill the while loop
            fseek(fp, offset+opened_files[i].fileOffset, SEEK_SET);
            fread(buffer, charLeft, 1, fp);
            buffer[charLeft]='\0';
            printf("%s\n",buffer);
            moreData = 0;
            opened_files[i].fileOffset = opened_files[i].fileOffset + size;
          }
        }
        //No more data to read
        return;
      }
      else{
        printf("ERROR: File is not in \'read\' mode\n");
        return;
      }
    }
  }
  printf("ERROR: File not opened\n");
  return;
}

void writeFile(char* fileName, long int size, char* string){
  fileName = makeStrUpper(fileName);
  //Scan opened files

  for (int i=0; i < num_opened_files; i++)
  {
    //if file is opened
    if (compareStr(fileName, opened_files[i].fileEntry.DIR_Name))
    {
      //if file is in write mode
      if(strcmp(opened_files[i].mode,"r")){
        //recalibrating size (WIP)
        if(size>opened_files[i].fileEntry.DIR_FileSize){
          int j=0;
          while (j*sizeof(DIRENTRY) < bb.BPB_BytsPerSec) {
            unsigned Offset = FirstDataSector + (env.clusterNum-2) * bb.BPB_SecPerClus;
            unsigned ByteOffset = Offset * bb.BPB_BytsPerSec;
            fseek(fp, ByteOffset + j*sizeof(DIRENTRY), SEEK_SET);
            unsigned char start_line;
            fread(&start_line, sizeof(unsigned char), 1, fp);

            fseek(fp, ByteOffset + j*sizeof(DIRENTRY) + 11, SEEK_SET);
            unsigned char check_bytes;
            fread(&check_bytes, sizeof(unsigned char), 1, fp);
            // if this is a long directory name entry, skip
            // TODO: add condition to look for attr_long_name
            if (start_line == 0x41 || check_bytes == 15) {
              j++;
              continue;
            }
            else if (start_line == 0x00) {
              // no more directory entries here
              break;
            }

            // move file pointer to where it should be fread moves the pointer
            fseek(fp, ByteOffset + j*sizeof(DIRENTRY), SEEK_SET);

            DIRENTRY currentFile;
            fread(&currentFile, sizeof(currentFile), 1, fp);
            // found the matching file
            if (compareStr((char*)opened_files[i].fileEntry.DIR_Name, currentFile.DIR_Name)) {
              //set the new size
              currentFile.DIR_FileSize = size;
              unsigned dirOffset = getDirOffset(currentFile);
              fseek(fp,dirOffset,SEEK_SET);
              fwrite(&currentFile, sizeof(DIRENTRY), 1, fp);
            }
            j++;
          }
        }
        //set up variables
        int keepWriting = 1;
        long int charLeft=size;
        long int lastRead = 0;
        //get current file cluster
        unsigned long int currentCluster= getClustNum(opened_files[i].fileEntry);
        //while there is data left to write
        while(keepWriting){
          //if writing to whole cluster
          if(charLeft>512){
            char buffer[512];

            //load buffer with next 512 chars
            for(int j =0; j<512; j++){
              buffer[j] = string[lastRead];
              lastRead++;
            }
            buffer[511]='\0';
            //Calculate offset and write 512 chars to cluster
            unsigned long int offset = (((bb.BPB_RsvdSecCnt + (bb.BPB_NumFATs * bb.BPB_FATSz32)) + (currentCluster - 2) * bb.BPB_SecPerClus) * bb.BPB_BytsPerSec);
            fseek(fp, offset+opened_files[i].fileOffset, SEEK_SET);
            fwrite(buffer, size, 1, fp);
            charLeft=charLeft-512;


            //Find next cluster by reading the FAT for where the cluster is pointing
            unsigned long int fatOffset = bb.BPB_RsvdSecCnt * bb.BPB_BytsPerSec + currentCluster * 4;
            unsigned char fourth;
            unsigned char third;
            unsigned char second;
            unsigned char first;
            //Store byte values that represent next cluster
            fseek(fp,fatOffset,SEEK_SET);
            fread(&fourth, sizeof(unsigned char), 1, fp);

            fseek(fp,fatOffset+1,SEEK_SET);
            fread(&third, sizeof(unsigned char), 1, fp);

            fseek(fp,fatOffset+2,SEEK_SET);
            fread(&second, sizeof(unsigned char), 1, fp);

            fseek(fp,fatOffset+3,SEEK_SET);
            fread(&first, sizeof(unsigned char), 1, fp);
            //Calulate clusterNum with byte values
            unsigned long int nextCluster= ((16777216*first)+(65536*second)+(third*256)+(fourth));
            //if the next cluster is the end of the file: allocate more space
            if(nextCluster==268435455){
              //create new variables to manipulate as we be using the data in the old ones later
              unsigned long int writeOffset = fatOffset;
              int allocClust = 1;
              unsigned long int tempCluster = currentCluster;
              //while a new cluster has not been allocated
              while(allocClust){
                //look in the next FAT cluster and read the pointing bytes
                tempCluster = tempCluster + 1;
                fatOffset = bb.BPB_RsvdSecCnt * bb.BPB_BytsPerSec + tempCluster * 4;
                fseek(fp,fatOffset,SEEK_SET);
                fread(&fourth, sizeof(unsigned char), 1, fp);

                fseek(fp,fatOffset+1,SEEK_SET);
                fread(&third, sizeof(unsigned char), 1, fp);

                fseek(fp,fatOffset+2,SEEK_SET);
                fread(&second, sizeof(unsigned char), 1, fp);

                fseek(fp,fatOffset+3,SEEK_SET);
                fread(&first, sizeof(unsigned char), 1, fp);
                //calculate cluster num
                nextCluster= ((16777216*first)+(65536*second)+(third*256)+(fourth));
                //if cluster num is 0, the cluster in unallocated, we will allocate this one
                if(nextCluster==0){
                  //calculate the required cluster number based on the offset
                  unsigned long int numClus = ((fatOffset/4)-((bb.BPB_RsvdSecCnt*bb.BPB_BytsPerSec)/4));
                  int hexs[8];

                  //break the clusterNum down into ints that will be translated into hex by fwrite()

                  for(int k = 7; k>=0; k--){
                    hexs[k] = numClus % 16;
                    numClus = (numClus - hexs[k])/16;
                  }
                  unsigned long int update1;
                  unsigned long int update2;
                  unsigned long int update3;
                  unsigned long int update4;
                  update1 = ((hexs[6]*16) + hexs[7]);
                  update2 = ((hexs[4]*16) + hexs[5]);
                  update3 = ((hexs[2]*16) + hexs[3]);
                  update4 = ((hexs[0]*16) + hexs[1]);
                  //Break the loop and write these bytes onto the FAT to point to the newly allocated cluster
                  allocClust=0;
                  fseek(fp,writeOffset,SEEK_SET);
                  fwrite(&update1, sizeof(int), 1, fp);
                  fseek(fp,writeOffset+1,SEEK_SET);
                  fwrite(&update2, sizeof(int), 1, fp);
                  fseek(fp,writeOffset+2,SEEK_SET);
                  fwrite(&update3, sizeof(int), 1, fp);
                  fseek(fp,writeOffset+3,SEEK_SET);
                  fwrite(&update4, sizeof(int), 1, fp);
                  //Now we must write 0x0FFFFFFF onto the newly allocated cluster
                  unsigned int FF = 255;
                  unsigned int OF = 15;
                  fseek(fp,fatOffset,SEEK_SET);
                  fwrite(&FF, sizeof(unsigned int), 1, fp);
                  fseek(fp,fatOffset+1,SEEK_SET);
                  fwrite(&FF, sizeof(unsigned int), 1, fp);
                  fseek(fp,fatOffset+2,SEEK_SET);
                  fwrite(&FF, sizeof(unsigned int), 1, fp);
                  fseek(fp,fatOffset+3,SEEK_SET);
                  fwrite(&OF, sizeof(unsigned int), 1, fp);
                  fseek(fp,fatOffset+4,SEEK_SET);
                  //set the new cluster as the one we will write to next
                  currentCluster = tempCluster;
                }
              }

            }
            //if next cluster is already allocated, move there.
            else{
              currentCluster = nextCluster;
            }
          }
          //if writing to a portion of the cluster
          else {
            // read the remaining chars into a buffer, if we run out of chars before the end of the
            // size, fill the chars as empty chars
            char buffer[charLeft];
            unsigned long int offset = (((bb.BPB_RsvdSecCnt + (bb.BPB_NumFATs * bb.BPB_FATSz32)) +
              (currentCluster - 2) * bb.BPB_SecPerClus) * bb.BPB_BytsPerSec);

            for(int j=0; j<charLeft; j++){
              if(j<=strlen(string)){
                buffer[j]=string[lastRead];
                lastRead++;
              }
              else{
                buffer[j] = '\0';
              }
            }
            buffer[lastRead]= '\0';
            //write the buffer to the last cluster
            fseek(fp, offset+opened_files[i].fileOffset, SEEK_SET);
            fwrite(buffer, charLeft, 1, fp);
            fseek(fp, offset+charLeft, SEEK_SET);
            keepWriting = 0;
            opened_files[i].fileOffset = opened_files[i].fileOffset + size;
          }
        }

        return;
      }
      else{
        printf("ERROR: Opened file is not in \"write\" mode\n");
        return;
      }
    }
  }
  printf("ERROR: Opened file does not exist\n");
  return;
}


void lSeek(char* fileName, long int offset){
  fileName = makeStrUpper(fileName);
  //Scan opened files

  for (int i=0; i < num_opened_files; i++)
  {
    //if file is opened
    if (compareStr(fileName, opened_files[i].fileEntry.DIR_Name)){
      if(opened_files[i].fileEntry.DIR_FileSize > offset){
        opened_files[i].fileOffset = offset;
        printf("\n%u\n",opened_files[i].fileOffset);
        return;
      }
      else{
        printf("\nERROR: Offset cannot be bigger than the file's size\n");
        return;
      }
    }
  }
  //if file is not opened
  printf("ERROR: File not opened\n");
  return;
}
