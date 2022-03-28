#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <taipool.h>
#include <taihen.h>
#include <psp2/ctrl.h>
#include <psp2/io/fcntl.h>
#include <psp2/io/stat.h>
#include <psp2/io/dirent.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/modulemgr.h>
#include <psp2/kernel/clib.h>
#include <psp2/power.h>
#include <psp2/appmgr.h>

#define TRANSFER_SIZE (128 * 1024)
#define SCE_ERROR_ERRNO_EEXIST 0x80010011
#define MAX_PATH_LENGTH 1024

//function from VitaShell by TheOfficialFlow
int hasEndSlash(const char *path) {
  return path[strlen(path) - 1] == '/';
}

//function from VitaShell by TheOfficialFlow
int copyFile(const char *src_path, const char *dst_path) {

  SceUID fdsrc = sceIoOpen(src_path, SCE_O_RDONLY, 0);
  if (fdsrc < 0)
    return fdsrc;

  SceUID fddst = sceIoOpen(dst_path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
  if (fddst < 0) {
    sceIoClose(fdsrc);
    return fddst;
  }

  void *buf = malloc(TRANSFER_SIZE);

  while (1) {
    int read = sceIoRead(fdsrc, buf, TRANSFER_SIZE);

    if (read < 0) {
      free(buf);

      sceIoClose(fddst);
      sceIoClose(fdsrc);

      sceIoRemove(dst_path);

      return read;
    }

    if (read == 0)
      break;

    int written = sceIoWrite(fddst, buf, read);

    if (written < 0) {
      free(buf);

      sceIoClose(fddst);
      sceIoClose(fdsrc);

      sceIoRemove(dst_path);

      return written;
    }

  }

  free(buf);

  // Inherit file stat
  SceIoStat stat;
  memset(&stat, 0, sizeof(SceIoStat));
  sceIoGetstatByFd(fdsrc, &stat);
  sceIoChstatByFd(fddst, &stat, 0x3B);

  sceIoClose(fddst);
  sceIoClose(fdsrc);

  return 1;
}

//function from VitaShell by TheOfficialFlow
int copyPath(const char *src_path, const char *dst_path) {

  SceUID dfd = sceIoDopen(src_path);
  sceClibPrintf("dfd = %x\n", dfd);
  if (dfd >= 0) {
    SceIoStat stat;
    memset(&stat, 0, sizeof(SceIoStat));
    sceIoGetstatByFd(dfd, &stat);

    stat.st_mode |= SCE_S_IWUSR;

    int ret = sceIoMkdir(dst_path, stat.st_mode & 0xFFF);
	
    if (ret < 0 && ret != SCE_ERROR_ERRNO_EEXIST) {
      sceIoDclose(dfd);
      return ret;
    }

    if (ret == SCE_ERROR_ERRNO_EEXIST) {
      sceIoChstat(dst_path, &stat, 0x3B);
    }

    int res = 0;

    do {
      SceIoDirent dir;
      memset(&dir, 0, sizeof(SceIoDirent));

      res = sceIoDread(dfd, &dir);
	  sceClibPrintf("dir.d_name = %s\n", dir.d_name);
      if (res > 0) {
		char *new_src_path = malloc(strlen(src_path) + strlen(dir.d_name) + 2);
		snprintf(new_src_path, MAX_PATH_LENGTH, "%s%s%s", src_path, hasEndSlash(src_path) ? "" : "/", dir.d_name);
		
		char *new_dst_path = malloc(strlen(dst_path) + strlen(dir.d_name) + 2);
		snprintf(new_dst_path, MAX_PATH_LENGTH, "%s%s%s", dst_path, hasEndSlash(dst_path) ? "" : "/", dir.d_name);

		int ret = 0;

		if (SCE_S_ISDIR(dir.d_stat.st_mode)) {
		  ret = copyPath(new_src_path, new_dst_path);
		} 
		else {
		  ret = copyFile(new_src_path, new_dst_path);
		}

		free(new_dst_path);
		free(new_src_path);

		if (ret <= 0) {
		  sceIoDclose(dfd);
		  return ret;
		}
      }
    } while (res > 0);

    sceIoDclose(dfd);
  }
  else {
    return copyFile(src_path, dst_path);
  }
  return 1;
}

int checkFolderExist(const char *folder) {
  SceUID dfd = sceIoDopen(folder);
  if (dfd < 0)
    return 0;

  sceIoDclose(dfd);
  return 1;
}

int removePath(const char *path){
  SceUID dfd = sceIoDopen(path);
  if (dfd >= 0) {
    int res = 0;

    do {
      SceIoDirent dir;
      memset(&dir, 0, sizeof(SceIoDirent));

      res = sceIoDread(dfd, &dir);
      if (res > 0) {
        char *new_path = malloc(strlen(path) + strlen(dir.d_name) + 2);
        snprintf(new_path, MAX_PATH_LENGTH, "%s%s%s", path, hasEndSlash(path) ? "" : "/", dir.d_name);

        if (SCE_S_ISDIR(dir.d_stat.st_mode)) {
          int ret = removePath(new_path);
          if (ret <= 0) {
            free(new_path);
            sceIoDclose(dfd);
            return ret;
          }
        } else {
          int ret = sceIoRemove(new_path);
          if (ret < 0) {
            free(new_path);
            sceIoDclose(dfd);
            return ret;
          }
        }

        free(new_path);
      }
    } while (res > 0);

    sceIoDclose(dfd);

    int ret = sceIoRmdir(path);
    if (ret < 0)
      return ret;

  } else {
    int ret = sceIoRemove(path);
    if (ret < 0)
      return ret;

  }

  return 1;
}

/*Thread function to listen for user input while playing
  a game with autosave functionality.  Thread will copy
  savedata folder when user triggers button combo*/
int checkpoint_thread(SceSize arglen, void *arg) {
    (void)arglen;
    (void)arg;
	SceCtrlData ctrl;
	SceCtrlPortInfo portinfo;
	SceUID processID;
	char title_id[10] = {0};
	char title_id_array[6][10];
	char paste_location[19] = {0};
	char copy_location[31] = {0};
	SceUID appIds[6] = {0};
	SceUID processIds[6] = {0};
	char uri[35] = {0};

	/*loop runs until vita shuts down*/
    for (;;) {
		//Check user input
		sceCtrlGetControllerPortInfo(&portinfo);
	
		if(portinfo.port[1] != SCE_CTRL_TYPE_UNPAIRED){
			sceCtrlPeekBufferPositive(1, &ctrl, 1);
		}
		else{
			sceCtrlPeekBufferPositive(0, &ctrl, 1);
		}
		
		//If specific button combo
		if ((ctrl.buttons == (SCE_CTRL_SELECT | SCE_CTRL_SQUARE | SCE_CTRL_CIRCLE))){
			
			//check which apps are running
			memset(appIds, 0, sizeof(appIds));
			sceAppMgrGetRunningAppIdListForShell(appIds, 6);
			//get process ids of all running apps
			memset(processIds, 0, sizeof(processIds));
			for (int i = 0; i < 6; i++){
				processIds[i] = sceAppMgrGetProcessIdByAppIdForShell(appIds[i]);
				if (processIds[i] < 0){
					processIds[i] = 0;
				}
			}
			//get title ids of all running apps
			memset(title_id_array, 0, sizeof(title_id_array));
			for (int i = 0; i < 6; i++){
				if (processIds[i] != 0){
					sceAppMgrGetNameById(processIds[i], title_id_array[i]);
				}
			}
			//Figure out which game app is currently running in the foreground
			for (int i = 0; i < 6; i++){
				memset(title_id, 0, sizeof(title_id));
				snprintf(title_id, 4, "%s", title_id_array[i]);
				if (strcmp(title_id, "NPXS") != 0){
					snprintf(title_id, 10, "%s", title_id_array[i]);
					break;
				}
				else{
					memset(title_id, 0, sizeof(title_id));
				}
			}
			
			//If no app is running, do nothing
			if (strlen(title_id) == 0) {
			}
			else{
				
				//fill in paste/copy_location
				memset(paste_location, 0, 19);
				memset(copy_location, 0, 31);
				snprintf(copy_location, 31, "ux0:user/00/savedata/%s", title_id);
				snprintf(paste_location, 19, "ux0:temp/%s", title_id);
				
				//check if copy_location directory exists.  If it does not, then do nothing.
				if (checkFolderExist(copy_location) == 1){
					
					//Kill game
					sceAppMgrDestroyAppByName(title_id);
					
					//delay for 1-second
					sceKernelDelayThread(1000000);
				
					//Check if folder exists.  If it does then delete it first before copying.
					if (checkFolderExist(paste_location) == 1){
						removePath(paste_location);
					}
				
					sceClibPrintf("Start Copying\n");
				
					//Call copypath
					copyPath(copy_location, paste_location);
					
					//checkpoint saved to <destination>
					sceClibPrintf("Finished Copying\n");
					
					//restart game
					sprintf(uri, "psgm:play?titleid=%s", title_id);  //sceAppMgrLaunchAppByName2 only opens game livearea.  Doesn't actually launch.
					sceAppMgrLaunchAppByUri(0x20000, uri);
					
					memset(uri, 0, sizeof(uri));
					
				}
				
			}

		}
		
		/*Add delay in between iterations for stability*/
		sceKernelDelayThread(3000);
    }
    return 0;
}

/*Code execution starts from this point. Loaded by kernel.*/
int _start() __attribute__ ((weak, alias("module_start")));
int module_start(SceSize argc, const void *argv) { 
	
	(void)argc; 
	(void)argv;
	SceUID thread_id;
	
	//init taipool.  2 MB max size for user plugin loaded with main (shell).
	taipool_init(2 * 1024 * 1024);

	/*Create thread to create checkpoints.  Highest thread priority is 64.  Lowest thread priority is 191.*/
	thread_id = sceKernelCreateThread("LightbarThread", checkpoint_thread, 191, 0x10000, 0, 0, NULL);

	/*Start the created thread*/
	sceKernelStartThread(thread_id, 0, NULL);

    return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *argv) { (void)argc; (void)argv;

    return SCE_KERNEL_STOP_SUCCESS;
}
