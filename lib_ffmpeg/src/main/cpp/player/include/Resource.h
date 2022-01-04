//
// Created by LN on 2022/1/4.
//

#ifndef FF_PLAYER_C_RESOURCE_H
#define FF_PLAYER_C_RESOURCE_H


class Resource {

public:
    char *url;
    bool isLocalFile;

    bool checkIsLocalFile(char *url);
    void check(void);
};

#endif //FF_PLAYER_C_RESOURCE_H
