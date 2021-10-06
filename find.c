#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <locale.h>
#include <langinfo.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

//an array with the correct rwx pairings
char* rwxp[] = {
    "---",  // 0 = 000(0) 
    "--x",  // 1 = 001(1) 
    "-w-",  // 2 = 010(2) 
    "-wx",  // 3 = 011(3)
    "r--",  // 4 = 100(4) 
    "r-x",  // 5 = 101(5) 
    "rw-",  // 6 = 110(6)
    "rwx"   // 7 = 111(7) 
};

// Takes an err number and the fuction that throw the error
// then prints the error to stderr with the correct error code
void handleError(int err, char* preCursor, char* fileName){
    fprintf(stderr,"%s %s: %s\n",preCursor,fileName,strerror(err)); 
    exit(-1); 
}

// returns true if the file mode is a directory
bool isDirectory(mode_t mode){
    return ((mode&S_IFMT) == S_IFDIR); 
}

// returns true if mode is a valid 
bool isSomething(mode_t mode){
    return((mode&S_IFMT) == S_IFREG || (mode&S_IFMT) == S_IFDIR || (mode&S_IFMT) == S_IFLNK || (mode&S_IFMT) == S_IFCHR || (mode&S_IFMT) ==S_IFBLK || (mode&S_IFMT) ==S_IFIFO || (mode&S_IFMT) == S_IFSOCK); 
}

// returns true if the mode is a link
bool isLink(mode_t mode){
    return((mode&S_IFMT) == S_IFLNK); 
}

// takes a fileName and determines if it is a . or .. directory
bool isRef(char* fileName,int size){
    return(fileName[size-2] == '.' && ((fileName[size-3] == '.' && fileName[size-4] == '/') || fileName[size-3] == '/'));
}

// returns false if the file is the parent
bool isNotParent(char* fileName, int size){
    return (!(fileName[size-2] == '.' && fileName[size-3] == '.' && fileName[size-4] == '/')); 
}

// takes in the mode of a file and prints out the correct drwx format
void printMode(mode_t mode){
    if(isDirectory(mode)) printf("d");
    else if(isSomething(mode)) printf("-");
    else printf("?");
    char buf[10]; 
    strcpy(&buf[0],rwxp[(int)((mode&0700) >> 6)]);
    strcpy(&buf[3],rwxp[(int)((mode&0070) >> 3)]); 
    strcpy(&buf[6],rwxp[(int)(mode&0007)]); 
    buf[9] = '\0';
    if (mode & S_ISUID)
        buf[2] = (mode & S_IXUSR) ? 's' : 'S';
    if (mode & S_ISGID)
        buf[5] = (mode & S_IXGRP) ? 's' : 'l';
    if (mode & S_ISVTX)
        buf[8] = (mode & S_IXOTH) ? 't' : 'T';
    printf("%s",buf); 
}

// Takes in directory information and prints it out formatted correctly 
void stringPrinter(struct dirent * dp, struct stat statbuf, char* fullName){
    printf("%d ",dp->d_ino);
    printf("%4d ",statbuf.st_blocks/2);
    printMode(statbuf.st_mode);
    printf("%4d ", statbuf.st_nlink); 

    struct passwd* pwd;
    if ((pwd = getpwuid(statbuf.st_uid)) != NULL) printf("%-8s ", pwd->pw_name);
    else printf("%-8d ",statbuf.st_uid); 

    struct group* grp;
    if ((grp = getgrgid(statbuf.st_gid)) != NULL) printf("%-8s ",grp->gr_name);
    else printf("%-8d ",statbuf.st_gid);

    printf("%9d ",statbuf.st_size);

    struct tm* tm; 
    tm = localtime(&statbuf.st_mtime);
    char datestring[256]; 
    strftime(datestring,sizeof(datestring),nl_langinfo(D_T_FMT), tm); 
    printf("%s ", datestring); 

    if(isLink(statbuf.st_mode)){
        char* buf = malloc(PATH_MAX*sizeof(char));
        ssize_t len = readlink(fullName,buf,PATH_MAX);
        if(len == -1) handleError(errno,"Error readlink failed on file",fullName); 
        printf("%s -> %s\n",fullName,buf);
        free(buf); 
    } else printf("%s\n",fullName);
    
}

// Takes in a fileName and recursivly searches through all subdirectories 
// printing file information
void myReadDir(char* fileName, bool isTop){
    DIR* dirp = opendir(fileName);
    if (dirp == NULL)handleError(errno,"Error while opening directory", fileName);
    struct dirent * dp;
    int errnoC = errno; 
    while((dp = readdir(dirp)) != NULL){
        if(errnoC != errno) handleError(errno,"Error while reading file",fileName); 
        errnoC = errno; 
        struct stat statbuf;
        const unsigned int size = strlen(fileName) + strlen(dp->d_name)+1; 
        char fullName[size];
        memset(fullName,0,size*sizeof(char));
        strcat(fullName,fileName);strcat(fullName,dp->d_name);
        if(lstat(fullName, &statbuf) == -1) handleError(errno,"Error getting statistics on",fileName); 
        else if(!isRef(fullName,size) || (isTop && !isNotParent(fullName,size))){
            stringPrinter(dp,statbuf,(isTop && !isNotParent(fullName,size)) ? fileName:fullName);
            if(isDirectory(statbuf.st_mode) && !(isTop && !isNotParent(fullName,size))) {
                strcat(fullName,"/"); 
                myReadDir(fullName,false); 
            }
        }
    }
}

int main(int argc, char* argv[]){
    if (argc > 1){
        char * fileName = malloc((strlen(argv[1])+1)*sizeof(char)); 
        strcat(fileName,argv[1]);strcat(fileName,"/");
        myReadDir(fileName,true);
        free(fileName); 
    }
    else myReadDir("./",true);
    return 0; 
}