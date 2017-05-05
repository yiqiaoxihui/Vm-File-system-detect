#ifndef PUBLIC_H_INCLUDED
#define PUBLIC_H_INCLUDED



pthread_t *read_image_thread;

struct ThreadVar *threadVar;
/**多线程读取镜像文件所需参数*/
struct ThreadVar{
    char *image_id;
    char *image_path;
};

float all_file_count;
float error_file_count;
int overlay_file_count;
float inode_in_overlay_file_count;
float read_error;

int blockInOverlay_error;
int inodeInOverlay_error;
int magic_error;


char overlay_filepath[3000][256];//TODO
#endif // PUBLIC_H_INCLUDED
