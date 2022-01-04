//
// Created by LN on 2022/1/4.
//

#ifndef FF_PLAYER_C_RESOURCE_H
#define FF_PLAYER_C_RESOURCE_H

enum ResourceType {
    LOCAL_FILE, RTP, OTHER
};

class Resource {

public:
    char *url;
    ResourceType type;

    void check(void);

};

#endif //FF_PLAYER_C_RESOURCE_H
