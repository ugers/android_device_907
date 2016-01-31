#include <stdio.h>
#define LOG_TAG "MNG_BLT"
#include "cutils/log.h"
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>

int main() {
     char node[50] = {'\0',};
	 char node1[50] = {'\0',};
	 char node2[50] = {'\0',};

     char buf[5] = {'\0',};
	 char busnum[1] = {'\0',};
	 char devnum[1] = {'\0',};
	 char device[50] = {'\0',};
	uid_t user = 1002;
	gid_t group = 1002;

     DIR *dir = opendir("/sys/bus/usb/devices/");
     struct dirent *dent;
     if (dir != NULL) {
	 while ((dent = readdir(dir)) != NULL) {
	     memset(node, '\0', 50);
	     memset(node1, '\0', 50);
	     memset(node2, '\0', 50);
	     sprintf(node, "/sys/bus/usb/devices/%s/bDeviceClass", dent->d_name);
	     int vid_fd = open(node, O_RDONLY);
	     memset(buf, '\0', 5);
		 memset(busnum, '\0', 1);
		 memset(devnum, '\0', 1);
	     if (vid_fd > 0) {
		 read(vid_fd, buf, 2);
		 ALOGE("node = %s, vid = %s", node, buf);
				if (strcmp(buf, "e0") == 0){
		     sprintf(node1, "/sys/bus/usb/devices/%s/busnum", dent->d_name);
			 sprintf(node2, "/sys/bus/usb/devices/%s/devnum", dent->d_name);

		     int pid_fd = open(node1, O_RDONLY);
			 int pid_fd1 = open(node2, O_RDONLY);
		     if (pid_fd > 0 && pid_fd1 > 0) {
				read(pid_fd, busnum, 1);
				read(pid_fd1, devnum, 1);
				sprintf(device, "/dev/bus/usb/00%s/00%s", busnum, devnum);
			     ALOGE("Bluetooth device is %s", device);
				 int err = chown( device, user, group );
				 if (err == -1){
				 ALOGE("Cannot chown device %s", device);
				 }

			     close(pid_fd);
			     close(pid_fd1);
			     close(vid_fd);
			     break;
		     }
		 close(pid_fd);
		 close(pid_fd1);
		 }
		 close(vid_fd);
	     }
	 }
     }
     close(dir);
    return 0;
}
