//
// Created by LN on 2022/1/4.
//

#include <cstdio>
#include "Resource.h"

bool Resource::checkIsLocalFile(char *url) {
    bool isFile = false;
    FILE *file = fopen(url, "r");
    if (file) {
        isFile = true;
        fclose(file);
    }
    return isFile;
}

void Resource::check() {
    isLocalFile = checkIsLocalFile(url);
}
