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
float overlay_file_count;
float inode_in_overlay_file_count;
#endif // PUBLIC_H_INCLUDED
