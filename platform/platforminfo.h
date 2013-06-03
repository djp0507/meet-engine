#ifndef PLATFORMINFO_H_
#define PLATFORMINFO_H_

#define STRLEN 200

//namespace android {

typedef struct PlatformInfo {
    
    char release_version[STRLEN];
    char model_name[STRLEN];
    char board_name[STRLEN];
    char chip_name[STRLEN];
    char manufacture_name[STRLEN];
    char app_path[STRLEN];
    char ppbox_lib_name[STRLEN];
    int sdk_version;

    void* ppbox;
        

    PlatformInfo() 
    {
        bzero(release_version, STRLEN);
        bzero(model_name, STRLEN);
        bzero(chip_name, STRLEN);
        bzero(manufacture_name, STRLEN);
        bzero(app_path, STRLEN);
        bzero(ppbox_lib_name, STRLEN);
        sdk_version = 0;

        ppbox = NULL;
    }

    ~PlatformInfo() 
    {
        bzero(release_version, STRLEN);
        bzero(model_name, STRLEN);
        bzero(chip_name, STRLEN);
        bzero(manufacture_name, STRLEN);
        bzero(app_path, STRLEN);
        bzero(ppbox_lib_name, STRLEN);
        sdk_version = 0;
    }

} PlatformInfo;

//}

#endif
