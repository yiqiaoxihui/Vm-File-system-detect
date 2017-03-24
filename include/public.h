#ifndef PUBLIC_H_INCLUDED
#define PUBLIC_H_INCLUDED

pthread_t *read_image_thread;

struct ThreadVar *threadVar;
/**多线程读取镜像文件所需参数*/
struct ThreadVar{
    char *image_id;
    char *image_path;
};


#endif // PUBLIC_H_INCLUDED
