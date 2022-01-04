//
// Created by LN on 2022/1/4.
//

#include <cstdio>
#include <cstring>
#include "Resource.h"

bool checkIsLocalFile(char *url) {
    bool isFile = false;
    FILE *file = fopen(url, "r");
    if (file) {
        isFile = true;
        fclose(file);
    }
    return isFile;
}

void Resource::check() {
    if (checkIsLocalFile(url)) {
        type = LOCAL_FILE;
    } else if (strncmp(url, "rtp", 3) == 0 || strncmp(url, "rtsp", 3) == 0 ||
               strncmp(url, "rtmp", 3) == 0) {
        type = RTP;
    } else {
        type = OTHER;
    }
}
