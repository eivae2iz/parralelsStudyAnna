#define DEBUG

#include "archivator.h"

#include "scaner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <stddef.h>
#include <sys/mman.h>
#include "zlib.h"
#include <assert.h> 
#include <linux/ipc.h>
#include <linux/msg.h>
#include <linux/sem.h>
#include <linux/shm.h>
#include <dirent.h> 


void takeNextFileFromDir(fInfoForCompressor* f_info);
void takeNextFileCancelDir(fInfoForCompressor * f_info);
void takeNextFileFromDir(fInfoForCompressor* f_info);
void takeNextFileNewDir(char* name, fInfoForCompressor * f_info);


static controllerMainStruct* controllerMainInfo;
fileIterStruct  fileIterInfo = {.fileIsProcessing = 0,.fileNameIter=0, .dirsNumber = 0};



void scanerInit(controllerMainStruct* controllerInfo ){
    LOGMESG(LOG_INFO, "scanerInit, p =%p " ,controllerInfo  );
    controllerMainInfo = controllerInfo;
    fileIterInfo.dirsStack = (DirInterStruct*) malloc( sizeof( DirInterStruct) * MAXIMAL_STACK_OF_DIRECTORIES );
}

void scanerDestruct(){
    free(fileIterInfo.dirsStack);
}

fInfoForCompressor takeNextFile(char** names){
    LOGMESG(LOG_INFO, "takeNextFile");
    struct sembuf sops[1];
    sem_change_ptr(sops, S_TAKE_NEXT_FILE , -1, 0); //allow for only one process to be in the TAKE_NEXT_FILE function
    semop( controllerMainInfo->sem , sops, 1);
    fInfoForCompressor f_info = {.valid = -1}; // will be returned
    int fd;
    
    if ( fileIterInfo.fileIsProcessing == 1){ //if iterator is in a directory
        takeNextFileFromDir(&f_info);
        goto takeNextFile_exit_ok;
    }
    if ( names[fileIterInfo.fileNameIter] == NULL){ // if it is the end 
        goto takeNextFile_exit_error_0;
    }
    

    char*  new_name = names[fileIterInfo.fileNameIter];
    fileIterInfo.fileNameIter++;


    struct stat st;
    if(stat(new_name,&st) == 0){ // If it is a directory
        if(st.st_mode & S_IFDIR != 0)
        {
            fileIterInfo.fileIsProcessing = 1; 
            takeNextFileNewDir(new_name, &f_info );
            goto takeNextFile_exit_ok;
        }
    }
    

    
    fd = open( new_name , O_RDONLY);
    if ( fd == -1 ){
        LOGMESG(LOG_ERROR, "opening next file name");
        goto takeNextFile_exit_error_1;
    }
    size_t offset = addFileToFL(fd,new_name, 0 );
    
    fDescription * f_desc = ( fDescription *) ( controllerMainInfo->mmap_start +offset);
    
    f_info.length = f_desc->st_size;;
    f_info.fd = fd;
    f_info.firstFileChunkOffset = offset + offsetof( fDescription, firstFileChunkOffset );
    f_info.valid = 1;
    
    takeNextFile_exit_error_1:
    takeNextFile_exit_error_0:
    takeNextFile_exit_ok:
        sem_change_ptr(sops, S_TAKE_NEXT_FILE , 1,SEM_UNDO); //allow for only one process to be in the TAKE_NEXT_FILE function
        semop( controllerMainInfo->sem , sops, 1);
        return f_info;
}
void takeNextFileNewDir(char* name, fInfoForCompressor * f_info){
    LOGMESG(LOG_INFO, "takeNextFileNewDir");
    size_t FLoffset;
    if (fileIterInfo.dirsNumber + 1 < MAXIMAL_STACK_OF_DIRECTORIES){
        char* pathToDir;
        if ( fileIterInfo.dirsNumber == 0){
            LOGMESG(LOG_INFO, "fileIterInfo.dirsNumber == 0");
            FLoffset = 0;//////////////////////////////
            pathToDir = (char*) malloc( strlen(name) +1);
            memcpy(pathToDir, name, strlen(name) +1);
            LOGMESG(LOG_INFO, "Path to finded directory -> %s" , pathToDir);
            fileIterInfo.dirsStack[fileIterInfo.dirsNumber].pathToDir = pathToDir;
        }else{
            FLoffset = fileIterInfo.dirsStack[fileIterInfo.dirsNumber -1 ].offset_to_fl;
            char* pathToPrevDir = fileIterInfo.dirsStack[fileIterInfo.dirsNumber -1 ].pathToDir;
            pathToDir = (char*) malloc( strlen( pathToPrevDir) + strlen(name) +2);
            snprintf( pathToDir,strlen( pathToPrevDir) + strlen(name) +2, "%s/%s",pathToPrevDir, name );
            LOGMESG(LOG_WARN, "Path to finded directory -> %s", pathToDir);
            fileIterInfo.dirsStack[fileIterInfo.dirsNumber].pathToDir =pathToDir;
        }
        fileIterInfo.dirsStack[fileIterInfo.dirsNumber].d = opendir(pathToDir);
        if (fileIterInfo.dirsStack[fileIterInfo.dirsNumber].d == NULL){
            LOGMESG(LOG_ERROR, "Opening directory!");
        }
        size_t offset_to_fl = addDirToFL( 0 , name , FLoffset );
        fileIterInfo.dirsStack[fileIterInfo.dirsNumber].offset_to_fl = offset_to_fl;

        fileIterInfo.dirsNumber++;
        takeNextFileFromDir(f_info);
    }else{
        LOGMESG(LOG_ERROR, "MAXIMAL_STACK_OF_DIRECTORIES overflow");
        f_info->valid = -1;
    }
}

void takeNextFileFromDir(fInfoForCompressor* f_info){
    LOGMESG(LOG_INFO, "takeNextFileFromDir");
    DIR *d;
    d = fileIterInfo.dirsStack[fileIterInfo.dirsNumber-1].d;
    struct dirent *dir;
    dir = readdir(d);
    if( dir == NULL){
        takeNextFileCancelDir(f_info);
        return;
    }
    if ( dir->d_type == 4){ //////////////////////////////////////////////////////// 
        if ( strcmp(dir->d_name , ".")==0 ||  strcmp(dir->d_name , "..")==0 ){
            takeNextFileFromDir(f_info);
            return;
        }else{
            takeNextFileNewDir(dir->d_name , f_info );
            return ;
        }
    }else{// simple file
        char* name = dir->d_name;
        char * pathToDir = fileIterInfo.dirsStack[fileIterInfo.dirsNumber-1].pathToDir;
        char* pathToFile = (char*) malloc( strlen( pathToDir) + strlen(name) +2);
        snprintf( pathToFile,strlen( pathToDir) + strlen(name) +2, "%s/%s",pathToDir, name );
        int fd = open( pathToFile,O_RDONLY );
        LOGMESG(LOG_INFO, "takeNextFileFromDir addFileToFL, %s",pathToFile );
        size_t offset = addFileToFL(fd,name, fileIterInfo.dirsStack[fileIterInfo.dirsNumber-1].offset_to_fl );
        LOGMESG(LOG_WARN, "Path to finded file -> %s", pathToFile);
        fDescription * f_desc = ( fDescription *) ( controllerMainInfo->mmap_start +offset);
        f_info->length = f_desc->st_size;;
        f_info->fd = fd;
        f_info->firstFileChunkOffset = offset + offsetof( fDescription, firstFileChunkOffset );
        f_info->valid = 1;
        return;
    }
}

void takeNextFileCancelDir(fInfoForCompressor * f_info){
    LOGMESG(LOG_INFO, "takeNextFileCancelDir");
    if (fileIterInfo.dirsNumber > 0 ){
        fileIterInfo.dirsNumber--;
        char* pathToDir = fileIterInfo.dirsStack[fileIterInfo.dirsNumber].pathToDir;
        LOGMESG(LOG_WARN, "Path to canceling directory -> %s", pathToDir);
        free(pathToDir);
        closedir( fileIterInfo.dirsStack[fileIterInfo.dirsNumber].d );
    }
    if ( fileIterInfo.dirsNumber > 0){
        takeNextFileFromDir(f_info);
    }else{
        f_info->valid = 0;
        fileIterInfo.fileIsProcessing = 0;
        return;
    }
}